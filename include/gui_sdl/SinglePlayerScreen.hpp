#pragma once

#include "gui_sdl/Screen.hpp"
#include "core/GameState.hpp"
#include "controller/GameController.hpp"

namespace tetris::gui_sdl {

class SinglePlayerScreen final : public Screen {
public:
    SinglePlayerScreen();

    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    // IMPORTANT: declare Layout BEFORE any method that uses it
    struct Layout {
        int cell = 28;

        int boardX = 40;
        int boardY = 40;
        int boardW = 0;
        int boardH = 0;

        int nextX = 0;
        int nextY = 0;
        int nextW = 240;
        int nextH = 210;

        bool sideBySide = true;

        int groupX = 0;
        int groupW = 0;
    };

private:
    // Rendering helpers
    void renderBoard(SDL_Renderer* renderer, int x, int y, int cellSize) const;
    void renderNextPieceWindow(int x, int y, int w, int h) const;
    void renderHUD(Application& app, int windowW, int windowH, const Layout& L,
               int gameX, int gameY, int gameW, int gameH,
               int ctrlX, int ctrlY, int ctrlW, int ctrlH);
    void renderBoardOverlayText(const Layout& L) const;

    // Input helpers
    void dispatchAction(tetris::controller::InputAction action);

    Layout computeLayout(int windowW, int windowH) const;

private:
    tetris::core::GameState gameState_;
    tetris::controller::GameController controller_;

    // Soft drop hold
    float softDropHoldAccumulatorSec_{0.0f};
    const float softDropRepeatSec_{0.05f};

    // Side movement hold (DAS + ARR)
    float leftHoldSec_{0.0f};
    float rightHoldSec_{0.0f};
    float leftRepeatAccSec_{0.0f};
    float rightRepeatAccSec_{0.0f};

    const float sideDasSec_{0.18f};
    const float sideArrSec_{0.06f};
};

} // namespace tetris::gui_sdl
