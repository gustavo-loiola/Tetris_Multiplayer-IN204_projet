#pragma once

#include <SDL.h>

namespace tetris::gui_sdl {

class Application;

// Base interface for screens (Start, SinglePlayer, Multiplayer, etc.)
class Screen {
public:
    virtual ~Screen() = default;

    // Handle SDL events (keyboard/mouse/window)
    virtual void handleEvent(Application& app, const SDL_Event& e) = 0;

    // Update simulation/UI state
    virtual void update(Application& app, float dtSeconds) = 0;

    // Render ImGui + any SDL rendering
    virtual void render(Application& app) = 0;
};

} // namespace tetris::gui_sdl
