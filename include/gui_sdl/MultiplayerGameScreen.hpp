#pragma once

#include "gui_sdl/Screen.hpp"
#include "network/MultiplayerConfig.hpp"
#include "network/MessageTypes.hpp"

#include <imgui.h>      // <-- FIX 1: brings ImDrawList, ImVec2, etc.
#include <optional>

namespace tetris::gui_sdl {

class MultiplayerGameScreen final : public Screen {
public:
    explicit MultiplayerGameScreen(const tetris::net::MultiplayerConfig& cfg);

    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    tetris::net::MultiplayerConfig cfg_;

    std::optional<tetris::net::StateUpdate> lastUpdate_;

    // UI-first identity (later: set from JoinAccept)
    tetris::net::PlayerId localId_{1};

    // Fake simulation for visuals
    tetris::net::Tick fakeTick_{0};
    float fakeAccumulator_{0.0f};

    // Helpers
    void ensureFakeUpdate();
    void stepFakeWorld(float dtSeconds);

    void renderTopBar(Application& app, int w);
    void renderTimeAttackLayout(Application& app, int w, int h);
    void renderSharedTurnsLayout(Application& app, int w, int h);

    // FIX 2: signature must match the .cpp exactly (5 args)
    void drawBoardDTO(ImDrawList* dl,
                      const tetris::net::BoardDTO& board,
                      ImVec2 topLeft,
                      float cellPx,
                      bool drawGrid);

    const tetris::net::PlayerStateDTO* findPlayer(tetris::net::PlayerId id) const;
    const tetris::net::PlayerStateDTO* findOpponent() const;

    static float clampf(float v, float lo, float hi) {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }
};

} // namespace tetris::gui_sdl
