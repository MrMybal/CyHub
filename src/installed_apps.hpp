#pragma once

#include "hub.hpp"
#include <map>

namespace cyhub {
enum class InstallationSource { registry, shortcut, executable };

struct DetectedInstallation {
    std::string version;
    InstallationSource source;
};

using InstallationInventory = std::map<std::string, DetectedInstallation>;
bool supports_installation_detection();
InstallationInventory detect_installations(const Catalog& catalog, const Cancellation& cancel = {});
}
