#include "installed_apps.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <regex>
#include <set>
#include <utility>

namespace cyhub {
namespace {
using Microsoft::WRL::ComPtr;
using Products = std::map<std::string, std::string>;

std::string product_key(std::string_view name) {
    std::string key;
    for (const auto c : fold(name))
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            key += c;
    return key;
}

struct ProductNames {
    Products names;
    Products executables;
    std::map<std::wstring, std::string> app_paths;

    void add_executable(const fs::path& file, const std::string& id) {
        if (file.extension() != L".exe" || file.native().find(L'{') != std::wstring::npos)
            return;
        executables.emplace(product_key(cymcp::path_utf8(file.stem())), id);
        app_paths.emplace(file.native(), id);
    }

    explicit ProductNames(const Catalog& catalog) {
        for (const auto& project : catalog) {
            const auto name = product_key(project.name);
            names.emplace(name, project.id);
            add_executable(fs::u8path(project.name + ".exe"), project.id);
            if (project.windows_x64)
                add_executable(fs::u8path(project.windows_x64->entry).filename(), project.id);
        }
        if (find_project(catalog, "cyrevision"))
            add_executable(L"CyRevision.Desktop.exe", "cyrevision");
    }

    static std::string identify(std::string name, const Products& index) {
        if (const auto found = index.find(product_key(name)); found != index.end())
            return found->second;
        // Installer display names and extracted release folders often carry these suffixes.
        static const std::regex qualifier(
            R"([ ._(-]+(?:x64|x86|arm64|amd64|win(?:dows)?(?:32|64)?|64[ -]?bit|32[ -]?bit|portable|alpha|beta|preview|classic)[ )]*$)",
            std::regex::icase);
        static const std::regex version_suffix(
            R"([ ._(\-]+(?:version[ ]+)?v?[0-9]+(?:\.[0-9]+){1,3}(?:-(?:alpha|beta|rc|preview)(?:[.\-]?[0-9]+)*)?[ )]*$)",
            std::regex::icase);
        bool changed = false;
        do {
            const auto size = name.size();
            name = std::regex_replace(name, qualifier, "");
            name = std::regex_replace(name, version_suffix, "");
            changed = size != name.size();
            if (const auto found = index.find(product_key(name)); found != index.end())
                return found->second;
        } while (changed);
        return {};
    }
};

std::string display_version(std::string text) {
    const auto start = text.find_first_not_of(" \t");
    if (start == std::string::npos)
        return {};
    text = text.substr(start, text.find_last_not_of(" \t") - start + 1);
    static const std::regex short_version(R"(^[vV]?[0-9]+\.[0-9]+$)");
    static const std::regex version_text(
        R"(^[vV]?[0-9]+(?:\.[0-9]+){2,3}(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$)");
    if (std::regex_match(text, short_version))
        text += ".0";
    return std::regex_match(text, version_text) && newer_version("", text) ? text : "";
}

void remember(InstallationInventory& inventory, const std::string& id, std::string version,
              InstallationSource source) {
    if (id.empty())
        return;
    const auto [entry, inserted] = inventory.try_emplace(id, DetectedInstallation{version, source});
    if (!inserted && (source < entry->second.source ||
        (source == entry->second.source && !version.empty() &&
         newer_version(entry->second.version, version))))
        entry->second = {std::move(version), source};
}

std::wstring registry_string(HKEY key, const wchar_t* name, const wchar_t* subkey = nullptr,
                             DWORD flags = 0) {
    wchar_t value[1024]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(key, subkey, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | flags, nullptr, value, &size) != ERROR_SUCCESS)
        return {};
    return value;
}

void scan_registry(InstallationInventory& inventory, const ProductNames& products, const Cancellation& cancel) {
    constexpr auto key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    for (auto hive : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        for (auto view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
            HKEY handle = nullptr;
            if (RegOpenKeyExW(hive, key, 0, KEY_READ | view, &handle) != ERROR_SUCCESS)
                continue;
            const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> root(handle, RegCloseKey);
            for (DWORD i = 0; !cancel.cancelled(); ++i) {
                wchar_t subkey[256];
                DWORD length = ARRAYSIZE(subkey);
                const auto result = RegEnumKeyExW(root.get(), i, subkey, &length, nullptr, nullptr, nullptr, nullptr);
                if (result != ERROR_SUCCESS && result != ERROR_MORE_DATA)
                    break;
                HKEY item = nullptr;
                if (result != ERROR_SUCCESS || RegOpenKeyExW(root.get(), subkey, 0, KEY_QUERY_VALUE | view, &item) != ERROR_SUCCESS)
                    continue;
                const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> app(item, RegCloseKey);
                DWORD hidden = 0;
                DWORD size = sizeof(hidden);
                RegGetValueW(item, nullptr, L"SystemComponent", RRF_RT_REG_DWORD, nullptr, &hidden, &size);
                if (hidden || !registry_string(item, L"ParentKeyName").empty())
                    continue;
                const auto id = products.identify(cymcp::path_utf8(registry_string(item, L"DisplayName")), products.names);
                if (!id.empty())
                    remember(inventory, id, display_version(cymcp::path_utf8(registry_string(item, L"DisplayVersion"))),
                             InstallationSource::registry);
            }
        }
    }
}

fs::path known_folder(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &value)))
        return {};
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(value, CoTaskMemFree);
    return path.get();
}

