#include "gui_sdl/MultiplayerGameScreen.hpp"

#include <imgui.h>
#include <algorithm>
#include <random>
#include <mutex>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "controller/InputAction.hpp"

#include "network/NetworkHost.hpp"
#include "network/NetworkClient.hpp"

namespace tetris::gui_sdl {

static float clampf(float v, float a, float b) {
    return std::max(a, std::min(b, v));
}

// ------------------ ctor ------------------

MultiplayerGameScreen::MultiplayerGameScreen(const tetris::net::MultiplayerConfig& cfg,
                                             std::shared_ptr<tetris::net::NetworkHost> host,
                                             std::shared_ptr<tetris::net::NetworkClient> client)
    : cfg_(cfg)
    , host_(std::move(host))
    , client_(std::move(client))
    , localGame_(20, 10, 0)
    , localCtrl_(localGame_)
    , oppGame_(20, 10, 0)
    , oppCtrl_(oppGame_)
    , sharedGame_(20, 10, 0)
    , sharedCtrl_(sharedGame_)
{
    // We are "host/offline" if we are NOT a network client
    isHost_ = (client_ == nullptr); 

    // If we are a client, ensure we have started join handshake somewhere.
    // (Safe even if already joined)
    if (client_) {
        client_->setStateUpdateHandler([this](const tetris::net::StateUpdate& s) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            lastState_ = s;
        });
        if (!client_->isJoined()) client_->start();

        client_->setMatchResultHandler([this](const tetris::net::MatchResult& r) {
            // Client receives its own result from host
            matchEnded_ = true;
            localMatchResult_ = r;

            // Stop any "waiting rematch start" if match ended normally
            waitingRematchStart_ = false;
        });
    }

    // If host exists, start match immediately for testing
    if (host_ && cfg_.isHost && !host_->isMatchStarted()) {
        host_->startMatch();
    }

    // Core starts (used by host/offline)
    localGame_.start();  localCtrl_.resetTiming();
    oppGame_.start();    oppCtrl_.resetTiming();
    sharedGame_.start(); sharedCtrl_.resetTiming();
}

// ------------------ input mapping ------------------

std::optional<tetris::controller::InputAction>
MultiplayerGameScreen::actionFromKey(SDL_Keycode key)
{
    using tetris::controller::InputAction;
    switch (key) {
        case SDLK_LEFT:
        case SDLK_a: return InputAction::MoveLeft;

        case SDLK_RIGHT:
        case SDLK_d: return InputAction::MoveRight;

        case SDLK_DOWN:
        case SDLK_s: return InputAction::SoftDrop;

        case SDLK_SPACE: return InputAction::HardDrop;

        case SDLK_z:
        case SDLK_q: return InputAction::RotateCCW;

        case SDLK_x:
        case SDLK_e: return InputAction::RotateCW;

        case SDLK_p:
        case SDLK_ESCAPE: return InputAction::PauseResume;

        default: return std::nullopt;
    }
}

void MultiplayerGameScreen::sendOrApplyAction(tetris::core::GameState& gs,
                                             tetris::controller::GameController& gc,
                                             tetris::controller::InputAction action)
{
    // Client authoritative: send to host
    if (client_) {
        client_->sendInput(action, static_cast<tetris::net::Tick>(clientTick_++));
        return;
    }

    // Host/offline: apply locally
    (void)gs;
    gc.handleAction(action);
}

void MultiplayerGameScreen::handleEvent(Application& app, const SDL_Event& e)
{
    if (e.type != SDL_KEYDOWN || e.key.repeat != 0)
        return;

    const SDL_Keycode key = e.key.keysym.sym;

    if (key == SDLK_BACKSPACE) {
        app.setScreen(std::make_unique<StartScreen>());
        return;
    }

    // If match ended, ignore gameplay inputs (still allow Backspace above)
    if (matchEnded_) return;

    // choose which local state/controller is "ours"
    tetris::core::GameState* gs = nullptr;
    tetris::controller::GameController* gc = nullptr;

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        gs = &localGame_;
        gc = &localCtrl_;
    } else {
        gs = &sharedGame_;
        gc = &sharedCtrl_;
    }
    if (!gs || !gc) return;

    auto act = actionFromKey(key);
    if (!act) return;

    // Pause/resume should work anytime (host applies / client sends)
    if (*act == tetris::controller::InputAction::PauseResume) {
        sendOrApplyAction(*gs, *gc, *act);
        return;
    }

    // gameplay only when running (host has local status; client will still send holds in update)
    if (!client_ && gs->status() != tetris::core::GameStatus::Running)
        return;

    // first tap
    sendOrApplyAction(*gs, *gc, *act);
}

