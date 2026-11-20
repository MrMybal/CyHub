#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cyui/Core.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "dashboard.hpp"
#include "localization.hpp"

namespace {
void apply_theme() {
    cyui::Config config;
    config.themeKey = "navy";
    config.density = cyui::Density::comfortable;
    cyui::apply(config);
    auto palette = cyui::palette();
    palette.bg0 = cyui::rgb(0x0a1020);
    palette.bg1 = cyui::rgb(0x10182a);
    palette.bg2 = cyui::rgb(0x172238);
    palette.bg3 = cyui::rgb(0x1c2b43);
    palette.bg4 = cyui::rgb(0x253956);
    palette.bg5 = cyui::rgb(0x304969);
    palette.line = cyui::rgb(0x263650);
    palette.lineStrong = cyui::rgb(0x334a67);
    palette.accent = cyui::rgb(0x78e5dc);
    palette.accentHover = cyui::rgb(0x9bf3eb);
    palette.accentDim = cyui::rgb(0x4a9c98);
    palette.accentText = cyui::rgb(0x06252b);
    palette.selection = cyui::rgb(0x477f99, 0.44f);
    cyui::setPalette(palette);
    cyui::loadFonts();
}

int run(SDL_Window* window, SDL_Renderer* renderer, const cyhub::fs::path& preferences,
        int frame_limit, const char* snapshot) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    apply_theme();
    cyhub::register_ui_translations();
    cyui::setLocale(cyhub::load_language(preferences).c_str());
    const bool platform_ready = ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    const bool renderer_ready = platform_ready && ImGui_ImplSDLRenderer3_Init(renderer);
    int result = 0;
    try {
        if (!renderer_ready)
            throw std::runtime_error("Could not initialize the renderer.");
        cyhub::Dashboard dashboard(renderer, preferences);
        bool active = true;
        float scale = 0;
        int frames = 0;
        while (active) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    active = false;
            }
            if (!active)
                break;
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
                SDL_Delay(40);
                continue;
            }

            const auto density = SDL_GetWindowPixelDensity(window);
            const auto desired_scale = density > 0 ? SDL_GetWindowDisplayScale(window) / density : 1.0f;
            if (desired_scale > 0 && desired_scale != scale) {
                scale = desired_scale;
                cyui::applyDpi(scale);
            }
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            dashboard.render();
            cyui::renderNotifications();
            ImGui::Render();
            SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            SDL_SetRenderDrawColor(renderer, 10, 16, 32, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            if (snapshot && frames + 1 == frame_limit) {
                const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>
                    pixels(SDL_RenderReadPixels(renderer, nullptr), SDL_DestroySurface);
                if (!pixels || !SDL_SaveBMP(pixels.get(), snapshot))
                    throw std::runtime_error(SDL_GetError());
            }
            SDL_RenderPresent(renderer);
            if (frame_limit && ++frames >= frame_limit)
                active = false;
        }
    } catch (const std::exception& error) {
        if (frame_limit)
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
        else
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CyHub", cyhub::localized_diagnostic(error.what()).c_str(), window);
        result = 1;
    }
    if (renderer_ready)
        ImGui_ImplSDLRenderer3_Shutdown();
    if (platform_ready)
        ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    return result;
}
}

int main(int argc, char** argv) {
    const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke";
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        if (smoke)
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
        else
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CyHub", SDL_GetError(), nullptr);
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int result = 1;
    if (SDL_CreateWindowAndRenderer("CyHub", 1320, 850,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) {
        if (const auto* base = SDL_GetBasePath()) {
            const auto icon_path = std::string(base) + "assets/icons/cyhub.png";
            if (auto* icon = SDL_LoadPNG(icon_path.c_str())) {
                SDL_SetWindowIcon(window, icon);
                SDL_DestroySurface(icon);
            }
        }
        SDL_SetWindowMinimumSize(window, 790, 560);
        const bool vsync = SDL_SetRenderVSync(renderer, 1);
        cyhub::fs::path preferences;
        if (smoke) {
            if (argc > 3)
                preferences = cyhub::fs::u8path(argv[3]);
        } else {
            const std::unique_ptr<char, decltype(&SDL_free)> directory(SDL_GetPrefPath("CyHub", "CyHub"), SDL_free);
            if (directory)
                preferences = cyhub::fs::u8path(directory.get()) / "settings.json";
        }
        result = run(window, renderer, preferences, smoke ? 8 : 0, smoke && argc > 2 ? argv[2] : nullptr);
        (void)vsync;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    } else if (smoke) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
    } else {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CyHub", SDL_GetError(), nullptr);
    }
    SDL_Quit();
    return result;
}
