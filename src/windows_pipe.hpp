#pragma once

#include "hub.hpp"
#include <windows.h>
#include <functional>

namespace cyhub {
inline constexpr wchar_t service_name[] = L"CyHubUpdater";
void serve_updates(HANDLE stop, const std::function<Json(const Json&)>& handler,
                   const std::function<void()>& ready);
}