// ------------------ host consumes remote inputs ------------------

void MultiplayerGameScreen::applyRemoteInputsHost()
{
    if (!host_ || !cfg_.isHost) return;

    host_->poll();
    auto inputs = host_->consumeInputQueue();
    if (inputs.empty()) return;

    for (const auto& m : inputs) {
        // In TimeAttack: remote controls opponent game
        if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
            oppCtrl_.handleAction(m.action);
        } else {
            // SharedTurns: both affect the same shared board
            sharedCtrl_.handleAction(m.action);
        }
    }
}

// ------------------ offline AI (optional) ------------------

void MultiplayerGameScreen::stepOpponentAI(float dtSeconds)
{
    if (host_ || client_) return;
    if (cfg_.mode != tetris::net::GameMode::TimeAttack) return;
    if (oppGame_.status() != tetris::core::GameStatus::Running) return;

    oppInputAcc_ += dtSeconds;
    if (oppInputAcc_ < 0.20f) return;
    oppInputAcc_ = 0.0f;

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 9);
    int r = dist(rng);

    using tetris::controller::InputAction;
    if (r <= 3) oppCtrl_.handleAction(InputAction::MoveLeft);
    else if (r <= 7) oppCtrl_.handleAction(InputAction::MoveRight);
    else if (r == 8) oppCtrl_.handleAction(InputAction::RotateCW);
    else oppCtrl_.handleAction(InputAction::SoftDrop);
}

// ------------------ DTO mapping (host snapshot) ------------------

int MultiplayerGameScreen::colorIndexForTetromino(tetris::core::TetrominoType t)
{
    // 1..7 for I,O,T,J,L,S,Z. 0 = grey
    using TT = tetris::core::TetrominoType;
    switch (t) {
        case TT::I: return 1;
        case TT::O: return 2;
        case TT::T: return 3;
        case TT::J: return 4;
        case TT::L: return 5;
        case TT::S: return 6;
        case TT::Z: return 7;
    }
    return 0;
}

tetris::net::BoardDTO
MultiplayerGameScreen::makeBoardDTOFromGame(const tetris::core::GameState& gs, bool includeActive)
{
    tetris::net::BoardDTO dto;
    const auto& b = gs.board();

    dto.width  = b.cols();
    dto.height = b.rows();
    dto.cells.resize(static_cast<std::size_t>(dto.width * dto.height));

    auto idx = [&](int r, int c) -> std::size_t {
        return static_cast<std::size_t>(r * dto.width + c);
    };

    // Locked cells: occupied, but no color info in your core => grey (0)
    for (int r = 0; r < dto.height; ++r) {
        for (int c = 0; c < dto.width; ++c) {
            auto& cell = dto.cells[idx(r,c)];
            cell.occupied = (b.cell(r,c) == tetris::core::CellState::Filled);
            cell.colorIndex = cell.occupied ? 0 : 0;
        }
    }

    // Active piece: we can color it!
    if (includeActive && gs.activeTetromino().has_value()) {
        const auto& t = *gs.activeTetromino();
        const int colIdx = colorIndexForTetromino(t.type());
        for (const auto& blk : t.blocks()) {
            if (blk.row < 0) continue;
            if (blk.row >= dto.height || blk.col < 0 || blk.col >= dto.width) continue;
            auto& cell = dto.cells[idx(blk.row, blk.col)];
            cell.occupied = true;
            cell.colorIndex = colIdx;
        }
    }

    return dto;
}

