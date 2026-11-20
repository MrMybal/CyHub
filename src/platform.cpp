#include "hub.hpp"

#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace cyhub {
fs::path executable_directory() {
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        throw std::runtime_error("Chemin de CyHub introuvable.");
    path.resize(length);
    return fs::path(path).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0)
        throw std::runtime_error("Chemin de CyHub introuvable.");
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
    PWSTR path = nullptr;
    const auto result = SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &path);
    if (FAILED(result))
        throw std::runtime_error("Le dossier Program Files est introuvable.");
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> folder(path, CoTaskMemFree);
    return fs::path(folder.get()) / "CyHub" / "Managed";
#else
    return {};
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
        throw std::runtime_error("Installation inconnue.");
    const auto name = package_entry(*project.windows_x64, installed.version);
    if (!portable_path(name))
        throw std::runtime_error("Point d’entrée invalide.");
    return root / project.id / "versions" / installed.digest / fs::u8path(name);
}

void open_installed(const fs::path& path, bool execute) {
#ifdef _WIN32
    if (execute) {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        const auto directory = path.parent_path();
        auto command = L"\"" + path.native() + L"\"";
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, directory.c_str(), &startup, &process))
            throw std::system_error(GetLastError(), std::system_category(), "Démarrage impossible");
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
        return;
    }
    const auto directory = path.parent_path();
    const auto result = ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        throw std::runtime_error("Impossible d’ouvrir le dossier.");
#else
    (void)path;
    (void)execute;
    throw std::runtime_error("L’installation gérée est prévue pour Windows x64.");
#endif
}

#ifndef _WIN32
bool updater_running() { return false; }

void request_update(const Project&, std::string_view) {
    throw std::runtime_error("Le service de mise à jour est disponible sous Windows x64.");
}
#endif
}
