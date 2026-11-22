#include "dashboard.hpp"
#include "companion.hpp"
#include "localization.hpp"
#ifdef _WIN32
#include "packages.hpp"
#endif

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
    if (category == "Plugins Unreal" || category == "Plugins VR" || category == "Applications VR")
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
    if (project.id == "cymcphub" || project.id == "cymodelhub" || project.id == "cymcpsdk" || project.id == "cyuemcpsdk")
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

void SDLCALL download_folder_selected(void* userdata, const char* const* paths, int) {
    const std::unique_ptr<std::promise<std::string>> result(static_cast<std::promise<std::string>*>(userdata));
    try {
        result->set_value(paths && paths[0] ? paths[0] : "");
    } catch (...) {
        result->set_exception(std::current_exception());
    }
}
}

Dashboard::Dashboard(SDL_Renderer* renderer, fs::path preferences)
    : catalog_(load_catalog(executable_directory() / "catalog.json")), root_(managed_root()),
      preferences_(std::move(preferences)), companions_(companion_root()), user_projects_(user_projects_root()),
      options_(load_preferences(preferences_)), renderer_(renderer) {
    status_.try_emplace("cyhub");
    status_.at("cyhub").detected = DetectedInstallation{std::string(app_version()), InstallationSource::executable};
    std::ranges::copy(options_.download_directory.substr(0, download_input_.size() - 1), download_input_.begin());
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
    if (const auto* companion = find_project(catalog_, "cymcphub"); companion && is_companion(*companion)) {
        auto& status = status_.at(companion->id);
        try {
            status.bundle = bundled_companion(executable_directory());
        } catch (const std::exception& error) {
            status.error = cymcp::redact_error_message(error.what());
        }
    }
    refresh_state();
    scan_installations(false, options_.check_on_start);
}

Dashboard::~Dashboard() {
    cancel_.cancel();
}

void Dashboard::refresh_state() {
    service_running_ = updater_running();
    for (const auto& project : catalog_) {
        auto& status = status_.at(project.id);
        try {
            const auto& directory = is_companion(project) ? companions_ : root_;
            status.installed = installed_version(directory, project);
            status.user_installed = false;
            if (!status.installed && project.windows_x64 && !is_companion(project)) {
                status.installed = installed_version(user_projects_, project);
                status.user_installed = status.installed.has_value();
            }
            const auto user_job = status.user_installed ||
                (active_install_ == project.id && !active_install_service_);
            status.job = update_job(user_job && !is_companion(project) ? user_projects_ : directory, project);
        } catch (const std::exception& error) {
            status.job = {"error", cymcp::redact_error_message(error.what())};
        }
    }
    next_state_check_ = cymcp::Clock::now() + std::chrono::milliseconds(900);
}

void Dashboard::scan_installations(bool check_catalog, bool check_github) {
    if (!supports_installation_detection()) {
        if (check_github)
            check_projects(check_catalog ? catalog_ : Catalog{self_update_project()});
        return;
    }
    check_catalog_ = check_catalog;
    check_after_scan_ = check_github;
    inventory_ = std::async(std::launch::async, [this] { return detect_installations(catalog_, cancel_); });
}