void MultiplayerGameScreen::broadcastSnapshotHost()
{
    if (!host_ || !cfg_.isHost) return;

    tetris::net::StateUpdate su;
    su.serverTick = serverTick_;

    // TimeAttack: send remaining time
    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        const float left = std::max(0.0f, static_cast<float>(cfg_.timeLimitSeconds) - matchElapsedSec_);
        su.timeLeftSeconds = static_cast<std::uint32_t>(left + 0.999f); // ceil-ish
    } else {
        su.timeLeftSeconds = 0;
    }

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        // Player 1 = host(local), Player 2 = remote(opponent)
        tetris::net::PlayerStateDTO p1;
        p1.id = 1;
        p1.name = "Host";
        p1.board = makeBoardDTOFromGame(localGame_, true);
        p1.score = static_cast<int>(localGame_.score());
        p1.level = localGame_.level();
        p1.isAlive = (localGame_.status() != tetris::core::GameStatus::GameOver);

        tetris::net::PlayerStateDTO p2;
        p2.id = 2;
        p2.name = "Client";
        p2.board = makeBoardDTOFromGame(oppGame_, true);
        p2.score = static_cast<int>(oppGame_.score());
        p2.level = oppGame_.level();
        p2.isAlive = (oppGame_.status() != tetris::core::GameStatus::GameOver);

        su.players.push_back(std::move(p1));
        su.players.push_back(std::move(p2));
    } else {
        // SharedTurns: same board snapshot for both
        auto board = makeBoardDTOFromGame(sharedGame_, true);

        tetris::net::PlayerStateDTO p1;
        p1.id = 1;
        p1.name = "Host";
        p1.board = board;
        p1.score = static_cast<int>(sharedGame_.score());
        p1.level = sharedGame_.level();
        p1.isAlive = (sharedGame_.status() != tetris::core::GameStatus::GameOver);

        tetris::net::PlayerStateDTO p2 = p1;
        p2.id = 2;
        p2.name = "Client";

        su.players.push_back(std::move(p1));
        su.players.push_back(std::move(p2));
    }

    tetris::net::Message msg;
    msg.kind = tetris::net::MessageKind::StateUpdate;
    msg.payload = std::move(su);

    host_->broadcast(msg);
}

// ------------------ update loop ------------------

void MultiplayerGameScreen::update(Application&, float dtSeconds)
{
    // ---------------- CLIENT ----------------
    // Client does NOT run local physics.
    // It only:
    //  - sends hold-repeat inputs
    //  - reads snapshots from host
    if (!cfg_.isHost) {
        applyHoldInputs(dtSeconds);

        if (client_) {
            // Grab latest snapshot (thread-safe inside NetworkClient)
            auto up = client_->lastStateUpdate();
            if (up) {
                std::lock_guard<std::mutex> lock(stateMutex_);
                lastState_ = std::move(*up);
            }
        }
        return;
    }

    // ---------------- HOST ----------------
    // IMPORTANT (3.2): keep polling ALWAYS so we can:
    //  - detect disconnects (client left)
    //  - receive JoinRequest used as "rematch ready"
    if (host_) {
        host_->poll();
    }

    matchElapsedSec_ += dtSeconds;

    // If match ended, keep broadcasting snapshots so client stays visually consistent,
    // but do not run physics anymore.
    if (matchEnded_) {
        snapshotAccSec_ += dtSeconds;
        while (snapshotAccSec_ >= snapshotPeriodSec_) {
            snapshotAccSec_ -= snapshotPeriodSec_;
            ++serverTick_;
            broadcastSnapshotHost();
        }
        return;
    }

    // convert dt to ms for core controllers
    const int ms = static_cast<int>(dtSeconds * 1000.0f + 0.5f);
    auto dur = tetris::controller::GameController::Duration{ms};

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        // Host updates both games
        localCtrl_.update(dur);
        oppCtrl_.update(dur);

        // Consume client inputs -> apply to opponent game
        if (host_) {
            auto inputs = host_->consumeInputQueue();
            for (const auto& m : inputs) {
                (void)m.playerId; // for now: 1 client -> opponent
                oppCtrl_.handleAction(m.action);
            }
        }

        // IMPORTANT: if you're using network, DON'T run offline AI
        // stepOpponentAI(dtSeconds);

    } else {
        // SharedTurns: one shared board
        sharedCtrl_.update(dur);

        if (host_) {
            auto inputs = host_->consumeInputQueue();
            for (const auto& m : inputs) {
                (void)m.playerId;
                sharedCtrl_.handleAction(m.action);
            }
        }
    }

    // Host also gets hold-to-repeat locally
    applyHoldInputs(dtSeconds);

    // Check end-of-match conditions (time limit / game over)
    tryFinalizeMatchHost();

    // --- Broadcast authoritative snapshot to clients (20 Hz) ---
    snapshotAccSec_ += dtSeconds;
    while (snapshotAccSec_ >= snapshotPeriodSec_) {
        snapshotAccSec_ -= snapshotPeriodSec_;
        ++serverTick_;
        broadcastSnapshotHost();
    }
}

// ------------------ rendering helpers ------------------

ImU32 MultiplayerGameScreen::colorFromIndex(int idx)
{
    // 0 grey, 1..7 = I,O,T,J,L,S,Z
    switch (idx) {
        case 1: return IM_COL32(  0, 255, 255, 255); // I
        case 2: return IM_COL32(255, 255,   0, 255); // O
        case 3: return IM_COL32(160,  32, 240, 255); // T
        case 4: return IM_COL32(  0,   0, 255, 255); // J
        case 5: return IM_COL32(255, 165,   0, 255); // L
        case 6: return IM_COL32(  0, 255,   0, 255); // S
        case 7: return IM_COL32(255,   0,   0, 255); // Z
        default: return IM_COL32(120, 120, 130, 255);
    }
}