bool local_drive(const fs::path& path) {
    const auto root = path.root_path().native();
    return root.size() == 3 && root[1] == L':' && GetDriveTypeW(root.c_str()) == DRIVE_FIXED;
}

bool ordinary_path(const fs::path& path, bool directory) {
    if (!local_drive(path))
        return false;
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
        directory == static_cast<bool>(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::string file_version(const fs::path& path) {
    DWORD unused = 0;
    const auto size = GetFileVersionInfoSizeW(path.c_str(), &unused);
    if (size == 0 || size > 1024 * 1024)
        return {};
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        return {};
    void* value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\", &value, &length) || length < sizeof(VS_FIXEDFILEINFO))
        return {};
    const auto* info = static_cast<VS_FIXEDFILEINFO*>(value);
    if (info->dwSignature != 0xFEEF04BD || (!info->dwFileVersionMS && !info->dwFileVersionLS))
        return {};
    auto version = std::to_string(HIWORD(info->dwFileVersionMS)) + "." +
        std::to_string(LOWORD(info->dwFileVersionMS)) + "." + std::to_string(HIWORD(info->dwFileVersionLS));
    if (const auto revision = LOWORD(info->dwFileVersionLS))
        version += "." + std::to_string(revision);
    return version;
}

fs::path shortcut_target(const fs::path& path) {
    ComPtr<IShellLinkW> shortcut;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shortcut))))
        return {};
    ComPtr<IPersistFile> file;
    if (FAILED(shortcut.As(&file)) || FAILED(file->Load(path.c_str(), STGM_READ)))
        return {};
    wchar_t target[MAX_PATH]{};
    if (shortcut->GetPath(target, ARRAYSIZE(target), nullptr, SLGP_RAWPATH) != S_OK)
        return {};
    wchar_t expanded[MAX_PATH]{};
    const auto length = ExpandEnvironmentStringsW(target, expanded, ARRAYSIZE(expanded));
    return length && length <= ARRAYSIZE(expanded) ? fs::path(expanded) : fs::path{};
}

void inspect_executable(InstallationInventory& inventory, const ProductNames& products, const std::string& id,
                        const fs::path& executable, InstallationSource source) {
    const auto previous = inventory.find(id);
    if (previous != inventory.end() && previous->second.source < source)
        return;
    if (fold(cymcp::path_utf8(executable.extension())) != ".exe" ||
        products.identify(cymcp::path_utf8(executable.stem()), products.executables) != id ||
        !local_drive(executable))
        return;
    for (auto parent = executable.parent_path(); parent != parent.root_path(); parent = parent.parent_path())
        if (!ordinary_path(parent, true))
            return;
    DWORD type = 0;
    if (ordinary_path(executable, false) && GetBinaryTypeW(executable.c_str(), &type))
        remember(inventory, id, file_version(executable), source);
}

