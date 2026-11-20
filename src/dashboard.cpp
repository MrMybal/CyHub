#include "dashboard.hpp"
#include "localization.hpp"

#include <SDL3/SDL.h>
#include <cyui/Core.h>
#include <algorithm>
#include <string_view>

namespace cyhub {
namespace {
ImVec2 dp(float width, float height) { return {cyui::dp(width), cyui::dp(height)}; }

bool active_job(const Job& job) {
    return job.phase == "queued" || job.phase == "checking" ||
           job.phase == "downloading" || job.phase == "verifying";
}

const char* category_icon(std::string_view category) {
    if (category == "Bibliothèques et SDK")
        return cyui::icon::code;
    if (category == "Unreal et VR")
        return cyui::icon::globe;
    if (category == "IA et automatisation")
        return cyui::icon::settings;
    if (category == "Création et médias")
        return cyui::icon::play;
    return cyui::icon::grid;
}

const char* fallback_icon(const Project& project) {
    if (project.id == "cyhub")
        return cyui::icon::home;
    if (project.id == "cysynck" || project.id == "syrnck")
        return cyui::icon::sync;
    if (project.id == "cymcphub" || project.id == "cymcpsdk" || project.id == "cyuemcpsdk")
        return cyui::icon::connect;
    if (project.id == "cytools_subtitle")
        return cyui::icon::document;
    return category_icon(project.category);
}

void open_url(const std::string& url) {
    if (!SDL_OpenURL(url.c_str()))
        cyui::notify(cyui::tr("Could not open the link.").c_str(), cyui::Tone::error);
}

void subdued_wrapped(const std::string& text) {
    cyui::ScopedColor color(ImGuiCol_Text, cyui::palette().textDim);
    ImGui::TextWrapped("%s", text.c_str());
}
}

Dashboard::Dashboard(SDL_Renderer* renderer, fs::path preferences)
    : catalog_(load_catalog(executable_directory() / "catalog.json")), root_(managed_root()),
      preferences_(std::move(preferences)) {
    const auto icons = executable_directory() / "assets" / "icons";
    for (const auto& project : catalog_) {
        status_.try_emplace(project.id);
        if (std::ranges::find(categories_, project.category) == categories_.end())
            categories_.push_back(project.category);
        const auto path = icons / (project.id + ".png");
        if (auto* icon = SDL_LoadPNG(cymcp::path_utf8(path).c_str())) {
            std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image(icon, SDL_DestroySurface);
            if (auto* texture = SDL_CreateTextureFromSurface(renderer, icon)) {
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                icons_.emplace(project.id, std::unique_ptr<SDL_Texture, void (*)(SDL_Texture*)>(texture, SDL_DestroyTexture));
            }
        }
    }
    selected_ = catalog_.front().id;
    refresh_state();
}

Dashboard::~Dashboard() {
    cancel_.cancel();
}

void Dashboard::refresh_state() {
    service_running_ = updater_running();
    for (const auto& project : catalog_) {
        auto& status = status_.at(project.id);
        try {
            status.installed = installed_version(root_, project);
            status.job = update_job(root_, project);
        } catch (const std::exception& error) {
            status.job = {"error", cymcp::redact_error_message(error.what())};
        }
    }
    next_state_check_ = cymcp::Clock::now() + std::chrono::milliseconds(900);
}

void Dashboard::check_projects(std::vector<Project> projects) {
    if (checking_.exchange(true))
        return;
    worker_ = std::jthread([this, projects = std::move(projects)] {
        for (const auto& project : projects) {
            if (cancel_.cancelled())
                break;
            if (project.repo.empty())
                continue;
            Check result{project.id, {}, {}};
            bool abort = false;
            try {
                result.release = latest_release(project, cancel_);
            } catch (const cymcp::Error& error) {
                result.error = cymcp::redact_error_message(error.what());
                abort = error.code == "GitHubLimit" || error.code == "NetworkError";
            } catch (const std::exception& error) {
                result.error = cymcp::redact_error_message(error.what());
            }
            {
                std::lock_guard lock(mutex_);
                results_.push_back(std::move(result));
            }
            if (abort)
                break;
        }
        checking_.store(false);
    });
}

void Dashboard::collect_checks() {
    std::deque<Check> results;
    {
        std::lock_guard lock(mutex_);
        results.swap(results_);
    }
    for (auto& result : results) {
        auto& status = status_.at(result.id);
        if (result.error.empty()) {
            status.release = std::move(result.release);
            status.checked = true;
        } else if (state_error_ != result.error) {
            state_error_ = result.error;
            cyui::notify(localized_diagnostic(state_error_).c_str(), cyui::Tone::error);
        }
        status.error = std::move(result.error);
    }
}

bool Dashboard::ready(const Status& status) const {
    return status.release && status.installed &&
        newer_version(status.installed->version, status.release->tag);
}

bool Dashboard::visible(const Project& project) const {
    return (category_.empty() || project.category == category_) &&
        (!updates_only_ || ready(status_.at(project.id))) &&
        (!skills_only_ || !project.skill.empty()) && matches(project, search_.data());
}

void Dashboard::sidebar() {
    const auto& palette = cyui::palette();
    cyui::ScopedColor background(ImGuiCol_ChildBg, palette.bg0);
    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(14, 20));
    if (ImGui::BeginChild("Navigation", {cyui::dp(226), 0}, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        const auto p = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(p, {p.x + cyui::dp(37), p.y + cyui::dp(37)}, palette.accent, cyui::dp(9));
        cyui::centredText(draw, p, {p.x + cyui::dp(37), p.y + cyui::dp(37)}, palette.accentText, "CY");
        ImGui::Dummy(dp(37, 37));
        ImGui::SameLine(0, cyui::dp(10));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + cyui::dp(5));
        cyui::heading("CyHub", 1);

        ImGui::Dummy(dp(0, 24));
        cyui::sidebarGroup(cyui::tr("Catalog").c_str());
        const auto count = std::to_string(catalog_.size());
        if (cyui::sidebarItem(cyui::icon::grid, cyui::trLabel("All projects").c_str(),
                category_.empty() && !updates_only_ && !skills_only_, count.c_str())) {
            category_.clear();
            updates_only_ = false;
            skills_only_ = false;
        }
        const auto updates = std::ranges::count_if(status_, [this](const auto& pair) { return ready(pair.second); });
        const auto available = std::to_string(updates);
        if (cyui::sidebarItem(cyui::icon::download, cyui::trLabel("Updates").c_str(), updates_only_, available.c_str())) {
            category_.clear();
            updates_only_ = true;
            skills_only_ = false;
        }
        const auto skills = std::to_string(std::ranges::count_if(catalog_, [](const auto& project) {
            return !project.skill.empty();
        }));
        if (cyui::sidebarItem(cyui::icon::code, cyui::trLabel("MCP Skills").c_str(), skills_only_, skills.c_str())) {
            category_.clear();
            updates_only_ = false;
            skills_only_ = true;
        }
        ImGui::Dummy(dp(0, 15));
        cyui::sidebarGroup(cyui::tr("By category").c_str());
        for (const auto& category : categories_) {
            const auto label = std::string(category_for(category, cyui::locale())) + "###" + category;
            if (cyui::sidebarItem(category_icon(category), label.c_str(), category_ == category)) {
                category_ = category;
                updates_only_ = false;
                skills_only_ = false;
            }
        }

        const auto lower = ImGui::GetWindowHeight() - cyui::dp(184);
        if (ImGui::GetCursorPosY() < lower)
            ImGui::SetCursorPosY(lower);
        ImGui::Separator();
        language_picker();
        if (cyui::sidebarItem(cyui::icon::help, cyui::trLabel("Help").c_str(), false, "F1"))
            open_manual();
        ImGui::Dummy(dp(0, 5));
        cyui::textColoured(service_running_ ? palette.success : palette.textDim, "%s",
            cyui::tr(service_running_ ? "●  Service running" : "○  Service inactive").c_str());
        cyui::textFaint("%s", supports_packages() ? "Windows x64 · GitHub" : cyui::tr("Catalog · GitHub").c_str());
        if (!service_running_)
            cyui::tooltip(cyui::tr(supports_packages() ? "The catalog is available in portable mode."
                                                     : "Managed installation is available on Windows x64.").c_str());
    }
    ImGui::EndChild();
}