void MultiplayerGameScreen::drawBoardDTO(ImDrawList* dl, const tetris::net::BoardDTO& board,
                                        ImVec2 topLeft, float cell, bool border) const
{
    const int rows = board.height;
    const int cols = board.width;

    ImVec2 size(cols * cell, rows * cell);
    ImU32 bg = IM_COL32(12, 12, 16, 255);
    ImU32 grid = IM_COL32(40, 40, 55, 255);

    dl->AddRectFilled(topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y), bg, 6.0f);

    for (int r = 0; r <= rows; ++r) {
        float y = topLeft.y + r * cell;
        dl->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + size.x, y), grid);
    }
    for (int c = 0; c <= cols; ++c) {
        float x = topLeft.x + c * cell;
        dl->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + size.y), grid);
    }

    auto idx = [&](int r, int c)->std::size_t {
        return static_cast<std::size_t>(r * cols + c);
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const auto& cellDto = board.cells[idx(r,c)];
            if (!cellDto.occupied) continue;
            ImU32 col = colorFromIndex(cellDto.colorIndex);
            ImVec2 p0(topLeft.x + c * cell + 1, topLeft.y + r * cell + 1);
            ImVec2 p1(p0.x + cell - 2, p0.y + cell - 2);
            dl->AddRectFilled(p0, p1, col);
            dl->AddRect(p0, p1, IM_COL32(20, 20, 20, 255));
        }
    }

    if (border) {
        dl->AddRect(topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y),
                    IM_COL32(120,120,130,255), 6.0f, 0, 2.0f);
    }
}

void MultiplayerGameScreen::drawBoardFromGame(ImDrawList* dl, const tetris::core::GameState& gs,
                                             ImVec2 topLeft, float cell, bool border, bool drawActive) const
{
    // Reuse your previous drawing style: locked grey + active colored
    const auto& b = gs.board();
    const int rows = b.rows();
    const int cols = b.cols();

    ImVec2 size(cols * cell, rows * cell);
    ImU32 bg = IM_COL32(12, 12, 16, 255);
    ImU32 grid = IM_COL32(40, 40, 55, 255);
    ImU32 locked = IM_COL32(90, 90, 95, 255);

    dl->AddRectFilled(topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y), bg, 6.0f);

    for (int r = 0; r <= rows; ++r) {
        float y = topLeft.y + r * cell;
        dl->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + size.x, y), grid);
    }
    for (int c = 0; c <= cols; ++c) {
        float x = topLeft.x + c * cell;
        dl->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + size.y), grid);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (b.cell(r, c) == tetris::core::CellState::Filled) {
                ImVec2 p0(topLeft.x + c * cell + 1, topLeft.y + r * cell + 1);
                ImVec2 p1(p0.x + cell - 2, p0.y + cell - 2);
                dl->AddRectFilled(p0, p1, locked);
            }
        }
    }

    if (drawActive && gs.activeTetromino().has_value()) {
        const auto& t = *gs.activeTetromino();
        ImU32 col = colorFromIndex(colorIndexForTetromino(t.type()));
        for (const auto& blk : t.blocks()) {
            if (blk.row < 0) continue;
            ImVec2 p0(topLeft.x + blk.col * cell + 1, topLeft.y + blk.row * cell + 1);
            ImVec2 p1(p0.x + cell - 2, p0.y + cell - 2);
            dl->AddRectFilled(p0, p1, col);
            dl->AddRect(p0, p1, IM_COL32(20, 20, 20, 255));
        }
    }

    if (border) {
        dl->AddRect(topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y),
                    IM_COL32(120,120,130,255), 6.0f, 0, 2.0f);
    }
}

// ------------------ render ------------------

void MultiplayerGameScreen::render(Application& app)
{
    int w = 0, h = 0;
    app.getWindowSize(w, h);

    renderTopBar(app, w, h);

    if (cfg_.mode == tetris::net::GameMode::TimeAttack)
        renderTimeAttackLayout(app, w, h);
    else
        renderSharedTurnsLayout(app, w, h);
    
    renderMatchOverlay(app, w, h);
}

