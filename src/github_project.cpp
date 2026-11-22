#include "github_project.hpp"

#include <algorithm>
#include <regex>
#include <set>

namespace cyhub {
namespace {
std::string get(std::string url, std::string_view token, std::string_view accept,
                std::size_t limit, const Cancellation& cancel) {
    cancel.check();
    cymcp::HttpRequest request;
    request.url = std::move(url);
    request.headers["Accept"] = accept;
    if (!token.empty())
        request.headers["Authorization"] = "Bearer " + std::string(token);
    request.timeout = std::chrono::seconds(8);
    request.max_response_bytes = limit;
    const auto response = cymcp::HttpClient{}.perform(request, cancel);
    if (response.status == 404)
        return {};
    if (response.status != 200)
        throw std::runtime_error("GitHub returned HTTP " + std::to_string(response.status) + ".");
    return response.body;
}

std::string string_value(const Json& data, const char* key) {
    const auto it = data.find(key);
    return it != data.end() && it->is_string() ? it->get<std::string>() : "";
}

bool safe_link(std::string_view url) {
    return url.starts_with("https://") && url.size() <= 2048 && std::ranges::none_of(url, [](unsigned char c) {
        return c <= ' ' || c == '\\' || c == '"' || c == '<' || c == '>' || c == 127;
    });
}

std::string relative_path(std::string_view path) {
    while (path.starts_with("./"))
        path.remove_prefix(2);
    if (path.empty() || path.starts_with('/') || path.find(':') != path.npos)
        return {};
    for (std::size_t offset = 0;;) {
        const auto next = path.find('/', offset);
        if (path.substr(offset, next == path.npos ? next : next - offset) == "..")
            return {};
        if (next == path.npos)
            break;
        offset = next + 1;
    }
    std::string encoded;
    constexpr auto hex = "0123456789ABCDEF";
    for (unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '/' || c == '_' || c == '.' || c == '-')
            encoded += static_cast<char>(c);
        else {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 15];
        }
    }
    return encoded;
}

constexpr auto raw = "application/vnd.github.raw+json";
}

GitHubProject github_project(const Project& project, const Cancellation& cancel) {
    GitHubProject result;
    if (project.repo.empty())
        return result;
    const auto api = "https://api.github.com/repos/" + project.repo;
    const auto token = github_project_token(project);
    try {
        const auto repo = get(api, token, "application/vnd.github+json", 128 * 1024, cancel);
        if (!repo.empty()) {
            const auto data = cymcp::parse_json(repo);
            result.description = string_value(data, "description");
            const auto homepage = string_value(data, "homepage");
            if (safe_link(homepage))
                result.showcase_url = homepage;
        }
        result.readme = get(api + "/readme", token, raw, 384 * 1024, cancel);
        const std::regex showcase(R"(https://[^\s<>"')]+/showcase(?:/[^\s<>"')]*)?)", std::regex::icase);
        std::smatch match;
        if (std::regex_search(result.readme, match, showcase) && safe_link(match.str()))
            result.showcase_url = match.str();
    } catch (const std::exception& error) {
        result.warning = cymcp::redact_error_message(error.what());
    }
    try {
        result.releases = release_history(project, cancel);
    } catch (const std::exception& error) {
        if (result.warning.empty())
            result.warning = cymcp::redact_error_message(error.what());
    }

    const std::regex image(R"img(!\[[^\]]*\]\(<?([^\s)>]+)|<img\b[^>]*\bsrc\s*=\s*["']([^"']+)["'])img",
                           std::regex::icase);
    std::set<std::string> seen;
    for (auto it = std::sregex_iterator(result.readme.begin(), result.readme.end(), image);
         it != std::sregex_iterator{} && seen.size() < 8 && result.images.size() < 5 && !cancel.cancelled(); ++it) {
        const auto path = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();
        if (!seen.insert(path).second)
            continue;
        const auto extension = fold(path.substr(0, path.find_first_of("?#")));
        if (!extension.ends_with(".png") && !extension.ends_with(".jpg") && !extension.ends_with(".jpeg") &&
            !extension.ends_with(".gif"))
            continue;
        try {
            std::string bytes;
            if (safe_link(path) && (path.starts_with("https://raw.githubusercontent.com/") ||
                path.starts_with("https://user-images.githubusercontent.com/"))) {
                bytes = get(path, {}, "image/*", 3 * 1024 * 1024, cancel);
            } else if (const auto relative = relative_path(path); !relative.empty()) {
                bytes = get(api + "/contents/" + relative, token, raw, 3 * 1024 * 1024, cancel);
            }
            if (!bytes.empty())
                result.images.push_back({path, std::move(bytes)});
        } catch (const std::exception&) {
            // An unavailable illustration does not hide the text or releases.
        }
    }
    return result;
}
}
