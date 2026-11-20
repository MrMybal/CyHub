#pragma once

#include <cymcp/http.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cyhub {
namespace fs = std::filesystem;
using cymcp::Json;
using cymcp::Cancellation;

struct Package {
    std::string asset;
    std::string entry;
};

struct Project {
    std::string id, name, category, description, description_en, repo;
    std::optional<Package> windows_x64;
    std::string skill;
    bool include_prerelease = false;
    std::string url() const;
    const std::string& description_for(std::string_view language) const;
};

using Catalog = std::vector<Project>;

struct Asset {
    std::string name, url, sha256;
    std::uint64_t size = 0;
    std::string api_url;
};
struct Release {
    std::string tag, url;
    std::optional<Asset> asset;
};

struct Installed {
    std::string version, digest;
};
struct Job {
    std::string phase, detail;
};
inline constexpr std::uint64_t max_archive_size = 512ULL * 1024 * 1024;

Catalog load_catalog(const fs::path& file);
Json read_json(const fs::path& file, std::size_t limit);
std::string read_skill(const fs::path& directory, const Project& project);
const Project* find_project(const Catalog& catalog, std::string_view id);
bool valid_id(std::string_view value);
bool valid_digest(std::string_view value);
bool portable_path(std::string_view value);
std::string fold(std::string_view value);
bool matches(const Project& project, std::string_view query);
std::string_view category_for(std::string_view category, std::string_view language);
std::string load_language(const fs::path& file);
void save_language(const fs::path& file, std::string_view language);
bool newer_version(std::string_view installed, std::string_view candidate);
std::string package_entry(const Package& package, std::string_view tag);
std::optional<Release> parse_release(const Project& project, const Json& data);
std::optional<Release> latest_release(const Project& project, const Cancellation& cancel = {},
                                      std::string_view github_token = {});
std::string github_project_token(const Project& project);
std::optional<Release> discover_release(const Project& project, const Cancellation& cancel = {});
cymcp::HttpResponse download_asset(const Asset& asset, const Cancellation& cancel,
                                 const cymcp::ChunkSink& sink, std::string_view github_token = {});

fs::path executable_directory();
std::string file_url(const fs::path& file);
fs::path managed_root();
fs::path companion_root();
bool supports_packages();
std::optional<Installed> installed_version(const fs::path& root, const Project& project);
Job update_job(const fs::path& root, const Project& project);
fs::path installed_entry(const fs::path& root, const Project& project, const Installed& installed);
bool updater_running();
void request_update(const Project& project, std::string_view tag);
void open_installed(const fs::path& path, bool execute, const fs::path& config = {});
}
