#pragma once

#include <string>
#include <string_view>

namespace cyhub {
struct Language {
    const char* code;
    const char* name;
};
inline constexpr Language languages[] = {
    {"en", "English"},
    {"fr", "Français"},
};

void register_ui_translations();
std::string localized_diagnostic(std::string_view message);
}
