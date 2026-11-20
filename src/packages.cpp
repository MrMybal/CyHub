#include "packages.hpp"
#include "companion.hpp"

#include <miniz.h>
#include <picosha2.h>
#include <windows.h>
#include <array>
#include <fstream>
#include <iterator>
#include <system_error>

namespace cyhub {
namespace {
struct Scratch {
    fs::path path;
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

struct CompanionLock {
    HANDLE handle;
    explicit CompanionLock(const fs::path& project_root) {
        fs::create_directories(project_root);
        handle = CreateFileW((project_root / "install.lock").c_str(), GENERIC_WRITE, 0, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_SHARING_VIOLATION)
                throw std::runtime_error("An installation is already in progress.");
            throw std::runtime_error("Could not access the CyMCPHub installation directory.");
        }
    }
    ~CompanionLock() { CloseHandle(handle); }
};

void write_state(const fs::path& path, const Json& json) {
    Scratch pending{path.native() + L".tmp"};
    {
        std::ofstream stream(pending.path, std::ios::binary | std::ios::trunc);
        stream << json.dump(-1, ' ', false, Json::error_handler_t::replace);
        stream.close();
        if (!stream)
            throw std::runtime_error("Could not write the update state.");
    }
    for (int retries = 0; !MoveFileExW(pending.path.c_str(), path.c_str(),
             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH); ++retries) {
        const auto error = GetLastError();
        if (retries >= 12 || (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION))
            throw std::system_error(error, std::system_category(), "Publishing the update state");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

struct ZipReader {
    mz_zip_archive zip{};
    std::ifstream stream;

    ~ZipReader() {
        if (zip.m_zip_mode == MZ_ZIP_MODE_READING)
            mz_zip_reader_end(&zip);
    }

    static size_t read(void* opaque, mz_uint64 offset, void* buffer, size_t size) {
        auto& stream = static_cast<ZipReader*>(opaque)->stream;
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(offset));
        stream.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return static_cast<size_t>(stream.gcount());
    }
};

struct ExtractFile {
    std::ofstream stream;
    const Cancellation& cancel;

    static size_t write(void* opaque, mz_uint64, const void* data, size_t size) {
        auto& file = *static_cast<ExtractFile*>(opaque);
        if (file.cancel.cancelled())
            return 0;
        file.stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        return file.stream ? size : 0;
    }
};
}

void set_job(const fs::path& root, const Project& project, std::string_view phase, std::string_view detail) {
    fs::create_directories(root / project.id);
    write_state(root / project.id / "job.json", {{"phase", phase}, {"detail", detail}});
}

void extract_archive(const fs::path& archive, const fs::path& directory, const Cancellation& cancel) {
    ZipReader reader;
    reader.stream.open(archive, std::ios::binary);
    reader.zip.m_pRead = &ZipReader::read;
    reader.zip.m_pIO_opaque = &reader;
    if (!reader.stream || !mz_zip_reader_init(&reader.zip, fs::file_size(archive), 0))
        throw std::runtime_error("The ZIP file is unreadable.");

    const auto count = mz_zip_reader_get_num_files(&reader.zip);
    if (count == 0 || count > 30000)
        throw std::runtime_error("The ZIP contains an unsupported number of entries.");

    constexpr std::uint64_t total_limit = 2ULL * 1024 * 1024 * 1024;
    std::uint64_t total = 0;
    for (mz_uint i = 0; i < count; ++i) {
        cancel.check();
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&reader.zip, i, &stat) || stat.m_uncomp_size > total_limit - total)
            throw std::runtime_error("The ZIP exceeds the unpacked size limit.");
        total += stat.m_uncomp_size;

