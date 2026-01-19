#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"

#include <memory>

int main(int, char**) {
    tetris::gui_sdl::Application app;
    if (!app.init("Tetris - IN204 Project - Made by Gustavo Loiola and Felipe Biasuz", 900, 700)) {
        return 1;
    }

    app.setScreen(std::make_unique<tetris::gui_sdl::StartScreen>());
    return app.run();
}
