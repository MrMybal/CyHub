#pragma once

#include "hub.hpp"

namespace cyhub {
bool is_companion(const Project& project);
bool is_model_companion(const Project& project);
std::string companion_github_token(const Project& project);
std::optional<Release> bundled_companion(const fs::path& directory);
Json companion_mcp_config(const fs::path& cyhub_directory);
}
