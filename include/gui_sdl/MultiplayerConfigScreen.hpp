#pragma once

#include "gui_sdl/Screen.hpp"
#include "network/MultiplayerConfig.hpp"

namespace tetris::gui_sdl {

class MultiplayerConfigScreen final : public Screen {
public:
    MultiplayerConfigScreen();

    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    tetris::net::MultiplayerConfig cfg_;

    // UI buffers
    char hostAddressBuf_[256]{};
    int port_{5000};
    int modeIndex_{0}; // 0=TimeAttack, 1=SharedTurns
    int timeLimitSec_{180};
    int piecesPerTurn_{1};
    int roleIndex_{0}; // 0 = Host, 1 = Join
};

} // namespace tetris::gui_sdl
