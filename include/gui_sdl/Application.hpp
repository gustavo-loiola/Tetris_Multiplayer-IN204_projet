#pragma once

#include <memory>
#include <string>

#include <SDL.h>

#include "gui_sdl/Screen.hpp"

namespace tetris::gui_sdl {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool init(const char* title, int width, int height);
    int run();

    void requestQuit() { m_running = false; }

    // Screen management
    void setScreen(std::unique_ptr<Screen> screen);

    // SDL accessors
    SDL_Window* window() const { return m_window; }
    SDL_Renderer* renderer() const { return m_renderer; }

    // Simple utility: window size
    void getWindowSize(int& w, int& h) const;

private:
    void shutdown();
    void beginImGuiFrame();
    void endImGuiFrame();

private:
    bool m_running{false};

    SDL_Window* m_window{nullptr};
    SDL_Renderer* m_renderer{nullptr};

    std::unique_ptr<Screen> m_screen;
};

} // namespace tetris::gui_sdl
