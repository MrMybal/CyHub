cmake_minimum_required(VERSION 3.24)
project(CyHub VERSION 0.2.0 LANGUAGES C CXX)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
endif()

# FETCHCONTENT_SOURCE_DIR_CYUI, _CYMCPSDK, _SDL3 and _MINIZ may point to existing checkouts.
set(CYUI_BUILD_DEMO OFF CACHE BOOL "")
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "")
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "")
set(CYUI_MODULES Core CACHE STRING "")
set(CYMCP_BUILD_AI OFF CACHE BOOL "")
set(CYMCP_BUILD_LOCAL OFF CACHE BOOL "")
set(CYMCP_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CYMCP_BUILD_TESTS OFF CACHE BOOL "")
set(SDL_SHARED OFF CACHE BOOL "")
set(SDL_STATIC ON CACHE BOOL "")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
set(SDL_TESTS OFF CACHE BOOL "")
set(SDL_EXAMPLES OFF CACHE BOOL "")

FetchContent_Declare(cyui GIT_REPOSITORY https://github.com/MrMybal/CyUI.git
  GIT_TAG 6ef01cfcdb197f3a89d64618ce5ef8818c704728)
FetchContent_Declare(cymcpsdk GIT_REPOSITORY https://github.com/MrMybal/CyMCPSDK.git
  GIT_TAG e562c731a146d599e3404718ea8414bb5b96b66d)
FetchContent_Declare(sdl3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG a962f40bbba175e9716557a25d5d7965f134a3d3)
FetchContent_MakeAvailable(cyui cymcpsdk sdl3)

add_library(cyhub_core STATIC src/catalog.cpp src/companion.cpp src/preferences.cpp src/releases.cpp src/platform.cpp)
target_include_directories(cyhub_core PUBLIC src)
target_link_libraries(cyhub_core PUBLIC CyMCP::Http)
if(WIN32)
  target_sources(cyhub_core PRIVATE src/windows_pipe.cpp)
  target_link_libraries(cyhub_core PUBLIC shell32 ole32 advapi32)
endif()

add_executable(CyHub src/main.cpp src/dashboard.cpp src/localization.cpp
  ${cyui_SOURCE_DIR}/third_party/imgui-sdl3/imgui_impl_sdl3.cpp
  ${cyui_SOURCE_DIR}/third_party/imgui-sdl3/imgui_impl_sdlrenderer3.cpp)
