#pragma once

#include "gui_sdl/Screen.hpp"
#include "network/MultiplayerConfig.hpp"
#include "network/MessageTypes.hpp"
#include "core/GameState.hpp"
#include "core/Types.hpp"
#include "controller/GameController.hpp"

#include <imgui.h>     // ImDrawList / ImU32 / ImVec2
#include <optional>
#include <memory>
#include <mutex>
#include <cstdint>

namespace tetris::net {
class NetworkHost;
class NetworkClient;
} // namespace tetris::net

namespace tetris::gui_sdl {

class MultiplayerGameScreen final : public Screen {
public:
    explicit MultiplayerGameScreen(const tetris::net::MultiplayerConfig& cfg,
                                   std::shared_ptr<tetris::net::NetworkHost> host = nullptr,
                                   std::shared_ptr<tetris::net::NetworkClient> client = nullptr);

    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    // ---- Config ----
    tetris::net::MultiplayerConfig cfg_{};

    // ---- Network (optional) ----
    std::shared_ptr<tetris::net::NetworkHost> host_;
    std::shared_ptr<tetris::net::NetworkClient> client_;

    bool isHost_ = true;                 // host/offline if client_ == nullptr
    std::uint64_t clientTick_ = 0;       // client-side tick for InputActionMessage
    tetris::net::Tick serverTick_ = 0;   // host-side tick for StateUpdate
    tetris::net::Tick lastStartTickSeen_ = 0;

    // Snapshot sending (host)
    float snapshotAccSec_ = 0.0f;
    const float snapshotPeriodSec_ = 0.05f; // 20 Hz

    // -------- SharedTurns (host authoritative + HUD) --------
    tetris::net::PlayerId turnPlayerId_ = 1;     // host starts by default
    std::uint32_t piecesLeftThisTurn_ = 0;       // init from cfg_.piecesPerTurn in ctor
    bool boardHashInit_ = false;
    std::uint32_t lastBoardHash_ = 0;

    // in SharedTurns, used to decide who caused the GameOver (last input applier)
    tetris::net::PlayerId lastActionPlayerId_ = 1;

    void updateSharedTurnsTurnHost();

    static std::uint32_t boardHash(const tetris::core::Board& b);

    // -------- Disconnect / net quality --------
    bool hostDisconnected_ = false;       // client: detected host is gone
    bool opponentDisconnected_ = false;   // host: detected client left / client: opponent left
    float timeSinceLastSnapshotSec_ = 0.0f;
    float snapshotTimeoutSec_ = 2.0f;

    // -------- TimeAttack timer (client display from StateUpdate) --------
    std::uint32_t displayTimeLeftMs_ = 0;

    // ---- Match end / result ----
    bool matchEnded_ = false;
    float matchElapsedSec_ = 0.0f;

    bool hostWantsRematch_ = false;
    bool clientWantsRematch_ = false;
    bool waitingRematchStart_ = false;

    std::optional<tetris::net::MatchResult> localMatchResult_;
    std::optional<tetris::net::MatchResult> clientMatchResult_;

    void tryFinalizeMatchHost();
    void renderMatchOverlay(Application& app, int w, int h);

    // Client received snapshot (authoritative render)
    mutable std::mutex stateMutex_;
    std::optional<tetris::net::StateUpdate> lastState_;

    // -------- Core games (host/offline only) --------
    tetris::core::GameState localGame_;
    tetris::controller::GameController localCtrl_;

    tetris::core::GameState oppGame_;
    tetris::controller::GameController oppCtrl_;

    tetris::core::GameState sharedGame_;
    tetris::controller::GameController sharedCtrl_;

    float oppInputAcc_ = 0.0f; // offline AI only

    // ---------- INPUT ----------
    static std::optional<tetris::controller::InputAction> actionFromKey(SDL_Keycode key);

    void sendOrApplyAction(tetris::core::GameState& gs,
                           tetris::controller::GameController& gc,
                           tetris::controller::InputAction action);

    void applyRemoteInputsHost();
    void stepOpponentAI(float dtSeconds);

    // ---------- SNAPSHOT (host -> StateUpdate) ----------
    static int colorIndexForTetromino(tetris::core::TetrominoType t);
    static tetris::net::BoardDTO makeBoardDTOFromGame(const tetris::core::GameState& gs,
                                                      bool includeActive);

    void broadcastSnapshotHost();

    // ---------- RENDER ----------
    void renderTopBar(Application& app, int w, int h) const;
    void renderTimeAttackLayout(Application& app, int w, int h);
    void renderSharedTurnsLayout(Application& app, int w, int h);

    static ImU32 colorFromIndex(int idx);

    void drawBoardDTO(ImDrawList* dl,
                      const tetris::net::BoardDTO& board,
                      ImVec2 topLeft,
                      float cell,
                      bool border) const;

    void drawBoardFromGame(ImDrawList* dl,
                           const tetris::core::GameState& gs,
                           ImVec2 topLeft,
                           float cell,
                           bool border,
                           bool drawActive) const;

    // --- Hold-to-repeat (same feel as SinglePlayer) ---
    float softDropHoldAccSec_ = 0.0f;
    float leftHoldSec_ = 0.0f;
    float rightHoldSec_ = 0.0f;
    float leftRepeatAccSec_ = 0.0f;
    float rightRepeatAccSec_ = 0.0f;

    float softDropRepeatSec_ = 0.05f; // 20 Hz
    float sideDasSec_ = 0.16f;        // delay before sideways repeat
    float sideArrSec_ = 0.05f;        // sideways repeat rate

    void applyHoldInputs(float dtSeconds);
};

} // namespace tetris::gui_sdl