void scan_app_paths(InstallationInventory& inventory, const ProductNames& products, const Cancellation& cancel) {
    const std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";
    for (auto hive : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        for (auto view : {RRF_SUBKEY_WOW6464KEY, RRF_SUBKEY_WOW6432KEY}) {
            for (const auto& [name, id] : products.app_paths) {
                if (cancel.cancelled())
                    return;
                auto path = registry_string(hive, nullptr, (key + name).c_str(), view);
                if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
                    path = path.substr(1, path.size() - 2);
                if (!path.empty())
                    inspect_executable(inventory, products, id, path, InstallationSource::executable);
            }
        }
    }
}

enum class Scan { shortcuts, roots, product_folder };

void scan_folder(InstallationInventory& inventory, const ProductNames& products, const fs::path& root,
                 Scan scan, int depth, int& budget, const Cancellation& cancel) {
    if (!ordinary_path(root, true) || budget <= 0 || cancel.cancelled())
        return;
    std::error_code error;
    fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, error);
    for (; !error && it != fs::directory_iterator{} && budget > 0 && !cancel.cancelled(); it.increment(error)) {
        --budget;
        const auto& path = it->path();
        const auto filename = cymcp::path_utf8(path.filename());
        if (depth > 0 && it->is_directory(error)) {
            error.clear();
            if (scan != Scan::roots || !products.identify(filename, products.names).empty())
                scan_folder(inventory, products, path, scan == Scan::roots ? Scan::product_folder : scan,
                            depth - 1, budget, cancel);
            continue;
        }
        error.clear();
        const auto extension = fold(cymcp::path_utf8(path.extension()));
        const bool shortcut = extension == ".lnk";
        if ((scan == Scan::shortcuts && !shortcut) || (!shortcut && extension != ".exe"))
            continue;
        const auto stem = cymcp::path_utf8(path.stem());
        auto id = products.identify(stem, shortcut ? products.names : products.executables);
        if (id.empty() || !ordinary_path(path, false))
            continue;
        inspect_executable(inventory, products, id, shortcut ? shortcut_target(path) : path,
                           shortcut ? InstallationSource::shortcut : InstallationSource::executable);
    }
}
}

bool supports_installation_detection() { return true; }

InstallationInventory detect_installations(const Catalog& catalog, const Cancellation& cancel) {
    const ProductNames products(catalog);
    InstallationInventory inventory;
    scan_registry(inventory, products, cancel);
    scan_app_paths(inventory, products, cancel);

    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    struct Apartment {
        HRESULT result;
        ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    } apartment{com};
    if (SUCCEEDED(com) || com == RPC_E_CHANGED_MODE) {
        for (auto id : {FOLDERID_Programs, FOLDERID_CommonPrograms, FOLDERID_Desktop, FOLDERID_PublicDesktop}) {
            int budget = 2500;
            scan_folder(inventory, products, known_folder(id), Scan::shortcuts, 4, budget, cancel);
        }
    }

    const auto local = known_folder(FOLDERID_LocalAppData);
    std::set<fs::path> roots{known_folder(FOLDERID_ProgramFiles), known_folder(FOLDERID_ProgramFilesX86),
                            known_folder(FOLDERID_Desktop), known_folder(FOLDERID_PublicDesktop),
                            known_folder(FOLDERID_Downloads)};
    if (!local.empty()) {
        roots.insert(local);
        roots.insert(local / "Programs");
    }
    for (const auto& root : roots) {
        int budget = 2000;
        scan_folder(inventory, products, root, Scan::roots, 3, budget, cancel);
    }
    if (find_project(catalog, "cyhub") && ordinary_path(executable_directory() / "CyHub.exe", false))
        remember(inventory, "cyhub", CYHUB_VERSION, InstallationSource::executable);
    return inventory;
}
}
#else
namespace cyhub {
bool supports_installation_detection() { return false; }
InstallationInventory detect_installations(const Catalog&, const Cancellation&) { return {}; }
}
#endif
