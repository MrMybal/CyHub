#include "companion.hpp"

#include <algorithm>
#include <cstdlib>

namespace cyhub {
bool is_companion(const Project& project) {
    return project.id == "cymcphub" && project.repo == "MrMybal/CyMCPHub";
}

std::string companion_github_token(const Project& project) {
    if (!is_companion(project))
        return {};
    const auto* environment = std::getenv("GH_TOKEN");
    if (!environment)
        return {};
    const std::string_view token(environment);
    if (token.size() > 4096 || !std::ranges::all_of(token, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        }))
        return {};
    return std::string(token);
}

std::optional<Release> bundled_companion(const fs::path& directory) {
    if (!supports_packages())
        return {};
    const auto manifest = directory / "cymcphub-package.json";
    constexpr auto zip = "CyMCPHub-bundled.zip";
    if (!fs::exists(manifest) || !fs::is_regular_file(directory / zip))
        return {};
    const auto data = read_json(manifest, 4096);
    const auto tag = data.at("tag").get<std::string>();
    const auto sha = data.at("sha256").get<std::string>();
    const auto size = data.at("size").get<std::uint64_t>();
    if (!newer_version("", tag) || !valid_digest(sha) || size == 0 || size > max_archive_size)
        throw std::runtime_error("The bundled CyMCPHub package manifest is invalid.");
    return Release{tag, "https://github.com/MrMybal/CyMCPHub", Asset{zip, "", sha, size}};
}

Json companion_mcp_config(const fs::path& cyhub_directory) {
#ifdef _WIN32
    auto exe = cyhub_directory / "CyHubMCP.exe";
#else
    auto exe = cyhub_directory / "CyHubMCP";
#endif
    return {{"mcpServers", {{"CyHub", {{"command", cymcp::path_utf8(exe)}, {"args", Json::array()}}}}}};
}
}
