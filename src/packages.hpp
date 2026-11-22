#pragma once

#include "hub.hpp"

namespace cyhub {
void set_job(const fs::path& root, const Project& project, std::string_view phase, std::string_view detail = {});
void extract_archive(const fs::path& archive, const fs::path& directory, const Cancellation& cancel);
void activate_archive(const fs::path& root, const Project& project, const Release& release,
                      const fs::path& archive, const Cancellation& cancel);
void install_release(const fs::path& root, const Project& project, std::string_view tag,
                     const Cancellation& cancel);
void install_user_release(const fs::path& root, const Project& project, const Release& release,
                          const Cancellation& cancel);
void install_companion(const fs::path& root, const fs::path& directory, const Project& project,
                       const Release& release, const Cancellation& cancel);
bool open_companion(const fs::path& root, const fs::path& directory,
                    const Project& project, const Installed& installed);
fs::path download_release_file(const Project& project, const Release& release,
                              const fs::path& destination, const Cancellation& cancel);
void launch_self_installer(const fs::path& installer, const Asset& asset);
}
