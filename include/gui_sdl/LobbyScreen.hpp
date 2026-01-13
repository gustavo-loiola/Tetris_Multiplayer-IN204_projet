#pragma once

#include "gui_sdl/Screen.hpp"
#include "network/MultiplayerConfig.hpp"

namespace tetris::gui_sdl {

class LobbyScreen final : public Screen {
public:
    explicit LobbyScreen(const tetris::net::MultiplayerConfig& cfg);

    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    tetris::net::MultiplayerConfig cfg_;
    bool localReady_{false};

    // For now, we show placeholders for connection/remote readiness.
    bool connected_{false};
    bool remoteReady_{false};
};

} // namespace tetris::gui_sdl
