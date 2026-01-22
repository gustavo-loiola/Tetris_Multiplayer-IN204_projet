#include "gui_sdl/Application.hpp"

#include <cstdio>
#include <chrono>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

namespace tetris::gui_sdl {

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::init(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_renderer = SDL_CreateRenderer(
        m_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    // ---- ImGui setup ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Load a bigger font for crisp overlays (Windows default font)
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault(); // keep default
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 32.0f);

    // Initialize backends: SDL2 + SDL_Renderer
    ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer2_Init(m_renderer);

    m_running = true;
    return true;
}

void Application::shutdown() {
    // If SDL wasn't initialized, skip.
    if (!SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS)) {
        return;
    }

    // ImGui shutdown
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void Application::setScreen(std::unique_ptr<Screen> screen) {
    m_screen = std::move(screen);
}

void Application::getWindowSize(int& w, int& h) const {
    w = 0; h = 0;
    if (m_window) SDL_GetWindowSize(m_window, &w, &h);
}

void Application::beginImGuiFrame() {
    // Clear screen FIRST so Screen can draw SDL stuff and ImGui can overlay on top
    SDL_SetRenderDrawColor(m_renderer, 20, 20, 20, 255);
    SDL_RenderClear(m_renderer);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Application::endImGuiFrame() {
    ImGui::Render();

    // Render ImGui draw data
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);

    SDL_RenderPresent(m_renderer);
}

int Application::run() {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    SDL_Event e;
    while (m_running) {
        while (SDL_PollEvent(&e)) {
            // feed events to ImGui
            ImGui_ImplSDL2_ProcessEvent(&e);

            if (e.type == SDL_QUIT) {
                m_running = false;
            }

            if (m_screen) {
                m_screen->handleEvent(*this, e);
            }
        }

        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (m_screen) {
            if (!m_running) {dt = 0;}
            m_screen->update(*this, dt);
        }

        beginImGuiFrame();

        if (m_screen) {
            m_screen->render(*this);
        }

        endImGuiFrame();
    }

    return 0;
}

} // namespace tetris::gui_sdl
