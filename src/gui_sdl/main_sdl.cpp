#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"

#include <memory>

int main(int, char**) {
    tetris::gui_sdl::Application app;
    if (!app.init("Multiplayer Tetris (SDL2 + ImGui)", 900, 700)) {
        return 1;
    }

    app.setScreen(std::make_unique<tetris::gui_sdl::StartScreen>());
    return app.run();
}