void Dashboard::collect_inventory() {
    if (!inventory_.valid() || inventory_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;
    try {
        auto detected = inventory_.get();
        for (auto& [id, status] : status_) {
            if (id == "cyhub")
                continue;
            auto entry = detected.extract(id);
            status.detected = entry.empty() ? std::nullopt : std::optional{std::move(entry.mapped())};
        }
    } catch (const std::exception&) {
        cyui::notify(cyui::tr("Could not scan Windows installations.").c_str(), cyui::Tone::error);
    }
    if (!check_after_scan_)
        return;
    std::vector<Project> projects{self_update_project()};
    for (const auto& project : catalog_)
        if (project.id != "cyhub" && (check_catalog_ ||
            (!download_only(project) && !status_.at(project.id).installed_tag().empty())))
            projects.push_back(project);
    if (!projects.empty())
        check_projects(std::move(projects));
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
                result.release = discover_release(project.id == "cyhub" ? self_update_project() : project, cancel_);
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

bool Dashboard::Status::present() const { return installed || detected; }

std::string_view Dashboard::Status::installed_tag() const {
    return installed ? installed->version : detected ? std::string_view(detected->version) : std::string_view{};
}

const Release* Dashboard::Status::available_release() const {
    if (bundle && (installed || !detected) &&
        (!release || !release->asset || !newer_version(bundle->tag, release->tag)))
        return &*bundle;
    return release ? &*release : nullptr;
}

bool Dashboard::Status::update_available() const {
    const auto* candidate = available_release();
    return candidate && !installed_tag().empty() && newer_version(installed_tag(), candidate->tag);
}

bool Dashboard::Status::can_install() const {
    const auto* candidate = available_release();
    return candidate && candidate->asset && (!present() || (installed && update_available()));
}

bool Dashboard::visible(const Project& project) const {
    const auto& status = status_.at(project.id);
    return (view_ == View::category || view_ == View::skills || !download_only(project)) &&
        (view_ != View::category || project.category == category_) &&
        (view_ != View::installed || status.present()) &&
        (view_ != View::updates || status.update_available()) &&
        (view_ != View::skills || !project.skill.empty()) && matches(project, search_.data());
}

void Dashboard::queue_install(const Project& project, const Release& release) {
    active_install_ = project.id;
    active_install_service_ = false;
#ifdef _WIN32
    if (is_companion(project)) {
        queued_companion_ = true;
        queued_ = std::async(std::launch::async,
            [root = companions_, directory = executable_directory(), project, release, cancel = cancel_] {
                install_companion(root, directory, project, release, cancel);
            });
        return;
    }
    const auto& status = status_.at(project.id);
    if (status.user_installed || (!status.installed &&
        (is_model_companion(project) || !service_running_ || release.asset->authenticated))) {
        queued_companion_ = false;
        queued_ = std::async(std::launch::async,
            [root = user_projects_, project, release, cancel = cancel_] { install_user_release(root, project, release, cancel); });
        return;
    }
#endif
    queued_companion_ = false;
    active_install_service_ = true;
    queued_ = std::async(std::launch::async, [project, tag = release.tag] { request_update(project, tag); });
}

void Dashboard::open_project(const Project& project, const Installed& installed) {
    try {
#ifdef _WIN32
        if (is_companion(project)) {
            if (open_companion(companions_, executable_directory(), project, installed))
                cyui::notify(cyui::tr("CyMCPHub is already running. Quit it from the tray and reopen here to load the bridge or a new version.").c_str(),
                             cyui::Tone::info);
            return;
        }
#endif
        const auto wheel = project.windows_x64->asset.ends_with(".whl");
        const auto& directory = status_.at(project.id).user_installed ? user_projects_ : root_;
        open_installed(installed_entry(directory, project, installed), !wheel);
    } catch (const std::exception& error) {
        cyui::notify(localized_diagnostic(error.what()).c_str(), cyui::Tone::error);
    }
}

void Dashboard::companion_action(std::string_view id) {
    const auto* project = find_project(catalog_, id);
    if (!project || (!is_companion(*project) && !is_model_companion(*project))) {
        ImGui::Dummy(dp(160, 33));
        return;
    }
    const auto& status = status_.at(project->id);
    const bool can_install = status.can_install() && !inventory_.valid();
    const bool model_hub = is_model_companion(*project);
    const char* action = can_install ?
        (status.installed ? (model_hub ? "Update CyModelHub" : "Update CyMCPHub") :
                            (model_hub ? "Install CyModelHub" : "Install CyMCPHub")) :
        status.installed ? (model_hub ? "Open CyModelHub" : "Open CyMCPHub") : project->name.c_str();
    cyui::ScopedDisabled disabled(queued_.valid() || !active_install_.empty() || !batch_.empty() || download_.valid());
    if (cyui::ghostButton(cyui::trLabel(action).c_str(), dp(160, 33))) {
        if (can_install)
            queue_install(*project, *status.available_release());
        else if (status.installed)
            open_project(*project, *status.installed);
        else {
            view_ = View::all;
            search_.fill(0);
            selected_ = project->id;
        }
    }
    if (ImGui::IsItemHovered())
        cyui::tooltip(project->description_for(cyui::locale()).c_str());
}

void Dashboard::self_action() {
    const auto& status = status_.at("cyhub");
    const auto* release = status.available_release();
    const bool update = status.update_available();
    const bool working = (!update && checking_.load()) || inventory_.valid() || download_.valid() ||
        !active_install_.empty() || !batch_.empty();
    cyui::ScopedDisabled disabled(working);
    const char* action = self_install_asset_ ? "Downloading…" : update ? "Update CyHub" : "Check CyHub";
    if (cyui::ghostButton(cyui::trLabel(action).c_str(), {-1, cyui::dp(27)})) {
        if (update && release->asset && supports_packages())
            queue_download(self_update_project(), *release);
        else if (update)
            open_url(release->url);
        else {
            state_error_.clear();
            check_projects({self_update_project()});
        }
    }
    if (status.checked && !working && ImGui::IsItemHovered()) {
        cyui::tooltip(cyui::tr(release ? (update ? "Update available" : "CyHub is up to date.") :
            "No accessible release").c_str());
    }
}

void Dashboard::sidebar() {
    const auto& palette = cyui::palette();
    cyui::ScopedColor background(ImGuiCol_ChildBg, palette.bg0);
    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(14, 20));
    cyui::ScopedVar spacing(ImGuiStyleVar_ItemSpacing, dp(0, 4));
    if (ImGui::BeginChild("Navigation", {cyui::dp(226), 0}, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        const auto p = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 end{p.x + cyui::dp(37), p.y + cyui::dp(37)};
        if (const auto icon = icons_.find("cyhub"); icon != icons_.end()) {
            draw->AddImage(reinterpret_cast<ImTextureID>(icon->second.get()), p, end);
        } else {
            draw->AddRectFilled(p, end, palette.accent, cyui::dp(9));
            cyui::centredText(draw, p, end, palette.accentText, "CY");
        }
        ImGui::Dummy(dp(37, 37));
        ImGui::SameLine(0, cyui::dp(10));
        ImGui::BeginGroup();
        cyui::heading("CyHub", 1);
        cyui::textFaint("v%s", std::string(app_version()).c_str());
        ImGui::EndGroup();
        self_action();

        ImGui::Dummy(dp(0, 12));
        cyui::sidebarGroup(cyui::tr("Catalog").c_str());
        const auto count = std::to_string(std::ranges::count_if(catalog_, [](const auto& p) { return !download_only(p); }));
        if (cyui::sidebarItem(cyui::icon::grid, cyui::trLabel("Projects").c_str(),
                view_ == View::all, count.c_str()))
            view_ = View::all;
        if (supports_installation_detection()) {
            const auto installed = std::to_string(std::ranges::count_if(catalog_, [&](const Project& project) {
                return !download_only(project) && status_.at(project.id).present();
            }));
            if (cyui::sidebarItem(cyui::icon::check, cyui::trLabel("Installed apps").c_str(),
                                  view_ == View::installed, installed.c_str()))
                view_ = View::installed;
        }
        const auto updates = std::ranges::count_if(status_, [](const auto& pair) { return pair.second.update_available(); });
        const auto available = std::to_string(updates);
        const auto update_pos = ImGui::GetCursorScreenPos();
        const auto update_width = ImGui::GetContentRegionAvail().x;
        const auto update_height = ImGui::GetFrameHeight() + cyui::dp(8);
        if (cyui::sidebarItem(cyui::icon::download, cyui::trLabel("Updates").c_str(), view_ == View::updates,
                              updates ? nullptr : available.c_str()))
            view_ = View::updates;
        if (updates) {
            cyui::ScopedFont small(cyui::boldFont(), 0.75f);
            const auto size = ImGui::CalcTextSize(available.c_str());
            const float right = update_pos.x + update_width - cyui::dp(8);
            const float top = update_pos.y + (update_height - size.y) / 2 - cyui::dp(2);
            draw->AddRectFilled({right-size.x-cyui::dp(10), top}, {right, top+size.y+cyui::dp(4)},
                                IM_COL32(178, 45, 63, 255), cyui::dp(7));
            draw->AddText({right-size.x-cyui::dp(5), top+cyui::dp(2)}, IM_COL32_WHITE, available.c_str());
        }
        const auto skills = std::to_string(std::ranges::count_if(catalog_, [](const auto& project) {
            return !project.skill.empty();
        }));
        if (cyui::sidebarItem(cyui::icon::code, cyui::trLabel("MCP Skills").c_str(), view_ == View::skills, skills.c_str()))
            view_ = View::skills;
        ImGui::Dummy(dp(0, 15));
        cyui::sidebarGroup(cyui::tr("By category").c_str());
        for (const auto& category : categories_) {
            const auto label = std::string(category_for(category, cyui::locale())) + "###" + category;
            if (cyui::sidebarItem(category_icon(category), label.c_str(), view_ == View::category && category_ == category)) {
                category_ = category;
                view_ = View::category;
            }
        }

        const auto lower = ImGui::GetWindowHeight() - cyui::dp(146);
        if (ImGui::GetCursorPosY() < lower)
            ImGui::SetCursorPosY(lower);
        ImGui::Separator();
        if (cyui::sidebarItem(cyui::icon::settings, cyui::trLabel("Settings").c_str(), false))
            settings_open_ = true;
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
            options_.language = language.code;
            save_settings();
        }
        if (selected)
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

void Dashboard::save_settings() {
    try {
        save_preferences(preferences_, options_);
    } catch (const std::exception&) {
        cyui::notify(cyui::tr("Could not save settings.").c_str(), cyui::Tone::warning);
    }
}

void Dashboard::settings() {
    if (folder_picker_.valid() && folder_picker_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try {
            const auto folder = folder_picker_.get();
            if (!folder.empty() && folder.size() < download_input_.size()) {
                download_input_.fill(0);
                std::ranges::copy(folder, download_input_.begin());
                options_.download_directory = folder;
                save_settings();
            }
        } catch (const std::exception&) {
            cyui::notify(cyui::tr("Could not save settings.").c_str(), cyui::Tone::warning);
        }
    }
    const auto label = cyui::trLabel("Settings");
    if (settings_open_) {
        ImGui::OpenPopup(label.c_str());
        settings_open_ = false;
    }
    ImGui::SetNextWindowSize(dp(510, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(label.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    language_picker();
    if (ImGui::Checkbox(cyui::trLabel("Check GitHub for updates on startup").c_str(), &options_.check_on_start))
        save_settings();
    ImGui::Spacing();
    cyui::sectionTitle(cyui::tr("Download folder").c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##download-path", cyui::tr("Default Downloads folder").c_str(),
                            download_input_.data(), download_input_.size());
    subdued_wrapped(cyui::tr("Used for CyHub installers and plug-in ZIPs."));
    if (cyui::primaryButton(cyui::trLabel("Save folder").c_str())) {
        try {
            const std::string candidate(download_input_.data());
            if (!candidate.empty() && !fs::u8path(candidate).is_absolute())
                throw std::runtime_error("Enter an absolute download folder path.");
            options_.download_directory = candidate;
            save_settings();
        } catch (const std::exception&) {
            cyui::notify(cyui::tr("Enter an absolute download folder path.").c_str(), cyui::Tone::warning);
        }
    }
    ImGui::SameLine(0, cyui::dp(8));
    {
        cyui::ScopedDisabled disabled(folder_picker_.valid());
        if (cyui::ghostButton(cyui::trLabel("Browse…").c_str())) {
            auto result = std::make_unique<std::promise<std::string>>();
            folder_picker_ = result->get_future();
            SDL_ShowOpenFolderDialog(download_folder_selected, result.release(), SDL_GetRenderWindow(renderer_),
                options_.download_directory.empty() ? nullptr : options_.download_directory.c_str(), false);
        }
    }
    ImGui::SameLine(0, cyui::dp(8));
    if (cyui::ghostButton(cyui::trLabel("Use default").c_str())) {
        options_.download_directory.clear();
        download_input_.fill(0);
        save_settings();
    }
    ImGui::SameLine(0, cyui::dp(8));
    if (cyui::ghostButton(cyui::trLabel("Close").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    subdued_wrapped(cyui::tr("Private GitHub repositories use GH_TOKEN with read access."));
    ImGui::EndPopup();
}

void Dashboard::toolbar() {
    const bool checking = checking_.load();
    const bool scanning = inventory_.valid();
    const bool single_row = ImGui::GetContentRegionAvail().x >= cyui::dp(680);
    if (options_.cards ? cyui::ghostButton(cyui::trLabel("List").c_str(), dp(64, 33)) :
                        cyui::primaryButton(cyui::trLabel("List").c_str(), dp(64, 33))) {
        options_.cards = false;
        save_settings();
    }
    ImGui::SameLine(0, cyui::dp(6));
    if (options_.cards ? cyui::primaryButton(cyui::trLabel("Cards").c_str(), dp(74, 33)) :
                        cyui::ghostButton(cyui::trLabel("Cards").c_str(), dp(74, 33))) {
        options_.cards = true;
        save_settings();
    }
    ImGui::SameLine(0, cyui::dp(8));
    const auto right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - cyui::dp(single_row ? 521 : 185);
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right));
    if (single_row) {
        companion_action("cymcphub");
        ImGui::SameLine(0, cyui::dp(8));
        companion_action("cymodelhub");
        ImGui::SameLine(0, cyui::dp(8));
    }
    {
        cyui::ScopedDisabled disabled(scanning || checking || !batch_.empty());
        const bool installed_only = view_ == View::installed || view_ == View::updates;
        const char* action = scanning ? "Scanning…" : checking ? "Checking…" :
            view_ == View::installed ? "Scan again" : view_ == View::updates ? "Check for updates" : "Check all";
        if (cyui::primaryButton(cyui::trLabel(action).c_str(), dp(185, 33))) {
            state_error_.clear();
            scan_installations(!installed_only, view_ != View::installed);
        }
    }
    if (!single_row) {
        companion_action("cymcphub");
        ImGui::SameLine(0, cyui::dp(8));
        companion_action("cymodelhub");
    }
    if (!batch_.empty() || !active_install_.empty()) {
        cyui::textDim("%s  %zu", cyui::tr("Install queue").c_str(), batch_.size() + !active_install_.empty());
    }
    if (!selected_batch_.empty()) {
        cyui::ScopedDisabled disabled(scanning || checking || queued_.valid() || !batch_.empty() ||
                                     !active_install_.empty() || download_.valid());
        const auto action = cyui::tr("Install selected") + " (" + std::to_string(selected_batch_.size()) + ")###InstallBatch";
        if (cyui::primaryButton(action.c_str(), dp(180, 29))) {
            Catalog pending;
            for (const auto& project : catalog_) {
                if (selected_batch_.contains(project.id) && available_for_batch(project)) {
                    batch_.push_back(project.id);
                    if (!status_.at(project.id).checked && !status_.at(project.id).available_release())
                        pending.push_back(project);
                }
            }
            selected_batch_.clear();
            if (!pending.empty())
                check_projects(std::move(pending));
        }
        ImGui::SameLine(0, cyui::dp(8));
        if (cyui::ghostButton(cyui::trLabel("Clear selection").c_str()))
            selected_batch_.clear();
    }
}

void Dashboard::project_tile(const Project& project, bool card) {
    const auto& palette = cyui::palette();
    const auto& status = status_.at(project.id);
    const auto selected = selected_ == project.id;
    const auto available = ImGui::GetContentRegionAvail().x;
    const auto origin = ImGui::GetCursorScreenPos();
    const auto height = cyui::dp(card ? 120 : 60);
    const ImVec2 end{origin.x + available, origin.y + height};
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, end, selected ? palette.bg3 : palette.bg2, cyui::dp(8));
    draw->AddRect(origin, end, selected ? palette.accentDim : palette.line, cyui::dp(8));

    cyui::ScopedID id(project.id.c_str());
    const bool eligible = available_for_batch(project);
    const auto selectable_start = origin.x + cyui::dp(33);
    ImGui::SetCursorScreenPos({origin.x + cyui::dp(7), origin.y + cyui::dp(card ? 12 : 18)});
    bool checked = selected_batch_.contains(project.id);
    if (eligible) {
        if (ImGui::Checkbox("##batch", &checked)) {
            if (checked)
                selected_batch_.insert(project.id);
            else
                selected_batch_.erase(project.id);
        }
        if (ImGui::IsItemHovered())
            cyui::tooltip(cyui::tr("Select for installation").c_str());
    }
    ImGui::SetCursorScreenPos({eligible ? selectable_start : origin.x, origin.y});
    {
        cyui::ScopedColor select(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        select.push(ImGuiCol_HeaderActive, palette.selection);
        select.push(ImGuiCol_HeaderHovered, cyui::rgb(0x78e5dc, 0.09f));
        const auto label = project.name + "###project";
        cyui::ScopedColor hide_text(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
        if (ImGui::Selectable(label.c_str(), selected, 0, {end.x - ImGui::GetCursorScreenPos().x, height}))
            selected_ = project.id;
    }
    const float left = (eligible ? selectable_start : origin.x) + cyui::dp(10);
    const ImVec2 icon_min{left, origin.y + cyui::dp(10)};
    const ImVec2 icon_max{icon_min.x + cyui::dp(34), icon_min.y + cyui::dp(34)};
    draw->AddRectFilled(icon_min, icon_max, palette.bg0, cyui::dp(6));
    if (const auto icon = icons_.find(project.id); icon != icons_.end()) {
        const auto inset = cyui::dp(4);
        draw->AddImage(reinterpret_cast<ImTextureID>(icon->second.get()),
                       {icon_min.x + inset, icon_min.y + inset}, {icon_max.x - inset, icon_max.y - inset});
    } else {
        cyui::centredText(draw, icon_min, icon_max, selected ? palette.accent : palette.textDim, fallback_icon(project));
    }
    const auto is_update = status.update_available();
    const auto* candidate = status.available_release();
    const std::string installed = !status.installed_tag().empty() ? std::string(status.installed_tag()) :
        status.detected ? cyui::tr("Version unknown") : "";
    const auto version = is_update ? installed + "  →  " + candidate->tag : installed;
    const auto date = is_update && !candidate->published.empty() ? candidate->published.substr(0, 10) : "";
    const float title_x = icon_max.x + cyui::dp(10);
    const float version_room = !card && !version.empty()
        ? std::min(available * 0.47f, ImGui::CalcTextSize(version.c_str()).x + cyui::dp(19)) : 0.0f;
    {
        cyui::ScopedFont bold(cyui::boldFont());
        cyui::clippedText(draw, {title_x, origin.y + cyui::dp(8)},
                          std::max(0.0f, end.x - title_x - version_room - cyui::dp(5)), palette.text, project.name.c_str());
    }
    const auto& description = project.description_for(cyui::locale());
    {
        cyui::ScopedFont small(cyui::uiFont(), 0.86f);
        if (card) {
            const ImVec4 clipping{origin.x + cyui::dp(10), origin.y + cyui::dp(51),
                                  end.x - cyui::dp(10), origin.y + cyui::dp(88)};
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {clipping.x, clipping.y}, palette.textDim,
                          description.c_str(), nullptr, clipping.z - clipping.x, &clipping);
        } else {
            cyui::clippedText(draw, {title_x, origin.y + cyui::dp(32)},
                              std::max(0.0f, end.x - title_x - version_room - cyui::dp(8)),
                              palette.textDim, description.c_str());
        }
    }
    if (!version.empty()) {
        cyui::ScopedFont small(cyui::uiFont(), 0.88f);
        const float x = card ? origin.x + cyui::dp(10) : end.x - version_room;
        const float y = origin.y + cyui::dp(card ? 97 : 11);
        const auto summary = card && !date.empty() ? version + " · " + date : version;
        cyui::clippedText(draw, {x, y}, end.x - x - cyui::dp(8),
                          is_update ? palette.accent : palette.textFaint, summary.c_str());
        if (!card && !date.empty())
            cyui::clippedText(draw, {x, origin.y + cyui::dp(33)}, end.x - x - cyui::dp(8), palette.textDim, date.c_str());
    }
}

void Dashboard::project_list() {
    bool any = false;
    for (const auto& category : categories_) {
        std::vector<const Project*> group;
        for (const auto& project : catalog_)
            if (project.category == category && visible(project))
                group.push_back(&project);
        if (group.empty())
            continue;
        if (any)
            ImGui::Dummy(dp(0, 8));
        any = true;
        cyui::sectionTitle(std::string(category_for(category, cyui::locale())).c_str());
        cyui::ScopedID id(category.c_str());
        if (options_.cards) {
            const int columns = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / cyui::dp(218)), 1, 4);
            cyui::ScopedVar cell_padding(ImGuiStyleVar_CellPadding, dp(4, 4));
            if (ImGui::BeginTable("##cards", columns, ImGuiTableFlags_SizingStretchSame)) {
                for (const auto* project : group) {
                    ImGui::TableNextColumn();
                    project_tile(*project, true);
                }
                ImGui::EndTable();
            }
        } else {
            for (const auto* project : group)
                project_tile(*project, false);
        }
    }
    if (!any) {
        const char* message = "No projects match your search.";
        if (view_ == View::installed || view_ == View::updates) {
            if (inventory_.valid())
                message = "Looking for installed apps…";
            else if (view_ == View::updates && checking_.load())
                message = "Checking installed versions on GitHub…";
            else if (search_.front() == '\0')
                message = view_ == View::installed ? "No catalog projects were detected on this PC." :
                    "No newer accessible releases found for installed projects.";
        }
        subdued_wrapped(cyui::tr(message));
        if (view_ == View::updates && !state_error_.empty() && !checking_.load())
            subdued_wrapped(localized_diagnostic(state_error_));
    }
}

void Dashboard::detail() {
    const auto* project = find_project(catalog_, selected_);
    if (!project || !visible(*project)) {
        cyui::textDim("%s", cyui::tr("Select a project to see its versions.").c_str());
        return;
    }
    if (is_companion(*project) || is_model_companion(*project))
        cyui::overline(cyui::tr("RECOMMENDED COMPANION").c_str());
    if (const auto icon = icons_.find(project->id); icon != icons_.end()) {
        ImGui::Image(reinterpret_cast<ImTextureID>(icon->second.get()), dp(30, 30));
        ImGui::SameLine(0, cyui::dp(8));
    }
    cyui::heading(project->name.c_str(), 1);
    const auto info = content_.find(project->id);
    subdued_wrapped(info != content_.end() && !info->second.github.description.empty() ?
        info->second.github.description : project->description_for(cyui::locale()));
    ImGui::Spacing();
    if (ImGui::BeginTabBar("ProjectDetail")) {
        if (ImGui::BeginTabItem(cyui::trLabel("Overview").c_str())) {
            project_overview(*project);
            ImGui::EndTabItem();
        }
        if (!project->repo.empty())
            project_content(*project);
        ImGui::EndTabBar();
    }
}

void Dashboard::project_overview(const Project& item) {
    const auto* project = &item;
    auto& status = status_.at(project->id);
    ImGui::Spacing();
    const auto* candidate = status.available_release();
    const auto is_update = status.update_available();
    if (is_update)
        cyui::badge(cyui::tr("Update available").c_str(), cyui::Tone::success);
    else if (status.installed)
        cyui::badge(cyui::tr("Installed by CyHub").c_str(), cyui::Tone::info);
    else if (status.detected)
        cyui::badge(cyui::tr("Detected on Windows").c_str(), cyui::Tone::info);
    else if (candidate && candidate->asset)
        cyui::badge(cyui::tr("Package available").c_str(), cyui::Tone::info);
    ImGui::Dummy(dp(0, 12));
    ImGui::Separator();
    cyui::sectionTitle(cyui::tr("Versions").c_str());
    const auto local_version = !status.installed_tag().empty() ? std::string(status.installed_tag()) :
        status.detected ? cyui::tr("Version unknown") : "—";
    if (!download_only(*project))
        subdued_wrapped(cyui::tr("Installed") + "  " + local_version);
    if (!status.installed && status.detected) {
        const auto source = status.detected->source;
        cyui::textFaint("%s", cyui::tr(source == InstallationSource::registry ? "Source · Windows registry" :
            source == InstallationSource::shortcut ? "Source · Windows shortcut" : "Source · Local executable").c_str());
    }
    const auto latest = candidate ? candidate->tag :
        cyui::tr(project->repo.empty() ? "Local project" : status.checked ? "No accessible release" : "Not checked");
    subdued_wrapped(cyui::tr("Available version") + "  " + latest);
    if (candidate && !candidate->published.empty())
        subdued_wrapped(cyui::tr("Published") + "  " + candidate->published.substr(0, 10));
    if (is_update) {
        if (const auto info = content_.find(project->id); info != content_.end()) {
            const auto count = std::ranges::count_if(info->second.github.releases, [&](const Release& release) {
                return newer_version(status.installed_tag(), release.tag) &&
                    !newer_version(candidate->tag, release.tag);
            });
            if (count > 0)
                cyui::textDim("%s  %zu", cyui::tr("Newer releases shown").c_str(), static_cast<std::size_t>(count));
        }
    }
    if (!status.error.empty())
        subdued_wrapped(localized_diagnostic(status.error));

    ImGui::Dummy(dp(0, 9));
    const auto can_install = !download_only(*project) && status.can_install() && !inventory_.valid();
    const auto companion = is_companion(*project);
    const auto installing = active_install_ == project->id || (service_running_ && active_job(status.job));
    if (installing)
        cyui::textColoured(cyui::palette().accent, "%s",
            cyui::tr(status.job.phase == "downloading" ? "Downloading…" :
            status.job.phase == "verifying" ? "Verifying and installing…" : "Request accepted…").c_str());
    if (status.job.phase == "error")
        subdued_wrapped(localized_diagnostic(status.job.detail));

    if (project->id == "cyhub") {
        self_action();
    } else if (download_only(*project)) {
        cyui::ScopedDisabled disabled(download_.valid());
        if (cyui::primaryButton(cyui::trLabel(candidate && candidate->asset && supports_packages() ?
                "Download ZIP" : "Download sources on GitHub").c_str(), {-1, cyui::dp(35)})) {
            if (candidate && candidate->asset && supports_packages())
                queue_download(*project, *candidate);
            else
                open_url(project->url() + "/archive/HEAD.zip");
        }
    } else if (can_install) {
        const bool needs_service = !companion && status.installed && !status.user_installed &&
            (!service_running_ || candidate->asset->authenticated);
        cyui::ScopedDisabled disabled(needs_service || installing || !active_install_.empty() ||
                                      !batch_.empty() || queued_.valid() || download_.valid());
        if (cyui::primaryButton(cyui::trLabel(is_update ? "Update" : "Install").c_str(), {-1, cyui::dp(35)})) {
            queue_install(*project, *candidate);
        }
        if (needs_service)
            cyui::tooltip(cyui::tr("Start the CyHub Updater Windows service to install this package.").c_str());
    } else if (is_update && status.detected && !status.installed) {
        subdued_wrapped(cyui::tr("Update this installation from its GitHub release."));
    }

    if (status.installed && project->windows_x64) {
        const auto wheel = project->windows_x64->asset.ends_with(".whl");
        if (cyui::ghostButton(cyui::trLabel(wheel ? "Open folder" : "Open application").c_str(), {-1, 0}))
            open_project(*project, *status.installed);
    }
    if (!project->repo.empty()) {
        {
            cyui::ScopedDisabled disabled(inventory_.valid() || checking_.load());
            if (cyui::ghostButton(cyui::trLabel("Check on GitHub").c_str(), {-1, 0})) {
                state_error_.clear();
                check_projects({*project});
            }
        }
        const bool from_github = status.release.has_value();
        if (cyui::ghostButton(cyui::trLabel(from_github ? "View release" : "Open repository").c_str(), {-1, 0}))
            open_url(from_github ? status.release->url : project->url());
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

bool Dashboard::available_for_batch(const Project& project) const {
    if (!supports_packages() || download_only(project) || !project.windows_x64 || project.id == "cyhub")
        return false;
    const auto& status = status_.at(project.id);
    if (!status.present())
        return true;
    return status.installed && (is_companion(project) || status.user_installed || service_running_) &&
        (status.update_available() || !status.checked);
}

void Dashboard::advance_batch() {
    if (checking_.load() || queued_.valid() || !active_install_.empty() || batch_.empty())
        return;
    const auto id = std::move(batch_.front());
    batch_.pop_front();
    const auto* project = find_project(catalog_, id);
    const auto& status = status_.at(id);
    const auto* release = status.available_release();
    const bool private_machine_install = release && release->asset && release->asset->authenticated &&
        status.installed && !status.user_installed && project && !is_companion(*project);
    if (project && available_for_batch(*project) && status.can_install() && !private_machine_install) {
        queue_install(*project, *release);
    } else {
        const auto message = (project ? project->name : id) + " · " + cyui::tr("No compatible package to install.");
        cyui::notify(message.c_str(), cyui::Tone::warning);
    }
}

void Dashboard::queue_download(const Project& project, const Release& release) {
#ifdef _WIN32
    try {
        const auto path = options_.download_directory.empty() ? default_download_directory()
                                                               : fs::u8path(options_.download_directory);
        if (project.id == "cyhub")
            self_install_asset_ = release.asset;
        download_ = std::async(std::launch::async, [project, release, path, cancel = cancel_] {
            return download_release_file(project, release, path, cancel);
        });
    } catch (const std::exception& error) {
        self_install_asset_.reset();
        cyui::notify(localized_diagnostic(cymcp::redact_error_message(error.what())).c_str(), cyui::Tone::error);
    }
#else
    (void)project;
    open_url(release.url);
#endif
}

void Dashboard::collect_download() {
    if (!download_.valid() || download_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;
    try {
        const auto path = download_.get();
#ifdef _WIN32
        if (self_install_asset_) {
            launch_self_installer(path, *self_install_asset_);
            SDL_Event event{};
            event.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&event);
        } else
#endif
        {
            const auto message = cyui::tr("Download complete") + " · " + cymcp::path_utf8(path.filename());
            cyui::notify(message.c_str(), cyui::Tone::success);
            open_url(file_url(path.parent_path()));
        }
    } catch (const std::exception& error) {
        cyui::notify(localized_diagnostic(cymcp::redact_error_message(error.what())).c_str(), cyui::Tone::error);
    }
    self_install_asset_.reset();
}

void Dashboard::render() {
    collect_checks();
    collect_inventory();
    collect_download();
    if (cymcp::Clock::now() >= next_state_check_)
        refresh_state();
    if (queued_.valid() && queued_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try {
            queued_.get();
            refresh_state();
            if (!active_install_service_) {
                cyui::notify(cyui::tr(queued_companion_ ? "CyMCPHub is ready to open." :
                    "Installation complete.").c_str(), cyui::Tone::success);
                active_install_.clear();
            }
        } catch (const std::exception& error) {
            cyui::notify(localized_diagnostic(error.what()).c_str(), cyui::Tone::error);
            active_install_.clear();
        }
        queued_companion_ = false;
    }
    if (!active_install_.empty() && active_install_service_ && !queued_.valid()) {
        const auto& job = status_.at(active_install_).job;
        if (job.phase == "done" || job.phase == "error" || !service_running_) {
            const bool done = job.phase == "done";
            cyui::notify(cyui::tr(done ? "Installation complete." : "Installation stopped.").c_str(),
                         done ? cyui::Tone::success : cyui::Tone::error);
            active_install_.clear();
        }
    }
    advance_batch();

    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(0, 0));
    cyui::ScopedVar spacing(ImGuiStyleVar_ItemSpacing, dp(0, 7));
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("CyHub##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
        open_manual();
    sidebar();
    settings();
    ImGui::SameLine();
    {
        cyui::ScopedVar content_padding(ImGuiStyleVar_WindowPadding, dp(22, 20));
        if (ImGui::BeginChild("Collection", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding)) {
            toolbar();
            ImGui::SetNextItemWidth(-1);
            if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_F))
                ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##search", cyui::tr("Search for a project…").c_str(), search_.data(), search_.size());
            ImGui::Dummy(dp(0, 5));

            const auto* current = find_project(catalog_, selected_);
            if (current && !visible(*current))
                selected_.clear();
            const auto size = ImGui::GetContentRegionAvail();
            const bool wide = size.x >= cyui::dp(690);
            const auto list_size = wide ? ImVec2(size.x - cyui::dp(355), 0)
                                        : ImVec2(0, std::max(cyui::dp(123), size.y * 0.54f));
            if (ImGui::BeginChild("Projets", list_size))
                project_list();
            ImGui::EndChild();
            fetch_detail();
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
