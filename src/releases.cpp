#include "hub.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <tuple>

namespace cyhub {
namespace {
struct Version {
    std::array<unsigned, 3> numbers{};
    std::string_view pre;
    std::string_view label;
};

bool url_token(std::string_view value) {
    return !value.empty() && value.size() <= 120 && std::ranges::all_of(value, [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '+';
    });
}

std::optional<Version> version(std::string_view tag) {
    if (!url_token(tag))
        return {};
    // GitHub project tags may have a product prefix, such as studio-v0.1.2.
    const auto first = tag.find_first_of("0123456789");
    if (first == std::string_view::npos)
        return {};
    auto text = tag.substr(first);
    Version v;
    v.label = text;
    text = text.substr(0, text.find('+'));
    for (auto& number : v.numbers) {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
        if (result.ptr == text.data() || result.ec != std::errc{})
            return {};
        text.remove_prefix(static_cast<std::size_t>(result.ptr - text.data()));
        if (&number != &v.numbers.back()) {
            if (!text.starts_with('.'))
                return {};
            text.remove_prefix(1);
        }
    }
    if (!text.empty()) {
        if (!text.starts_with('-') || text.size() == 1)
            return {};
        text.remove_prefix(1);
        v.pre = text;
    }
    return v;
}

bool numeric(std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](char c) { return c >= '0' && c <= '9'; });
}

bool prerelease_less(std::string_view a, std::string_view b) {
    if (a.empty() || b.empty())
        return !a.empty() && b.empty();
    while (true) {
        const auto left = a.substr(0, a.find('.'));
        const auto right = b.substr(0, b.find('.'));
        const auto left_numeric = numeric(left);
        const auto right_numeric = numeric(right);
        if (left_numeric != right_numeric)
            return left_numeric;
        if (left != right) {
            if (left_numeric && left.size() != right.size())
                return left.size() < right.size();
            return left < right;
        }
        if (left.size() == a.size() || right.size() == b.size())
            return left.size() == a.size() && right.size() != b.size();
        a.remove_prefix(left.size() + 1);
        b.remove_prefix(right.size() + 1);
    }
}

std::string replace_version(std::string text, std::string_view label) {
    constexpr std::string_view variable = "{version}";
    for (auto pos = text.find(variable); pos != std::string::npos; pos = text.find(variable, pos + label.size()))
        text.replace(pos, variable.size(), label);
    return text;
}

std::string encoded_tag(std::string_view tag) {
    std::string result;
    for (char c : tag) {
        if (c == '+')
            result += "%2B";
        else
            result += c;
    }
    return result;
}
}

bool newer_version(std::string_view installed, std::string_view candidate) {
    const auto a = version(installed);
    const auto b = version(candidate);
    if (!b || (!installed.empty() && !a))
        return false;
    if (!a)
        return true;
    if (a->numbers != b->numbers)
        return a->numbers < b->numbers;
    return prerelease_less(a->pre, b->pre);
}

std::string package_entry(const Package& package, std::string_view tag) {
    const auto parsed = version(tag);
    return parsed ? replace_version(package.entry, parsed->label) : "";
}

std::optional<Release> parse_release(const Project& project, const Json& data) {
    if (data.value("draft", false) || (data.value("prerelease", false) && !project.include_prerelease))
        return std::nullopt;
    const std::string tag = data.at("tag_name");
    Release release{tag, project.url() + "/releases", {}};
    if (url_token(tag))
        release.url += "/tag/" + encoded_tag(tag);
    const auto v = version(tag);
    if (!v || !project.windows_x64 || !supports_packages())
        return release;

    const auto name = replace_version(project.windows_x64->asset, v->label);
    const auto expected_url = project.url() + "/releases/download/" + encoded_tag(tag) + "/" + encoded_tag(name);
    for (const auto& item : data.at("assets")) {
        if (item.value("name", "") != name || item.value("state", "") != "uploaded")
            continue;
        const auto digest = item.contains("digest") && item["digest"].is_string()
            ? item["digest"].get<std::string>() : std::string{};
        const auto size = item.value("size", std::uint64_t{0});
        if (item.value("browser_download_url", "") == expected_url && digest.starts_with("sha256:") &&
            valid_digest(digest.substr(7)) && size > 0 && size <= max_archive_size)
            release.asset = Asset{name, expected_url, digest.substr(7), size};
        break;
    }
    return release;
}

std::optional<Release> latest_release(const Project& project, const Cancellation& cancel) {
    if (project.repo.empty())
        return std::nullopt;
    cymcp::HttpRequest request;
    request.url = "https://api.github.com/repos/" + project.repo +
        (project.include_prerelease ? "/releases?per_page=1" : "/releases/latest");
    request.headers["Accept"] = "application/vnd.github+json";
    request.timeout = std::chrono::seconds(12);
    request.max_response_bytes = 1024 * 1024;
    const auto response = cymcp::HttpClient{}.perform(request, cancel);
    if (response.status == 404)
        return std::nullopt;
    const auto quota = response.headers.find("x-ratelimit-remaining");
    if (response.status == 429 || (response.status == 403 &&
        (response.headers.contains("retry-after") || (quota != response.headers.end() && quota->second == "0"))))
        throw cymcp::Error("GitHubLimit", "GitHub limite les requêtes. Réessaye plus tard.");
    if (response.status != 200)
        throw std::runtime_error("GitHub a répondu HTTP " + std::to_string(response.status) + ".");
    const auto data = cymcp::parse_json(response.body);
    if (project.include_prerelease)
        return data.empty() ? std::nullopt : parse_release(project, data.at(0));
    return parse_release(project, data);
}

cymcp::HttpResponse download_asset(const Asset& asset, const Cancellation& cancel,
                                 const cymcp::ChunkSink& sink) {
    cymcp::HttpRequest request;
    request.url = asset.url;
    request.timeout = std::chrono::minutes(15);
    request.idle_timeout = std::chrono::seconds(25);
    request.max_response_bytes = static_cast<std::size_t>(asset.size);
    request.error_body_limit = 16 * 1024;
    for (int redirect = 0; redirect < 4; ++redirect) {
        const auto response = cymcp::HttpClient{}.perform(request, cancel, sink);
        if (response.status != 301 && response.status != 302 && response.status != 303 &&
            response.status != 307 && response.status != 308)
            return response;
        const auto next = response.headers.find("location");
        if (next == response.headers.end() ||
            (!next->second.starts_with("https://release-assets.githubusercontent.com/") &&
             !next->second.starts_with("https://objects.githubusercontent.com/")))
            throw std::runtime_error("Redirection de téléchargement refusée.");
        request.url = next->second;
    }
    throw std::runtime_error("Trop de redirections de téléchargement.");
}
}
