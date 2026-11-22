#pragma once

#include "installed_apps.hpp"
#include "github_project.hpp"
#include <array>
#include <deque>
#include <future>
#include <mutex>
#include <set>
#include <thread>

struct SDL_Renderer;
struct SDL_Texture;

namespace cyhub {
class Dashboard {
public:
    Dashboard(SDL_Renderer* renderer, fs::path preferences);
    ~Dashboard();
    void render();

private:
    enum class View { all, installed, updates, skills, category };
    struct Status {
        std::optional<Installed> installed;
        std::optional<DetectedInstallation> detected;
        Job job;
        std::optional<Release> release;
        std::optional<Release> bundle;
        bool checked = false;
        bool user_installed = false;
        std::string error;

        bool present() const;
        std::string_view installed_tag() const;
        const Release* available_release() const;
        bool update_available() const;
        bool can_install() const;
    };
    struct Check {
        std::string id;
        std::optional<Release> release;
        std::string error;
    };

    void refresh_state();
    void scan_installations(bool check_catalog = false, bool check_github = true);
    void collect_inventory();
    void check_projects(std::vector<Project> projects);
    void collect_checks();
    void sidebar();
    void language_picker();
    void toolbar();
    void companion_action(std::string_view id);
    void self_action();
    void settings();
    void save_settings();
    void project_list();
    void project_tile(const Project& project, bool card);
    void detail();
    void project_overview(const Project& project);
    void fetch_detail();
    void project_content(const Project& project);
    void skill_popup(const Project& project);
    void open_manual();
    void queue_install(const Project& project, const Release& release);
    void queue_download(const Project& project, const Release& release);
    void collect_download();
    void advance_batch();
    bool available_for_batch(const Project& project) const;
    void open_project(const Project& project, const Installed& installed);
    bool visible(const Project& project) const;

    Catalog catalog_;
    fs::path root_, preferences_, companions_, user_projects_;
    Preferences options_;
    SDL_Renderer* renderer_;
    using Texture = std::unique_ptr<SDL_Texture, void (*)(SDL_Texture*)>;
    std::map<std::string, Texture> icons_;
    struct ProjectContent {
        GitHubProject github;
        std::vector<Texture> images;
    };
    std::map<std::string, ProjectContent> content_;
    std::map<std::string, Status> status_;
    std::vector<std::string> categories_;
    std::string category_, selected_;
    std::array<char, 160> search_{};
    std::array<char, 1024> download_input_{};
    bool settings_open_ = false;
    std::set<std::string> selected_batch_;
    std::deque<std::string> batch_;
    std::string active_install_;
    bool active_install_service_ = false;
    View view_ = View::all;
    bool service_running_ = false;
    std::string state_error_;
    std::string skill_text_;
    cymcp::Clock::time_point next_state_check_{};

    std::mutex mutex_;
    std::deque<Check> results_;
    Cancellation cancel_;
    std::atomic_bool checking_{false};
    std::jthread worker_;
    std::future<InstallationInventory> inventory_;
    bool check_catalog_ = false;
    bool check_after_scan_ = true;
    std::future<void> queued_;
    bool queued_companion_ = false;
    std::future<std::pair<std::string, GitHubProject>> detail_fetch_;
    std::future<fs::path> download_;
    std::future<std::string> folder_picker_;
    std::optional<Asset> self_install_asset_;
};
}