        std::array<char, 512> name{};
        const auto name_size = mz_zip_reader_get_filename(&reader.zip, i, name.data(), static_cast<mz_uint>(name.size()));
        if (name_size < 2 || name_size > name.size() || std::char_traits<char>::length(name.data()) != name_size - 1)
            throw std::runtime_error("Invalid ZIP entry name.");
        std::string_view relative(name.data(), name_size - 1);
        if (stat.m_is_directory && relative.ends_with('/'))
            relative.remove_suffix(1);
        const auto unix_type = (stat.m_external_attr >> 16) & 0170000;
        const bool regular = unix_type == 0 || unix_type == (stat.m_is_directory ? 0040000 : 0100000);
        if (!portable_path(relative) || !regular || (stat.m_external_attr & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("The ZIP contains an unsupported entry.");

        const auto destination = directory / fs::u8path(relative);
        if (stat.m_is_directory) {
            fs::create_directories(destination);
            continue;
        }
        fs::create_directories(destination.parent_path());
        if (fs::exists(destination))
            throw std::runtime_error("The ZIP contains duplicate file names.");
        ExtractFile file{std::ofstream(destination, std::ios::binary | std::ios::trunc), cancel};
        if (!file.stream || !mz_zip_reader_extract_to_callback(&reader.zip, i, &ExtractFile::write, &file, 0))
            throw std::runtime_error("Could not extract a file from the ZIP.");
        file.stream.close();
        if (!file.stream)
            throw std::runtime_error("Could not save a file from the ZIP.");
    }
}

void activate_archive(const fs::path& root, const Project& project, const Release& release,
                      const fs::path& archive, const Cancellation& cancel) {
    if (!release.asset || !project.windows_x64 || !valid_digest(release.asset->sha256))
        throw std::runtime_error("The Windows package cannot be verified.");
    const auto& asset = *release.asset;
    if (!asset.size || asset.size > max_archive_size || fs::file_size(archive) != asset.size)
        throw std::runtime_error("The package size differs from the GitHub release.");

    std::ifstream stream(archive, std::ios::binary);
    picosha2::hash256_one_by_one digest;
    std::array<char, 64 * 1024> buffer{};
    while (stream.read(buffer.data(), buffer.size()) || stream.gcount()) {
        cancel.check();
        digest.process(buffer.begin(), buffer.begin() + stream.gcount());
    }
    digest.finish();
    if (stream.bad() || !stream.eof() || picosha2::get_hash_hex_string(digest) != asset.sha256)
        throw std::runtime_error("The SHA-256 checksum does not match the GitHub release.");
    stream.close();

    const auto project_root = root / project.id;
    const auto final = project_root / "versions" / asset.sha256;
    const auto name = package_entry(*project.windows_x64, release.tag);
    if (!portable_path(name))
        throw std::runtime_error("Invalid package entry point.");
    const auto entry = fs::u8path(name);
    const auto complete = [&](const fs::path& location) {
        return fs::is_regular_file(location / entry) &&
            (!is_companion(project) || fs::is_regular_file(location / "CyMCPHubMcp.exe"));
    };

    if (!fs::exists(final)) {
        Scratch staging{project_root / "pending"};
        fs::remove_all(staging.path);
        fs::create_directories(staging.path);
        if (asset.name.ends_with(".zip"))
            extract_archive(archive, staging.path, cancel);
        else
            fs::copy_file(archive, staging.path / entry);
        if (!complete(staging.path))
            throw std::runtime_error("The main file is missing from the package.");
        cancel.check();
        fs::create_directories(final.parent_path());
        fs::rename(staging.path, final);
    }
    if (!complete(final))
        throw std::runtime_error("The version directory is incomplete.");
    cancel.check();
    write_state(project_root / "current.json", {{"version", release.tag}, {"digest", asset.sha256}});
}

static void install_package(const fs::path& root, const fs::path& directory, const Project& project,
                            const Release& release, const Cancellation& cancel, std::string_view token) {
    if (!release.asset)
        throw std::runtime_error("The requested package changed or is unavailable.");
    const auto current = installed_version(root, project);
    if (current && current->version == release.tag)
        return;
    if (current && !newer_version(current->version, release.tag))
        throw std::runtime_error("This version is older than the active installation.");

    if (!directory.empty()) {
        set_job(root, project, "verifying");
        activate_archive(root, project, release, directory / "CyMCPHub-bundled.zip", cancel);
        return;
    }
    set_job(root, project, "downloading");
    Scratch archive{root / project.id / "pending.package"};
    std::ofstream output(archive.path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Could not create the download file.");
    const auto response = download_asset(*release.asset, cancel, [&](const char* bytes, std::size_t count) {
        output.write(bytes, static_cast<std::streamsize>(count));
        return static_cast<bool>(output);
    }, token);
    output.close();
    if (response.status != 200 || !output)
        throw std::runtime_error("Download incomplete or refused (HTTP " + std::to_string(response.status) + ").");
    cancel.check();
    set_job(root, project, "verifying");
    activate_archive(root, project, release, archive.path, cancel);
}

void install_release(const fs::path& root, const Project& project, std::string_view tag, const Cancellation& cancel) {
    set_job(root, project, "checking");
    const auto release = latest_release(project, cancel);
    if (!release || release->tag != tag)
        throw std::runtime_error("The requested package changed or is unavailable.");
    install_package(root, {}, project, *release, cancel, {});
}

void install_companion(const fs::path& root, const fs::path& directory, const Project& project,
                       const Release& release, const Cancellation& cancel) {
    if (!is_companion(project) || root.empty() || !release.asset)
        throw std::runtime_error("The requested package changed or is unavailable.");
    const CompanionLock lock(root / project.id);
    try {
        const bool bundled = release.asset->url.empty();
        install_package(root, bundled ? directory : fs::path{}, project, release, cancel,
                        bundled ? std::string{} : companion_github_token(project));
        set_job(root, project, "done");
    } catch (const std::exception& error) {
        try {
            set_job(root, project, "error", cymcp::redact_error_message(error.what()));
        } catch (const std::exception&) {
            // Preserve the download or installation error if the state can't be written.
        }
        throw;
    }
}

bool open_companion(const fs::path& root, const fs::path& directory,
                    const Project& project, const Installed& installed) {
    const auto running = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\CyMCPHub.SingleInstance");
    if (running)
        CloseHandle(running);
    const auto config = root / project.id / "cyhub-mcp.json";
    const CompanionLock lock(config.parent_path());
    write_state(config, companion_mcp_config(directory));
    open_installed(installed_entry(root, project, installed), true, config);
    return running != nullptr;
}
}