void MultiplayerGameScreen::renderTopBar(Application& app, int w, int h) const
{
    (void)app; (void)h;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w - 20), 52), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("##mp_topbar", nullptr, flags);

    std::optional<tetris::net::StateUpdate> snap;
    if (client_) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        snap = lastState_;
    }

    std::uint32_t timeLeft = 0;
    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        if (client_ && snap) {
            timeLeft = snap->timeLeftSeconds;
        } else {
            // host/offline: compute locally
            const float left = std::max(0.0f, static_cast<float>(cfg_.timeLimitSeconds) - matchElapsedSec_);
            timeLeft = static_cast<std::uint32_t>(left + 0.999f);
        }
    }

    ImGui::Text("Role: %s  |  Network: %s",
                cfg_.isHost ? "Host" : "Client",
                (host_ || client_) ? "ON" : "OFF");

    // ---- TimeAttack timer (authoritative on host, replicated to client) ----
    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        std::uint32_t leftSec = 0;

        if (client_) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (lastState_) leftSec = lastState_->timeLeftSeconds;
        } else {
            const float left = std::max(
                0.0f,
                static_cast<float>(cfg_.timeLimitSeconds) - matchElapsedSec_
            );
            leftSec = static_cast<std::uint32_t>(left + 0.999f);
        }

        const std::uint32_t mm = leftSec / 60;
        const std::uint32_t ss = leftSec % 60;

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 260);
        ImGui::Text("Time left: %02u:%02u", mm, ss);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120);
    ImGui::TextUnformatted("Backspace: Menu");

    ImGui::End();
}

void MultiplayerGameScreen::renderTimeAttackLayout(Application& app, int w, int h)
{
    (void)app;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float top = 70.0f;
    const float margin = 18.0f;

    float availableW = static_cast<float>(w) - margin * 3.0f;
    float availableH = static_cast<float>(h) - top - margin;

    const float panelW = 320.0f;
    const float boardsW = availableW - panelW - margin;
    const float boardW = boardsW * 0.5f - margin * 0.5f;

    int cols = 10, rows = 20;
    float cell = std::min(boardW / cols, availableH / rows);
    cell = clampf(cell, 16.0f, 42.0f);

    float boardPxW = cols * cell;
    float boardPxH = rows * cell;

    float x0 = (static_cast<float>(w) - (boardPxW * 2 + margin + panelW + margin)) * 0.5f;
    if (x0 < margin) x0 = margin;

    float y0 = top + (availableH - boardPxH) * 0.5f;
    if (y0 < top) y0 = top;

    // Decide data source: client -> DTO, else -> GameState
    std::optional<tetris::net::StateUpdate> snap;
    if (client_) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        snap = lastState_;
    }

    dl->AddText(ImVec2(x0, y0 - 22), IM_COL32_WHITE, "You");
    dl->AddText(ImVec2(x0 + boardPxW + margin, y0 - 22), IM_COL32_WHITE, "Opponent");

    if (client_ && snap && snap->players.size() >= 2) {
        // assume players[0]=host, players[1]=client (as sent)
        // client sees itself on left for now (simple)
        drawBoardDTO(dl, snap->players[1].board, ImVec2(x0, y0), cell, true);
        drawBoardDTO(dl, snap->players[0].board, ImVec2(x0 + boardPxW + margin, y0), cell, true);
    } else {
        drawBoardFromGame(dl, localGame_, ImVec2(x0, y0), cell, true, true);
        drawBoardFromGame(dl, oppGame_,   ImVec2(x0 + boardPxW + margin, y0), cell, true, true);
    }

    float panelX = x0 + boardPxW * 2 + margin * 2;
    float panelY = y0;

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, boardPxH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("Scoreboard", nullptr, flags);

    ImGui::SeparatorText("YOU");
    if (client_ && snap && snap->players.size() >= 2) {
        ImGui::Text("Score: %d", snap->players[1].score);
        ImGui::Text("Level: %d", snap->players[1].level);
        ImGui::Text("Alive: %s", snap->players[1].isAlive ? "Yes" : "No");
    } else {
        ImGui::Text("Score: %llu", (unsigned long long)localGame_.score());
        ImGui::Text("Level: %d", localGame_.level());
        ImGui::Text("Alive: %s", localGame_.status() != tetris::core::GameStatus::GameOver ? "Yes" : "No");
    }

    ImGui::Spacing();
    ImGui::SeparatorText("OPPONENT");
    if (client_ && snap && snap->players.size() >= 2) {
        ImGui::Text("Score: %d", snap->players[0].score);
        ImGui::Text("Level: %d", snap->players[0].level);
        ImGui::Text("Alive: %s", snap->players[0].isAlive ? "Yes" : "No");
    } else {
        ImGui::Text("Score: %llu", (unsigned long long)oppGame_.score());
        ImGui::Text("Level: %d", oppGame_.level());
        ImGui::Text("Alive: %s", oppGame_.status() != tetris::core::GameStatus::GameOver ? "Yes" : "No");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled(client_ ? "Client: authoritative StateUpdate." : "Host/offline: core-driven.");

    ImGui::End();
}