target_include_directories(CyHub PRIVATE ${cyui_SOURCE_DIR}/third_party/imgui-sdl3)
target_link_libraries(CyHub PRIVATE cyhub_core CyUI::Core SDL3::SDL3-static)
if(WIN32)
  set_target_properties(CyHub PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

add_executable(CyHubMCP src/mcp.cpp)
target_link_libraries(CyHubMCP PRIVATE cyhub_core CyMCP::MCP)
set(hub_targets CyHub CyHubMCP)

if(WIN32)
  set(BUILD_EXAMPLES OFF CACHE BOOL "")
  set(BUILD_FUZZERS OFF CACHE BOOL "")
  set(INSTALL_PROJECT OFF CACHE BOOL "")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
  FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 174573d60290f447c13a2b1b3405de2b96e27d6c)
  FetchContent_MakeAvailable(miniz)
  add_library(cyhub_packages STATIC src/packages.cpp)
  target_include_directories(cyhub_packages PRIVATE ${cymcpsdk_SOURCE_DIR}/third_party/picosha2)
  target_link_libraries(cyhub_packages PUBLIC cyhub_core PRIVATE miniz)
  target_link_libraries(CyHub PRIVATE cyhub_packages)
  add_executable(CyHubUpdater src/windows_service.cpp)
  target_link_libraries(CyHubUpdater PRIVATE cyhub_packages)
  list(APPEND hub_targets CyHubUpdater)
  install(FILES scripts/Install-CyHub.ps1 scripts/Uninstall-CyHubService.ps1 DESTINATION . COMPONENT CyHub)
  install(FILES ${miniz_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME miniz.txt COMPONENT CyHub)
  install(FILES ${cymcpsdk_SOURCE_DIR}/third_party/picosha2/LICENSE DESTINATION licenses RENAME PicoSHA2.txt COMPONENT CyHub)
endif()

foreach(target IN LISTS hub_targets)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()
set(CYHUB_CYMCPHUB_PACKAGE "" CACHE FILEPATH "Optional CyMCPHub Windows x64 ZIP bundled with CyHub")
set(companion_files COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:CyHub>/cymcphub-package.json")
if(WIN32 AND CYHUB_CYMCPHUB_PACKAGE)
  get_filename_component(companion_name "${CYHUB_CYMCPHUB_PACKAGE}" NAME)
  if(NOT companion_name MATCHES "^CyMCPHub-([0-9]+\\.[0-9]+\\.[0-9]+)-windows-x64\\.zip$")
    message(FATAL_ERROR "Use a Windows ZIP named CyMCPHub-<version>-windows-x64.zip")
  endif()
  set(companion_tag "v${CMAKE_MATCH_1}")
  file(SHA256 "${CYHUB_CYMCPHUB_PACKAGE}" companion_sha256)
  file(SIZE "${CYHUB_CYMCPHUB_PACKAGE}" companion_size)
  set(companion_manifest "${CMAKE_CURRENT_BINARY_DIR}/cymcphub-package.json")
  set(companion_json "{\"tag\":\"${companion_tag}\",\"sha256\":\"${companion_sha256}\",\"size\":${companion_size}}\n")
  file(CONFIGURE OUTPUT "${companion_manifest}" CONTENT "${companion_json}" @ONLY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CYHUB_CYMCPHUB_PACKAGE}")
  set(companion_files
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${companion_manifest}" "$<TARGET_FILE_DIR:CyHub>/cymcphub-package.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CYHUB_CYMCPHUB_PACKAGE}" "$<TARGET_FILE_DIR:CyHub>/CyMCPHub-bundled.zip")
  install(FILES "${companion_manifest}" DESTINATION . COMPONENT CyHub)
  install(FILES "${CYHUB_CYMCPHUB_PACKAGE}" DESTINATION . RENAME CyMCPHub-bundled.zip COMPONENT CyHub)
endif()
add_custom_target(cyhub_catalog ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CyHub>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/catalog.json"
    "$<TARGET_FILE_DIR:CyHub>/catalog.json"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/skills"
    "$<TARGET_FILE_DIR:CyHub>/skills"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
    "$<TARGET_FILE_DIR:CyHub>/assets/icons"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Documentation"
    "$<TARGET_FILE_DIR:CyHub>/Documentation"
  ${companion_files}
  VERBATIM)
foreach(target IN LISTS hub_targets)
  add_dependencies(${target} cyhub_catalog)
endforeach()

install(TARGETS ${hub_targets} RUNTIME DESTINATION . COMPONENT CyHub)
install(FILES LICENSE catalog.json README.md README.fr.md VERSION.md VERSION.fr.md DESTINATION . COMPONENT CyHub)
install(DIRECTORY skills/ DESTINATION skills COMPONENT CyHub FILES_MATCHING PATTERN "SKILL.md")
install(DIRECTORY assets/icons/ DESTINATION assets/icons COMPONENT CyHub)
install(DIRECTORY Documentation/ DESTINATION Documentation COMPONENT CyHub)
install(FILES ${cyui_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME CyUI.txt COMPONENT CyHub)
install(FILES ${cymcpsdk_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME CyMCPSDK.txt COMPONENT CyHub)
install(FILES ${sdl3_SOURCE_DIR}/LICENSE.txt DESTINATION licenses RENAME SDL3.txt COMPONENT CyHub)
install(FILES ${cyui_SOURCE_DIR}/third_party/imgui/LICENSE.txt DESTINATION licenses RENAME DearImGui.txt COMPONENT CyHub)
install(FILES ${cymcpsdk_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT DESTINATION licenses RENAME nlohmann-json.txt COMPONENT CyHub)
install(FILES ${cymcpsdk_SOURCE_DIR}/third_party/httplib/LICENSE DESTINATION licenses RENAME cpp-httplib.txt COMPONENT CyHub)

include(CTest)
if(BUILD_TESTING)
  add_executable(CyHubTests tests/core.cpp)
  target_link_libraries(CyHubTests PRIVATE cyhub_core)
  if(WIN32)
    target_link_libraries(CyHubTests PRIVATE cyhub_packages miniz)
  endif()
  add_test(NAME CyHubCore COMMAND CyHubTests "${CMAKE_CURRENT_SOURCE_DIR}/catalog.json")
  if(WIN32 AND CYHUB_CYMCPHUB_PACKAGE)
    add_test(NAME CyHubCompanion COMMAND CyHubTests "${CMAKE_CURRENT_SOURCE_DIR}/catalog.json"
      --companion "$<TARGET_FILE_DIR:CyHub>")
  endif()
endif()
