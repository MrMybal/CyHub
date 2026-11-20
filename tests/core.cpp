#include "hub.hpp"

#include <iostream>
#ifdef _WIN32
#include "packages.hpp"
#include <miniz.h>
#include <fstream>
#endif

namespace {
using namespace cyhub;
void check(bool value, const char* label) {
    if (!value)
        throw std::runtime_error(label);
}

#ifdef _WIN32
void packages(const Project& project, const fs::path& scratch) {
    const auto bad = scratch / "bad.zip";
    mz_zip_archive zip{};
    check(mz_zip_writer_init_heap(&zip, 0, 0), "init fixture");
    check(mz_zip_writer_add_mem(&zip, "../outside.txt", "test", 4, 0), "add bad path");
    void* bytes = nullptr;
    size_t size = 0;
    check(mz_zip_writer_finalize_heap_archive(&zip, &bytes, &size), "finalize fixture");
    mz_zip_writer_end(&zip);
    {
        std::ofstream file(bad, std::ios::binary);
        file.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
    }
    mz_free(bytes);
    fs::create_directory(scratch / "extraction");
    bool rejected = false;
    try {
        extract_archive(bad, scratch / "extraction", {});
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected && !fs::exists(scratch / "outside.txt"), "reject unsafe ZIP path");

    const auto file = scratch / "package.exe";
    { std::ofstream fixture(file, std::ios::binary); fixture << "abc"; }
    Project portable = project;
    portable.windows_x64 = Package{"Test-{version}.exe", "test.exe"};
    const std::string digest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    Release release{"v1.2.0", "", Asset{"Test-1.2.0.exe", "", digest, 3}};
    activate_archive(scratch, portable, release, file, {});
    const auto current = installed_version(scratch, portable);
    check(current && current->version == "v1.2.0" && fs::exists(installed_entry(scratch, portable, *current)),
          "activate verified version");
    release.tag = "v1.3.0";
    release.asset->sha256 = std::string(64, 'a');
    rejected = false;
    try {
        activate_archive(scratch, portable, release, file, {});
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected && installed_version(scratch, portable)->version == "v1.2.0",
          "digest mismatch preserves active version");
}
#endif
}

int main(int argc, char** argv) {
    try {
        check(argc >= 2, "catalog path");
        const auto catalog = load_catalog(fs::u8path(argv[1]));
        const auto* ui = find_project(catalog, "cyui");
        check(ui && matches(*ui, "bibliotheques widgets"), "accent-insensitive catalog search");
        check(!matches(*ui, "workflow"), "query excludes unrelated projects");
        check(file_url(fs::u8path("CyHub é #.html")).ends_with("CyHub%20%C3%A9%20%23.html"),
              "offline documentation URL encodes spaces, Unicode and fragments");
        for (const auto& project : catalog) {
            if (!project.skill.empty()) {
                const auto skill = read_skill(fs::u8path(argv[1]).parent_path(), project);
                check(skill.starts_with("---\nname: " + project.skill + "\n"), "bundled skill frontmatter");
            }
        }
        check(newer_version("v1.2.9", "v1.10.0"), "version comparison");
        check(newer_version("v2.0.0-alpha.8", "v2.0.0-alpha.12"), "prerelease comparison");
        check(newer_version("v2.0.0-beta", "v2.0.0"), "stable supersedes prerelease");
        check(!newer_version("v2.0.0", "v2.0.0+build"), "build data is not an upgrade");
        check(!newer_version("v2.0.0", "v1.9.0"), "reject downgrade");
        check(portable_path("sub/Program.exe") && !portable_path("../test") &&
              !portable_path("C:/Program.exe") && !portable_path("lpt1.txt") &&
              !portable_path("sub/stream:payload"), "portable file path validation");

        const auto tag = "v1.2.0";
        const auto asset_name = "CyUI-Demo-1.2.0-windows-x64.zip";
        Json asset{{"name", asset_name}, {"state", "uploaded"}, {"size", 123},
                   {"digest", "sha256:" + std::string(64, 'a')},
                   {"browser_download_url", ui->url() + "/releases/download/" + tag + "/" + asset_name}};
        Json metadata{{"tag_name", tag}, {"assets", Json::array({asset})}};
        auto release = parse_release(*ui, metadata);
        check(release && release->asset.has_value() == supports_packages(), "platform package selection");
        metadata["assets"][0]["browser_download_url"] = "https://example.org/file.zip";
        check(!parse_release(*ui, metadata)->asset, "reject unmanaged distribution URL");
        metadata["assets"][0]["browser_download_url"] = asset["browser_download_url"];
        metadata["assets"][0]["digest"] = nullptr;
        check(!parse_release(*ui, metadata)->asset, "require digest");
        metadata["prerelease"] = true;
        check(!parse_release(*ui, metadata), "no GitHub prerelease by default");
        auto preview = *ui;
        preview.include_prerelease = true;
        check(parse_release(preview, metadata).has_value(), "explicit prerelease channel");

#ifdef _WIN32
        const auto scratch = fs::temp_directory_path() /
            ("cyhub-test-" + std::to_string(cymcp::Clock::now().time_since_epoch().count()));
        fs::create_directory(scratch);
        try {
            packages(*ui, scratch);
            if (argc == 3 && std::string_view(argv[2]) == "--live") {
                const auto live = latest_release(*ui);
                check(live && live->asset, "live CyUI Windows asset");
                install_release(scratch / "live", *ui, live->tag, {});
                const auto installed = installed_version(scratch / "live", *ui);
                check(installed && installed->version == live->tag &&
                      fs::is_regular_file(installed_entry(scratch / "live", *ui, *installed)),
                      "live CyUI package installation");
                std::cout << "CyUI " << installed->version << " downloaded, verified and installed.\n";
            }
            if (argc == 5 && std::string_view(argv[2]) == "--package") {
                const auto metadata = read_json(fs::u8path(argv[3]), 1024 * 1024);
                const auto official = parse_release(*ui, metadata);
                check(official && official->asset, "official CyUI Windows asset metadata");
                auto archive = fs::u8path(argv[4]);
                if (std::string_view(argv[4]) == "--download") {
                    archive = scratch / "official.zip";
                    std::ofstream output(archive, std::ios::binary);
                    const auto response = download_asset(*official->asset, {}, [&](const char* data, std::size_t size) {
                        output.write(data, static_cast<std::streamsize>(size));
                        return static_cast<bool>(output);
                    });
                    output.close();
                    check(response.status == 200 && static_cast<bool>(output), "official ZIP downloaded");
                }
                activate_archive(scratch / "official", *ui, *official, archive, {});
                const auto installed = installed_version(scratch / "official", *ui);
                check(installed && installed->version == official->tag, "official ZIP activated");
                std::cout << "Official CyUI ZIP verified and activated.\n";
            }
        } catch (...) {
            fs::remove_all(scratch);
            throw;
        }
        fs::remove_all(scratch);
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