void MultiplayerGameScreen::renderSharedTurnsLayout(Application& app, int w, int h)
{
    (void)app;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float top = 70.0f;
    const float margin = 18.0f;

    float availableW = static_cast<float>(w) - margin * 2.0f;
    float availableH = static_cast<float>(h) - top - margin * 2.0f - 120.0f;

    int cols = 10, rows = 20;
    float cell = std::min(availableW / cols, availableH / rows);
    cell = clampf(cell, 16.0f, 42.0f);

    float boardPxW = cols * cell;
    float boardPxH = rows * cell;

    float x0 = (static_cast<float>(w) - boardPxW) * 0.5f;
    float y0 = top + (availableH - boardPxH) * 0.5f;
    if (y0 < top) y0 = top;

    std::optional<tetris::net::StateUpdate> snap;
    if (client_) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        snap = lastState_;
    }

    dl->AddText(ImVec2(x0 + boardPxW * 0.5f - 70, y0 - 22), IM_COL32_WHITE, "Shared Board");

    if (client_ && snap && !snap->players.empty()) {
        // shared board is same for both; render first player's board
        drawBoardDTO(dl, snap->players[0].board, ImVec2(x0, y0), cell, true);
    } else {
        drawBoardFromGame(dl, sharedGame_, ImVec2(x0, y0), cell, true, true);
    }

    // cards
    float cardY = y0 + boardPxH + margin;
    float cardH = 110.0f;
    float cardW = (static_cast<float>(w) - margin * 3.0f) * 0.5f;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowPos(ImVec2(margin, cardY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGui::Begin("You##shared", nullptr, flags);

    if (client_ && snap && snap->players.size() >= 2) {
        ImGui::Text("Score: %d", snap->players[1].score);
        ImGui::Text("Level: %d", snap->players[1].level);
        ImGui::TextDisabled("Client (authoritative render).");
    } else {
        ImGui::Text("Score: %llu", (unsigned long long)sharedGame_.score());
        ImGui::Text("Level: %d", sharedGame_.level());
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(margin * 2 + cardW, cardY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGui::Begin("Opponent##shared", nullptr, flags);

    if (client_ && snap && snap->players.size() >= 2) {
        ImGui::Text("Score: %d", snap->players[0].score);
        ImGui::Text("Level: %d", snap->players[0].level);
        ImGui::TextDisabled("Opponent snapshot.");
    } else {
        ImGui::Text("Score: %llu", (unsigned long long)sharedGame_.score());
        ImGui::Text("Level: %d", sharedGame_.level());
        ImGui::TextDisabled("(will come from StateUpdate)");
    }
    ImGui::End();
}

// ------------------ hold inputs (client sends / host applies) ------------------

void MultiplayerGameScreen::applyHoldInputs(float dtSeconds)
{
    tetris::core::GameState* gs = nullptr;
    tetris::controller::GameController* gc = nullptr;

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) { gs = &localGame_; gc = &localCtrl_; }
    else { gs = &sharedGame_; gc = &sharedCtrl_; }

    if (!gs || !gc) return;

    // Client: allow sending hold repeats even without local Running state
    if (!client_) {
        if (gs->status() != tetris::core::GameStatus::Running) {
            softDropHoldAccSec_ = 0.0f;
            leftHoldSec_ = rightHoldSec_ = 0.0f;
            leftRepeatAccSec_ = rightRepeatAccSec_ = 0.0f;
            return;
        }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    const bool downHeld = (keys[SDL_SCANCODE_DOWN] != 0) || (keys[SDL_SCANCODE_S] != 0);
    if (!downHeld) {
        softDropHoldAccSec_ = 0.0f;
    } else {
        softDropHoldAccSec_ += dtSeconds;
        while (softDropHoldAccSec_ >= softDropRepeatSec_) {
            sendOrApplyAction(*gs, *gc, tetris::controller::InputAction::SoftDrop);
            softDropHoldAccSec_ -= softDropRepeatSec_;
        }
    }

    const bool leftHeld  = (keys[SDL_SCANCODE_LEFT] != 0) || (keys[SDL_SCANCODE_A] != 0);
    const bool rightHeld = (keys[SDL_SCANCODE_RIGHT] != 0) || (keys[SDL_SCANCODE_D] != 0);

    if (leftHeld && rightHeld) {
        leftHoldSec_ = rightHoldSec_ = 0.0f;
        leftRepeatAccSec_ = rightRepeatAccSec_ = 0.0f;
        return;
    }

    if (!leftHeld && !rightHeld) {
        leftHoldSec_ = rightHoldSec_ = 0.0f;
        leftRepeatAccSec_ = rightRepeatAccSec_ = 0.0f;
        return;
    }

    if (leftHeld) {
        leftHoldSec_ += dtSeconds;
        if (leftHoldSec_ >= sideDasSec_) {
            leftRepeatAccSec_ += dtSeconds;
            while (leftRepeatAccSec_ >= sideArrSec_) {
                sendOrApplyAction(*gs, *gc, tetris::controller::InputAction::MoveLeft);
                leftRepeatAccSec_ -= sideArrSec_;
            }
        } else {
            leftRepeatAccSec_ = 0.0f;
        }
        rightHoldSec_ = 0.0f; rightRepeatAccSec_ = 0.0f;
    }

    if (rightHeld) {
        rightHoldSec_ += dtSeconds;
        if (rightHoldSec_ >= sideDasSec_) {
            rightRepeatAccSec_ += dtSeconds;
            while (rightRepeatAccSec_ >= sideArrSec_) {
                sendOrApplyAction(*gs, *gc, tetris::controller::InputAction::MoveRight);
                rightRepeatAccSec_ -= sideArrSec_;
            }
        } else {
            rightRepeatAccSec_ = 0.0f;
        }
        leftHoldSec_ = 0.0f; leftRepeatAccSec_ = 0.0f;
    }
}

void MultiplayerGameScreen::tryFinalizeMatchHost()
{
    if (!cfg_.isHost || !host_) return;
    if (matchEnded_) return;

    // Only implement match logic for TimeAttack for now
    if (cfg_.mode != tetris::net::GameMode::TimeAttack) return;

    // Track time
    // (dt is accumulated in update via matchElapsedSec_ increment; if you didn't do it yet,
    // you can increment in update; easiest is here using snapshotPeriod accumulator isn't correct.)
    // We'll assume you increment matchElapsedSec_ in update by dtSeconds. If not, do it now:
    // NOTE: If you prefer, move this increment to update() once per frame.
    // We'll do it here safely by using ImGui delta is not available; so keep it in update:
    // matchElapsedSec_ += dtSeconds;  <-- Do this in update before calling tryFinalizeMatchHost()

    const bool hostDead = (localGame_.status() == tetris::core::GameStatus::GameOver);
    const bool oppDead  = (oppGame_.status()   == tetris::core::GameStatus::GameOver);

    const bool timeLimitEnabled = (cfg_.timeLimitSeconds > 0);
    const bool timeUp = timeLimitEnabled && (matchElapsedSec_ >= static_cast<float>(cfg_.timeLimitSeconds));

    // End conditions:
    // - time is up
    // - someone died
    // - both died
    if (!timeUp && !hostDead && !oppDead) {
        return;
    }

    matchEnded_ = true;
    ++serverTick_;

    const int hostScore = static_cast<int>(localGame_.score());
    const int oppScore  = static_cast<int>(oppGame_.score());

    // Decide winner:
    // Priority: if exactly one died -> the other wins.
    // Else (timeUp or both died) -> higher score wins, tie -> draw.
    tetris::net::MatchOutcome hostOutcome = tetris::net::MatchOutcome::Draw;
    tetris::net::MatchOutcome oppOutcome  = tetris::net::MatchOutcome::Draw;

    if (hostDead && !oppDead) {
        hostOutcome = tetris::net::MatchOutcome::Lose;
        oppOutcome  = tetris::net::MatchOutcome::Win;
    } else if (!hostDead && oppDead) {
        hostOutcome = tetris::net::MatchOutcome::Win;
        oppOutcome  = tetris::net::MatchOutcome::Lose;
    } else {
        // timeUp or both dead
        if (hostScore > oppScore) {
            hostOutcome = tetris::net::MatchOutcome::Win;
            oppOutcome  = tetris::net::MatchOutcome::Lose;
        } else if (hostScore < oppScore) {
            hostOutcome = tetris::net::MatchOutcome::Lose;
            oppOutcome  = tetris::net::MatchOutcome::Win;
        } else {
            hostOutcome = tetris::net::MatchOutcome::Draw;
            oppOutcome  = tetris::net::MatchOutcome::Draw;
        }
    }

    // Store local host result (for overlay)
    tetris::net::MatchResult hostRes;
    hostRes.endTick = serverTick_;
    hostRes.playerId = 1; // matches your "Host" id in snapshots
    hostRes.outcome = hostOutcome;
    hostRes.finalScore = hostScore;
    localMatchResult_ = hostRes;

    // Send client its result
    tetris::net::MatchResult clientRes;
    clientRes.endTick = serverTick_;
    clientRes.playerId = 2; // matches your "Client" id in snapshots
    clientRes.outcome = oppOutcome;      // client sees from their perspective
    clientRes.finalScore = oppScore;
    clientMatchResult_ = clientRes;

    tetris::net::Message msg;
    msg.kind = tetris::net::MessageKind::MatchResult;
    msg.payload = clientRes;
    host_->broadcast(msg);
}

void MultiplayerGameScreen::renderMatchOverlay(Application& app, int w, int h)
{
    if (!matchEnded_) return;
    if (!localMatchResult_) return;

    auto outcomeToText = [](tetris::net::MatchOutcome o) -> const char* {
        switch (o) {
            case tetris::net::MatchOutcome::Win:  return "YOU WIN!";
            case tetris::net::MatchOutcome::Lose: return "YOU LOSE";
            case tetris::net::MatchOutcome::Draw: return "DRAW";
        }
        return "MATCH OVER";
    };

    const char* title = outcomeToText(localMatchResult_->outcome);

    // no full-screen dim
    const ImVec2 size(500, 320);
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("Match Result", nullptr, flags);

    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.35f);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();
    ImGui::Text("Final score: %d", localMatchResult_->finalScore);

    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        ImGui::Text("Time limit: %u s", cfg_.timeLimitSeconds);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // -------- Rematch logic --------
    bool opponentPresent = true;
    bool opponentReady = false;

    if (cfg_.isHost) {
        opponentPresent = host_ && host_->hasAnyConnectedClient();
        opponentReady   = host_ && host_->anyClientReadyForRematch();

        if (!opponentPresent) {
            ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "Opponent left. Rematch unavailable.");
        } else {
            ImGui::Text("Opponent rematch: %s", opponentReady ? "READY" : "NOT READY");
        }

        ImGui::Spacing();

        ImGui::BeginDisabled(!opponentPresent);
        if (ImGui::Button(hostWantsRematch_ ? "Rematch: READY (click to cancel)" : "Rematch (I am ready)", ImVec2(-1, 42))) {
            hostWantsRematch_ = !hostWantsRematch_;
        }
        ImGui::EndDisabled();

        // If both agreed, restart match
        if (opponentPresent && hostWantsRematch_ && opponentReady && host_) {
            // reset host core state
            localGame_.reset(); localGame_.start(); localCtrl_.resetTiming();
            oppGame_.reset();   oppGame_.start();   oppCtrl_.resetTiming();
            sharedGame_.reset(); sharedGame_.start(); sharedCtrl_.resetTiming();

            matchEnded_ = false;
            matchElapsedSec_ = 0.0f;
            localMatchResult_.reset();
            clientMatchResult_.reset();
            hostWantsRematch_ = false;

            // clear rematch flags on host so next rematch requires fresh agreement
            host_->clearRematchFlags();

            // resend StartGame to clients
            host_->startMatch();
        }
    } else {
        // CLIENT
        // If client clicked rematch, it sends JoinRequest again (NetworkClient::start()).
        // Host interprets it as readyForRematch.
        if (client_) {
            // If host already restarted, client will receive StartGame.
            auto sg = client_->lastStartGame();
            if (sg && waitingRematchStart_) {
                // new match started
                matchEnded_ = false;
                matchElapsedSec_ = 0.0f;
                localMatchResult_.reset();
                waitingRematchStart_ = false;
                clientWantsRematch_ = false;

                // optional: clear last snapshots so UI doesn't show stale
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    lastState_.reset();
                }
            }
        }

        if (waitingRematchStart_) {
            ImGui::TextDisabled("Waiting host to restart...");
        }

        if (ImGui::Button(clientWantsRematch_ ? "Rematch requested" : "Rematch (request)", ImVec2(-1, 42))) {
            if (client_ && !clientWantsRematch_) {
                clientWantsRematch_ = true;
                waitingRematchStart_ = true;
                client_->start(); // resend JoinRequest => host marks readyForRematch
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Exit: if client exits, TcpSession will close => host sees disconnect
    if (ImGui::Button("Back to Menu", ImVec2(-1, 42))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    ImGui::End();
}

} // namespace tetris::gui_sdl
