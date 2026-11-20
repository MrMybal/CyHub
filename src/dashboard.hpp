#pragma once

#include "hub.hpp"
#include <array>
#include <deque>
#include <future>
#include <mutex>
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
    struct Status {
        std::optional<Installed> installed;
        Job job;
        std::optional<Release> release;
        bool checked = false;
        std::string error;
    };
    struct Check {
        std::string id;
        std::optional<Release> release;
        std::string error;
    };

    void refresh_state();
    void check_projects(std::vector<Project> projects);
    void collect_checks();
    void sidebar();
    void language_picker();
    void hero();
    void project_list();
    void detail();
    void skill_popup(const Project& project);
    void open_manual();
    bool visible(const Project& project) const;
    bool ready(const Status& status) const;

    Catalog catalog_;
    fs::path root_, preferences_;
    std::map<std::string, std::unique_ptr<SDL_Texture, void (*)(SDL_Texture*)>> icons_;
    std::map<std::string, Status> status_;
    std::vector<std::string> categories_;
    std::string category_, selected_;
    std::array<char, 160> search_{};
    bool updates_only_ = false;
    bool skills_only_ = false;
    bool service_running_ = false;
    std::string state_error_;
    std::string skill_text_;
    cymcp::Clock::time_point next_state_check_{};

    std::mutex mutex_;
    std::deque<Check> results_;
    Cancellation cancel_;
    std::atomic_bool checking_{false};
    std::jthread worker_;
    std::future<void> queued_;
};
}
