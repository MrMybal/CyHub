#include "dashboard.hpp"
#include "localization.hpp"

#include <SDL3/SDL.h>
#include <cyui/Core.h>
#include <algorithm>
#include <regex>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#include <stb_image.h>

namespace cyhub {
namespace {
void dim(const std::string& text) {
    cyui::ScopedColor color(ImGuiCol_Text, cyui::palette().textDim);
    ImGui::TextWrapped("%s", text.c_str());
}

void visit(const char* label, const std::string& url) {
    if (cyui::ghostButton(cyui::trLabel(label).c_str(), {-1, 0}) && !SDL_OpenURL(url.c_str()))
        cyui::notify(cyui::tr("Could not open the link.").c_str(), cyui::Tone::error);
}

std::string text_preview(const std::string& markdown) {
    static const std::regex images(R"(!\[[^\]\r\n]*\]\([^\)\r\n]*\))");
    static const std::regex links(R"(\[([^\]\r\n]+)\]\([^\)\r\n]*\))");
    static const std::regex tags(R"(<[^>\r\n]+>)");
    auto text = std::regex_replace(markdown, images, "");
    text = std::regex_replace(text, links, "$1");
    return std::regex_replace(text, tags, "");
}

void document(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    bool space = false;
    while (std::getline(lines, line)) {
        if (line.empty() || line == "\r") {
            space = true;
            continue;
        }
        if (space)
            ImGui::Spacing();
        space = false;
        const auto marker = line.find_first_not_of('#');
        if (line.starts_with('#') && marker < line.size() && line[marker] == ' ')
            cyui::sectionTitle(line.c_str() + marker + 1);
        else
            ImGui::TextWrapped("%s", line.c_str());
    }
}
}

void Dashboard::fetch_detail() {
    if (detail_fetch_.valid() && detail_fetch_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try {
            auto [id, github] = detail_fetch_.get();
            ProjectContent result{std::move(github), {}};
            result.github.readme = text_preview(result.github.readme);
            for (auto& release : result.github.releases)
                release.notes = text_preview(release.notes);
            for (const auto& image : result.github.images) {
                const auto* data = reinterpret_cast<const stbi_uc*>(image.bytes.data());
                const auto length = static_cast<int>(image.bytes.size());
                int width = 0, height = 0, channels = 0;
                if (!stbi_info_from_memory(data, length, &width, &height, &channels) ||
                    width <= 0 || height <= 0 || width > 4096 || height > 4096)
                    continue;
                const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>
                    pixels(stbi_load_from_memory(data, length, &width, &height, &channels, 4), stbi_image_free);
                if (!pixels)
                    continue;
                const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>
                    surface(SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels.get(), width * 4),
                            SDL_DestroySurface);
                if (!surface)
                    continue;
                const float scale = std::min(1.0f, 900.0f / std::max(width, height));
                const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> scaled(
                    scale < 1 ? SDL_ScaleSurface(surface.get(), std::max(1, static_cast<int>(width * scale)),
                        std::max(1, static_cast<int>(height * scale)), SDL_SCALEMODE_LINEAR) : nullptr,
                    SDL_DestroySurface);
                if (auto* texture = SDL_CreateTextureFromSurface(renderer_, scaled ? scaled.get() : surface.get())) {
                    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                    result.images.emplace_back(texture, SDL_DestroyTexture);
                }
            }
            result.github.images.clear();
            if (content_.size() >= 8)
                content_.erase(content_.begin());
            const auto& history = result.github.releases;
            auto& status = status_.at(id);
            if (!status.checked && id != "cyhub") {
                const auto* item = find_project(catalog_, id);
                // Use the existing release check for package selection and stable/preview channels.
                if (item && !checking_.load() && !inventory_.valid() && !history.empty())
                    check_projects({*item});
            }
            content_.insert_or_assign(std::move(id), std::move(result));
        } catch (const std::exception& error) {
            cyui::notify(localized_diagnostic(cymcp::redact_error_message(error.what())).c_str(), cyui::Tone::error);
        }
    }
    if (detail_fetch_.valid() || content_.contains(selected_))
        return;
    if (const auto* project = find_project(catalog_, selected_); project && !project->repo.empty()) {
        detail_fetch_ = std::async(std::launch::async, [project = *project, cancel = cancel_] {
            return std::pair{project.id, github_project(project, cancel)};
        });
    }
}

void Dashboard::project_content(const Project& project) {
    const auto it = content_.find(project.id);
    const auto empty = [&] {
        dim(cyui::tr("Loading project from GitHub…"));
    };
    if (ImGui::BeginTabItem("README")) {
        visit("Open README on GitHub", project.url() + "#readme");
        if (it == content_.end())
            empty();
        else if (it->second.github.readme.empty())
            dim(cyui::tr("No README is accessible."));
        else
            document(it->second.github.readme);
        if (it != content_.end() && !it->second.github.warning.empty())
            dim(localized_diagnostic(it->second.github.warning));
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(cyui::trLabel("Showcase").c_str())) {
        if (it == content_.end())
            empty();
        else {
            const auto& [github, images] = it->second;
            if (!github.showcase_url.empty())
                visit("Open showcase", github.showcase_url);
            for (const auto& image : images) {
                float width = 0, height = 0;
                if (SDL_GetTextureSize(image.get(), &width, &height) && width > 0) {
                    const auto size = std::min(width, ImGui::GetContentRegionAvail().x);
                    ImGui::Image(reinterpret_cast<ImTextureID>(image.get()), {size, size * height / width});
                }
            }
            if (github.showcase_url.empty() && images.empty())
                dim(cyui::tr("No showcase or screenshots are accessible."));
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(cyui::trLabel("Releases").c_str())) {
        visit("Open releases on GitHub", project.url() + "/releases");
        if (it == content_.end())
            empty();
        else if (it->second.github.releases.empty())
            dim(cyui::tr("No accessible release"));
        else {
            for (std::size_t index = 0; index < it->second.github.releases.size(); ++index) {
                const auto& release = it->second.github.releases[index];
                cyui::ScopedID id(static_cast<int>(index));
                const auto date = release.published.empty() ? "" : " · " + release.published.substr(0, 10);
                const auto title = release.tag + date;
                if (ImGui::CollapsingHeader(title.c_str(), index == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
                    visit("View release", release.url);
                    if (release.notes.empty())
                        dim(cyui::tr("No release notes."));
                    else
                        document(release.notes);
                }
            }
        }
        ImGui::EndTabItem();
    }
}
}
