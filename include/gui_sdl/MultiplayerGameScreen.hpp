#pragma once

#include "gui_sdl/Screen.hpp"
#include "network/MultiplayerConfig.hpp"
#include "core/GameState.hpp"
#include "core/Types.hpp"
#include "controller/GameController.hpp"

#include <imgui.h> // <-- necessário para ImDrawList / ImU32 / ImVec2
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

    // TimeAttack: 2 estados independentes
    tetris::core::GameState localGame_;
    tetris::controller::GameController localCtrl_;

    tetris::core::GameState oppGame_;
    tetris::controller::GameController oppCtrl_;

    // SharedTurns: 1 estado compartilhado
    tetris::core::GameState sharedGame_;
    tetris::controller::GameController sharedCtrl_;

    float oppInputAcc_ = 0.0f;

    void dispatchLocalAction(tetris::core::GameState& gs,
                             tetris::controller::GameController& gc,
                             SDL_Keycode key);

    void stepOpponentAI(float dtSeconds);

    // Rendering helpers (ASSINATURAS DEVEM BATER COM O .CPP)
    void drawBoardFromGame(ImDrawList* dl,
                           const tetris::core::GameState& gs,
                           ImVec2 topLeft,
                           float cell,
                           bool drawBorder,
                           bool drawActive) const;

    void renderTopBar(Application& app, int w, int h) const;
    void renderTimeAttackLayout(Application& app, int w, int h);
    void renderSharedTurnsLayout(Application& app, int w, int h);

    static ImU32 colorForType(tetris::core::TetrominoType t);

    // --- Hold-to-repeat (same feel as SinglePlayer) ---
    float softDropHoldAccSec_ = 0.0f;
    float leftHoldSec_ = 0.0f;
    float rightHoldSec_ = 0.0f;
    float leftRepeatAccSec_ = 0.0f;
    float rightRepeatAccSec_ = 0.0f;

    // tuning (match SinglePlayerScreen values)
    float softDropRepeatSec_ = 0.05f; // 20 Hz
    float sideDasSec_ = 0.16f;        // delay before sideways repeat
    float sideArrSec_ = 0.05f;        // sideways repeat rate

    void applyHoldInputs(float dtSeconds);
};

} // namespace tetris::gui_sdl