void Dashboard::language_picker() {
    const auto current = std::ranges::find(languages, std::string_view(cyui::locale()),
        [](const Language& language) { return std::string_view(language.code); });
    cyui::sectionTitle(cyui::tr("Language").c_str());
    ImGui::SetNextItemWidth(-1);
    if (!ImGui::BeginCombo("##language", (current == std::end(languages) ? languages[0] : *current).name))
        return;
    for (const auto& language : languages) {
        const bool selected = cyui::locale() == language.code;
        if (ImGui::Selectable(language.name, selected) && !selected) {
            cyui::setLocale(language.code);
            try {
                save_language(preferences_, language.code);
            } catch (const std::exception&) {
                cyui::notify(cyui::tr("Could not save your language preference.").c_str(), cyui::Tone::warning);
            }
        }
        if (selected)
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

void Dashboard::hero() {
    const auto& palette = cyui::palette();
    const auto origin = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
    const auto height = cyui::dp(127);
    const ImVec2 end{origin.x + size.x, origin.y + height};
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, end, palette.bg2, cyui::dp(14));
    draw->AddRect(origin, end, palette.lineStrong, cyui::dp(14));
    draw->PushClipRect({origin.x + cyui::dp(15), origin.y + cyui::dp(7)}, {end.x - cyui::dp(10), end.y - cyui::dp(7)}, true);
    for (int i = 0; i < 8; ++i) {
        const auto x = end.x - cyui::dp(142.0f + i * 19.0f);
        draw->AddLine({x, end.y}, {x + cyui::dp(95), origin.y}, cyui::rgb(0x78e5dc, 0.09f), cyui::dp(1));
    }
    draw->PopClipRect();

    const auto start = ImGui::GetCursorPos();
    ImGui::SetCursorPos({start.x + cyui::dp(22), start.y + cyui::dp(20)});
    {
        cyui::ScopedFont big(cyui::boldFont(), 1.95f);
        ImGui::TextUnformatted(cyui::tr(skills_only_ ? "MCP Skills" : "Your collection").c_str());
    }
    ImGui::SetCursorPosX(start.x + cyui::dp(22));
    cyui::textDim("%s", cyui::tr(skills_only_ ? "Connection guides for AI agents."
                                            : "Applications, libraries and extensions.").c_str());
    const bool checking = checking_.load();
    ImGui::SetCursorPos({start.x + size.x - cyui::dp(167), start.y + cyui::dp(22)});
    {
        cyui::ScopedDisabled disabled(checking);
        if (cyui::primaryButton(cyui::trLabel(checking ? "Checking…" : "Check all").c_str(), dp(145, 35))) {
            state_error_.clear();
            check_projects(catalog_);
        }
    }
    ImGui::SetCursorPos({start.x, start.y + height});
    ImGui::Dummy(dp(0, 10));
}

void Dashboard::project_list() {
    const auto& palette = cyui::palette();
    std::string_view section;
    bool any = false;
    for (const auto& project : catalog_) {
        if (!visible(project))
            continue;
        if (section != project.category) {
            if (any)
                ImGui::Dummy(dp(0, 8));
            section = project.category;
            cyui::sectionTitle(std::string(category_for(project.category, cyui::locale())).c_str());
        }
        any = true;
        cyui::ScopedID id(project.id.c_str());
        const auto& status = status_.at(project.id);
        const auto selected = selected_ == project.id;
        const auto available = ImGui::GetContentRegionAvail().x;
        const auto origin = ImGui::GetCursorScreenPos();
        const auto height = cyui::dp(55);
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 end{origin.x + available, origin.y + height};
        draw->AddRectFilled(origin, end, selected ? palette.bg3 : palette.bg2, cyui::dp(8));
        draw->AddRect(origin, end, selected ? palette.accentDim : palette.line, cyui::dp(8));
        {
            cyui::ScopedColor select(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
            select.push(ImGuiCol_HeaderActive, palette.selection);
            select.push(ImGuiCol_HeaderHovered, cyui::rgb(0x78e5dc, 0.09f));
            if (ImGui::Selectable("##project", selected, 0, {available, height}))
                selected_ = project.id;
        }
        const ImVec2 icon_min{origin.x + cyui::dp(10), origin.y + cyui::dp(10)};
        const ImVec2 icon_max{icon_min.x + cyui::dp(34), icon_min.y + cyui::dp(34)};
        draw->AddRectFilled(icon_min, icon_max, palette.bg0, cyui::dp(6));
        if (const auto icon = icons_.find(project.id); icon != icons_.end()) {
            const auto inset = cyui::dp(4);
            draw->AddImage(reinterpret_cast<ImTextureID>(icon->second.get()),
                           {icon_min.x + inset, icon_min.y + inset}, {icon_max.x - inset, icon_max.y - inset});
        } else {
            cyui::centredText(draw, icon_min, icon_max, selected ? palette.accent : palette.textDim,
                              fallback_icon(project));
        }
        const auto is_update = ready(status);
        const auto tag = is_update ? status.release->tag : status.installed ? status.installed->version : "";
        float right = cyui::dp(13);
        if (!tag.empty()) {
            right += ImGui::CalcTextSize(tag.c_str()).x + cyui::dp(14);
            draw->AddText({end.x - right + cyui::dp(14), origin.y + cyui::dp(13)},
                          is_update ? palette.accent : palette.textFaint, tag.c_str());
        }
        const float x = origin.x + cyui::dp(55);
        {
            cyui::ScopedFont bold(cyui::boldFont());
            cyui::clippedText(draw, {x, origin.y + cyui::dp(7)}, available - cyui::dp(55) - right,
                               palette.text, project.name.c_str());
        }
        {
            cyui::ScopedFont small(cyui::uiFont(), 0.86f);
            cyui::clippedText(draw, {x, origin.y + cyui::dp(30)}, available - cyui::dp(69),
                               palette.textDim, project.description_for(cyui::locale()).c_str());
        }
    }
    if (!any)
        cyui::textDim("%s", cyui::tr(updates_only_ ? "No updates found." : "No projects match your search.").c_str());
}

void Dashboard::detail() {
    const auto* project = find_project(catalog_, selected_);
    if (!project || !visible(*project)) {
        cyui::textDim("%s", cyui::tr("Select a project to see its versions.").c_str());
        return;
    }
    auto& status = status_.at(project->id);
    cyui::overline(cyui::tr("PROJECT").c_str());
    if (const auto icon = icons_.find(project->id); icon != icons_.end()) {
        ImGui::Image(reinterpret_cast<ImTextureID>(icon->second.get()), dp(30, 30));
        ImGui::SameLine(0, cyui::dp(8));
    }
    cyui::heading(project->name.c_str(), 1);
    subdued_wrapped(project->description_for(cyui::locale()));
    ImGui::Dummy(dp(0, 7));

    const auto is_update = ready(status);
    if (is_update)
        cyui::badge(cyui::tr("Update available").c_str(), cyui::Tone::success);
    else if (status.installed)
        cyui::badge(cyui::tr("Installed by CyHub").c_str(), cyui::Tone::info);
    else if (status.release && status.release->asset)
        cyui::badge(cyui::tr("Package available").c_str(), cyui::Tone::info);
    ImGui::Dummy(dp(0, 12));
    ImGui::Separator();
    cyui::sectionTitle(cyui::tr("Versions").c_str());
    cyui::textDim("%s  %s", cyui::tr("Installed").c_str(), status.installed ? status.installed->version.c_str() : "—");
    const auto latest = status.release ? status.release->tag :
        cyui::tr(status.checked ? "No stable release" : "Not checked");
    cyui::textDim("GitHub     %s", latest.c_str());
    if (!status.error.empty())
        subdued_wrapped(localized_diagnostic(status.error));

    ImGui::Dummy(dp(0, 9));
    const auto can_install = status.release && status.release->asset && (!status.installed || is_update);
    const auto installing = service_running_ && active_job(status.job);
    if (installing)
        cyui::textColoured(cyui::palette().accent, "%s",
            cyui::tr(status.job.phase == "downloading" ? "Downloading…" :
            status.job.phase == "verifying" ? "Verifying and installing…" : "Request accepted…").c_str());
    if (status.job.phase == "error")
        subdued_wrapped(localized_diagnostic(status.job.detail));

    if (can_install) {
        cyui::ScopedDisabled disabled(!service_running_ || installing || queued_.valid());
        if (cyui::primaryButton(cyui::trLabel(is_update ? "Update" : "Install").c_str(), {-1, cyui::dp(35)})) {
            const auto tag = status.release->tag;
            queued_ = std::async(std::launch::async, [project = *project, tag] { request_update(project, tag); });
        }
        if (!service_running_)
            cyui::tooltip(cyui::tr("Start the CyHub Updater Windows service to install this package.").c_str());
    } else if (status.release && !status.release->asset) {
        subdued_wrapped(cyui::tr("See the release on GitHub."));
    }

    if (status.installed) {
        const auto wheel = project->windows_x64->asset.ends_with(".whl");
        if (cyui::ghostButton(cyui::trLabel(wheel ? "Open folder" : "Open application").c_str(), {-1, 0})) {
            try {
                open_installed(installed_entry(root_, *project, *status.installed), !wheel);
            } catch (const std::exception& error) {
                cyui::notify(localized_diagnostic(error.what()).c_str(), cyui::Tone::error);
            }
        }
    }
    if (!project->repo.empty()) {
        {
            cyui::ScopedDisabled disabled(checking_.load());
            if (cyui::ghostButton(cyui::trLabel("Check on GitHub").c_str(), {-1, 0})) {
                state_error_.clear();
                check_projects({*project});
            }
        }
        if (cyui::ghostButton(cyui::trLabel(status.release ? "View release" : "Open repository").c_str(), {-1, 0}))
            open_url(status.release ? status.release->url : project->url());
    } else {
        cyui::textFaint("%s", cyui::tr("Local project").c_str());
    }
    if (!project->skill.empty()) {
        ImGui::Dummy(dp(0, 8));
        ImGui::Separator();
        cyui::sectionTitle(cyui::tr("MCP Skill").c_str());
        if (cyui::ghostButton(cyui::trLabel("Read skill").c_str(), {-1, 0})) {
            try {
                skill_text_ = read_skill(executable_directory(), *project);
                ImGui::OpenPopup(cyui::trLabel("MCP Skill").c_str());
            } catch (const std::exception& error) {
                cyui::notify(localized_diagnostic(error.what()).c_str(), cyui::Tone::error);
            }
        }
        skill_popup(*project);
    }
}

void Dashboard::skill_popup(const Project& project) {
    const auto viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(cyui::dp(685), viewport.x * 0.92f),
                             std::min(cyui::dp(525), viewport.y * 0.9f)}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(cyui::trLabel("MCP Skill").c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    cyui::heading(project.name.c_str(), 1);
    cyui::textDim("%s", ("skills/" + project.skill + "/SKILL.md").c_str());
    if (ImGui::BeginChild("SkillText", {0, -cyui::dp(40)}, ImGuiChildFlags_Borders))
        ImGui::TextWrapped("%s", skill_text_.c_str());
    ImGui::EndChild();
    if (cyui::primaryButton(cyui::trLabel("Copy Markdown").c_str(), dp(180, 28))) {
        if (SDL_SetClipboardText(skill_text_.c_str()))
            cyui::notify(cyui::tr("Skill copied.").c_str(), cyui::Tone::success);
        else
            cyui::notify(cyui::tr("Clipboard unavailable.").c_str(), cyui::Tone::error);
    }
    ImGui::SameLine(0, cyui::dp(10));
    if (cyui::ghostButton(cyui::trLabel("Close").c_str(), dp(85, 28)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Dashboard::open_manual() {
    try {
        const auto manual = executable_directory() / "Documentation" / "Documentation.html";
        if (!fs::is_regular_file(manual)) {
            cyui::notify(cyui::tr("The manual is missing from the CyHub folder.").c_str(), cyui::Tone::error);
            return;
        }
        open_url(file_url(manual) + "#/" + cyui::locale() + "/overview");
    } catch (const std::exception& error) {
        cyui::notify(localized_diagnostic(error.what()).c_str(), cyui::Tone::error);
    }
}

void Dashboard::render() {
    collect_checks();
    if (cymcp::Clock::now() >= next_state_check_)
        refresh_state();
    if (queued_.valid() && queued_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try {
            queued_.get();
            refresh_state();
            cyui::notify(cyui::tr("Request sent to the service.").c_str(), cyui::Tone::success);
        } catch (const std::exception& error) {
            cyui::notify(localized_diagnostic(error.what()).c_str(), cyui::Tone::error);
        }
    }

    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(0, 0));
    cyui::ScopedVar spacing(ImGuiStyleVar_ItemSpacing, dp(0, 7));
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("CyHub##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
        open_manual();
    sidebar();
    ImGui::SameLine();
    {
        cyui::ScopedVar content_padding(ImGuiStyleVar_WindowPadding, dp(22, 20));
        if (ImGui::BeginChild("Collection", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding)) {
            hero();
            ImGui::SetNextItemWidth(-1);
            if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_F))
                ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##search", cyui::tr("Search for a project…").c_str(), search_.data(), search_.size());
            ImGui::Dummy(dp(0, 5));

            const auto* current = find_project(catalog_, selected_);
            if (!current || !visible(*current)) {
                const auto first = std::ranges::find_if(catalog_, [this](const auto& project) { return visible(project); });
                selected_ = first == catalog_.end() ? "" : first->id;
            }
            const auto size = ImGui::GetContentRegionAvail();
            const bool wide = size.x >= cyui::dp(690);
            const auto list_size = wide ? ImVec2(size.x - cyui::dp(296), 0)
                                        : ImVec2(0, std::max(cyui::dp(123), size.y * 0.54f));
            if (ImGui::BeginChild("Projets", list_size))
                project_list();
            ImGui::EndChild();
            if (wide)
                ImGui::SameLine(0, cyui::dp(15));
            {
                cyui::ScopedColor panel(ImGuiCol_ChildBg, cyui::palette().bg2);
                cyui::ScopedVar panel_padding(ImGuiStyleVar_WindowPadding, dp(15, 14));
                cyui::ScopedVar rounding(ImGuiStyleVar_ChildRounding, cyui::dp(10));
                if (ImGui::BeginChild("Détails", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders))
                    detail();
                ImGui::EndChild();
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
}
