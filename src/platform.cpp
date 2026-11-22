#include "hub.hpp"

#include <cstdlib>
#include <system_error>
#include <cwchar>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace cyhub {
#ifdef _WIN32
namespace {
fs::path known_folder(REFKNOWNFOLDERID id, const char* error) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &path)))
        throw std::runtime_error(error);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> folder(path, CoTaskMemFree);
    return fs::path(folder.get());
}

std::wstring environment_without_github_token() {
    const std::unique_ptr<wchar_t, decltype(&FreeEnvironmentStringsW)> original(
        GetEnvironmentStringsW(), FreeEnvironmentStringsW);
    if (!original)
        throw std::runtime_error("Could not prepare the application environment.");
    std::wstring environment;
    for (auto* value = original.get(); *value; value += std::wcslen(value) + 1) {
        const auto length = std::wcslen(value);
        if (_wcsnicmp(value, L"GH_TOKEN=", 9) != 0)
            environment.append(value, length + 1);
    }
    if (environment.empty())
        environment.push_back(L'\0');
    environment.push_back(L'\0');
    return environment;
}
}
#endif

fs::path executable_directory() {
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        throw std::runtime_error("Could not locate CyHub.");
    path.resize(length);
    return fs::path(path).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0)
        throw std::runtime_error("Could not locate CyHub.");
    return fs::canonical(path.c_str()).parent_path();
#else
    return fs::read_symlink("/proc/self/exe").parent_path();
#endif
}

std::string file_url(const fs::path& file) {
    const auto path = fs::absolute(file).generic_u8string();
    std::string url = path.starts_with(u8"//") ? "file:" :
        path.starts_with(u8"/") ? "file://" : "file:///";
    constexpr char hex[] = "0123456789ABCDEF";
    for (const auto c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~' || c == '/' || c == ':') {
            url += static_cast<char>(c);
        } else {
            url += '%';
            url += hex[c >> 4];
            url += hex[c & 0x0f];
        }
    }
    return url;
}

fs::path managed_root() {
#ifdef _WIN32
    return known_folder(FOLDERID_ProgramFiles, "Could not locate Program Files.") / "CyHub" / "Managed";
#else
    return {};
#endif
}

fs::path companion_root() {
#ifdef _WIN32
    return known_folder(FOLDERID_LocalAppData, "Could not locate the local user data directory.") /
           "CyHub" / "Companions";
#else
    return {};
#endif
}

fs::path user_projects_root() {
#ifdef _WIN32
    return companion_root().parent_path() / "Projects";
#else
    return {};
#endif
}

std::string_view app_version() { return CYHUB_VERSION; }

fs::path default_download_directory() {
#ifdef _WIN32
    return known_folder(FOLDERID_Downloads, "Could not locate Downloads.") / "CyHub";
#else
    const auto* home = std::getenv("HOME");
    return home ? fs::u8path(home) / "Downloads" / "CyHub" : fs::temp_directory_path() / "CyHub";
#endif
}

bool supports_packages() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return true;
#else
    return false;
#endif
}

std::optional<Installed> installed_version(const fs::path& root, const Project& project) {
    if (root.empty() || !project.windows_x64)
        return {};
    const auto path = root / project.id / "current.json";
    if (!fs::exists(path))
        return {};
    const auto state = read_json(path, 4096);
    Installed installed{state.at("version"), state.at("digest")};
    return valid_digest(installed.digest) && newer_version("", installed.version)
        ? std::optional{std::move(installed)} : std::nullopt;
}

Job update_job(const fs::path& root, const Project& project) {
    if (root.empty() || !project.windows_x64)
        return {};
    const auto path = root / project.id / "job.json";
    if (!fs::exists(path))
        return {};
    const auto state = read_json(path, 8192);
    return {state.at("phase"), state.value("detail", "")};
}

fs::path installed_entry(const fs::path& root, const Project& project, const Installed& installed) {
    if (!project.windows_x64 || !valid_digest(installed.digest))
        throw std::runtime_error("Unknown installation.");
    const auto name = package_entry(*project.windows_x64, installed.version);
    if (!portable_path(name))
        throw std::runtime_error("Invalid entry point.");
    return root / project.id / "versions" / installed.digest / fs::u8path(name);
}

void open_installed(const fs::path& path, bool execute, const fs::path& config) {
#ifdef _WIN32
    if (execute) {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        const auto directory = path.parent_path();
        auto command = L"\"" + path.native() + L"\"";
        auto environment = environment_without_github_token();
        if (!config.empty())
            command += L" --config \"" + config.native() + L"\"";
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE,
                            CREATE_UNICODE_ENVIRONMENT, environment.data(), directory.c_str(), &startup, &process))
            throw std::system_error(GetLastError(), std::system_category(), "Could not launch the application");
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
        return;
    }
    const auto directory = path.parent_path();
    const auto result = ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        throw std::runtime_error("Could not open the folder.");
#else
    (void)path;
    (void)execute;
    (void)config;
    throw std::runtime_error("Managed installation is available on Windows x64.");
#endif
}

#ifndef _WIN32
bool updater_running() { return false; }

void request_update(const Project&, std::string_view) {
    throw std::runtime_error("The update service is available on Windows x64.");
}
#endif
}
