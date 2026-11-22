#pragma once

#include "hub.hpp"

namespace cyhub {
struct ProjectImage {
    std::string source, bytes;
};

struct GitHubProject {
    std::string description, readme, showcase_url;
    std::vector<Release> releases;
    std::vector<ProjectImage> images;
    std::string warning;
};

GitHubProject github_project(const Project& project, const Cancellation& cancel);
}
