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

std::uint32_t MultiplayerGameScreen::boardHash(const tetris::core::Board& b)
{
    std::uint32_t h = 2166136261u; // FNV-1a
    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            const bool occ = (b.cell(r, c) == tetris::core::CellState::Filled);
            std::uint32_t v = occ ? 1u : 0u;
            if (occ) {
                if (auto t = b.cellType(r, c)) {
                    v = 10u + static_cast<std::uint32_t>(*t);
                }
            }
            h ^= (v + 31u * (std::uint32_t)r + 131u * (std::uint32_t)c);
            h *= 16777619u;
        }
    }
    return h;
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
    isHost_ = (client_ == nullptr);

    if (client_) {
        client_->setStateUpdateHandler([this](const tetris::net::StateUpdate& s) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            lastState_ = s;
        });
        if (!client_->isJoined()) client_->start();

        client_->setMatchResultHandler([this](const tetris::net::MatchResult& r) {
            matchEnded_ = true;
            localMatchResult_ = r;
            waitingRematchStart_ = false;
        });
    }

    if (host_ && cfg_.isHost && !host_->isMatchStarted()) {
        host_->startMatch();
    }

    localGame_.start();  localCtrl_.resetTiming();
    oppGame_.start();    oppCtrl_.resetTiming();
    sharedGame_.start(); sharedCtrl_.resetTiming();

    turnPlayerId_ = 1;
    piecesLeftThisTurn_ = (cfg_.piecesPerTurn > 0 ? cfg_.piecesPerTurn : 1);
    lastActionPlayerId_ = turnPlayerId_;

    boardHashInit_ = false;
    lastBoardHash_ = 0;
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
    if (client_) {
        client_->sendInput(action, static_cast<tetris::net::Tick>(clientTick_++));
        return;
    }

    if (cfg_.isHost && cfg_.mode == tetris::net::GameMode::SharedTurns) {
        const tetris::net::PlayerId selfId = tetris::net::NetworkHost::HostPlayerId;
        if (selfId != turnPlayerId_) {
            return;
        }
    }

    (void)gs;

    if (cfg_.isHost && cfg_.mode == tetris::net::GameMode::SharedTurns) {
        lastActionPlayerId_ = tetris::net::NetworkHost::HostPlayerId;
    }

    gc.handleAction(action);
}

void MultiplayerGameScreen::handleEvent(Application& app, const SDL_Event& e)
{
    if (e.type != SDL_KEYDOWN || e.key.repeat != 0)
        return;

    const SDL_Keycode key = e.key.keysym.sym;

    if (key == SDLK_BACKSPACE) {
        if (cfg_.isHost && host_) {
            tetris::net::Message msg;
            msg.kind = tetris::net::MessageKind::PlayerLeft;

            tetris::net::PlayerLeft pl;
            pl.playerId = tetris::net::NetworkHost::HostPlayerId;
            pl.wasHost  = true;
            pl.reason   = "LEFT_TO_MENU";

            msg.payload = std::move(pl);
            host_->broadcast(msg);
        }
        app.setScreen(std::make_unique<StartScreen>());
        return;
    }

    if (matchEnded_) return;

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

    if (*act == tetris::controller::InputAction::PauseResume) {
        sendOrApplyAction(*gs, *gc, *act);
        return;
    }

    if (!client_ && gs->status() != tetris::core::GameStatus::Running)
        return;

    if (cfg_.mode == tetris::net::GameMode::SharedTurns) {
        tetris::net::PlayerId myId = cfg_.isHost ? 1u : (client_ && client_->playerId() ? *client_->playerId() : 0u);

        tetris::net::PlayerId currentTurn = 0u;
        if (cfg_.isHost) {
            currentTurn = turnPlayerId_;
        } else {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (lastState_) currentTurn = lastState_->turnPlayerId;
        }

        if (myId == 0u || currentTurn == 0u || myId != currentTurn) {
            return;
        }
    }

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
        if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
            oppCtrl_.handleAction(m.action);
        } else {
            sharedCtrl_.handleAction(m.action);
        }
    }
}

void MultiplayerGameScreen::updateSharedTurnsTurnHost()
{
    if (!cfg_.isHost) return;
    if (cfg_.mode != tetris::net::GameMode::SharedTurns) return;

    if (piecesLeftThisTurn_ == 0) {
        piecesLeftThisTurn_ = std::max(1u, cfg_.piecesPerTurn);
        turnPlayerId_ = tetris::net::NetworkHost::HostPlayerId;
        boardHashInit_ = false;
    }

    const auto currentHash = boardHash(sharedGame_.board());
    if (!boardHashInit_) {
        boardHashInit_ = true;
        lastBoardHash_ = currentHash;
        return;
    }

    if (currentHash != lastBoardHash_) {
        lastBoardHash_ = currentHash;

        if (piecesLeftThisTurn_ > 0) {
            --piecesLeftThisTurn_;
        }

        if (piecesLeftThisTurn_ == 0) {
            const auto hostId = tetris::net::NetworkHost::HostPlayerId;
            const auto clientId = static_cast<tetris::net::PlayerId>(2);

            turnPlayerId_ = (turnPlayerId_ == hostId) ? clientId : hostId;
            piecesLeftThisTurn_ = std::max(1u, cfg_.piecesPerTurn);
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

    for (int r = 0; r < dto.height; ++r) {
        for (int c = 0; c < dto.width; ++c) {
            auto& cell = dto.cells[idx(r, c)];
            const bool occ = (b.cell(r, c) == tetris::core::CellState::Filled);
            cell.occupied = occ;

            if (!occ) {
                cell.colorIndex = 0;
                continue;
            }

            const auto t = b.cellType(r, c);
            cell.colorIndex = t ? colorIndexForTetromino(*t) : 0;
        }
    }

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

    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        const float total = static_cast<float>(cfg_.timeLimitSeconds);
        const float leftSec = std::max(0.0f, total - matchElapsedSec_);
        su.timeLeftMs = static_cast<std::uint32_t>(leftSec * 1000.0f + 0.5f);
    } else {
        su.timeLeftMs = 0;
    }

    su.turnPlayerId = turnPlayerId_;
    su.piecesLeftThisTurn = piecesLeftThisTurn_;

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        tetris::net::PlayerStateDTO pHost;
        pHost.id = tetris::net::NetworkHost::HostPlayerId;
        pHost.name = "Host";
        pHost.board = makeBoardDTOFromGame(localGame_, true);
        pHost.score = static_cast<int>(localGame_.score());
        pHost.level = localGame_.level();
        pHost.isAlive = (localGame_.status() != tetris::core::GameStatus::GameOver);

        tetris::net::PlayerStateDTO pClient;
        pClient.id = 2;
        pClient.name = "Client";
        pClient.board = makeBoardDTOFromGame(oppGame_, true);
        pClient.score = static_cast<int>(oppGame_.score());
        pClient.level = oppGame_.level();
        pClient.isAlive = (oppGame_.status() != tetris::core::GameStatus::GameOver);

        su.players.push_back(std::move(pHost));
        su.players.push_back(std::move(pClient));
    } else {
        auto board = makeBoardDTOFromGame(sharedGame_, true);

        tetris::net::PlayerStateDTO pHost;
        pHost.id = tetris::net::NetworkHost::HostPlayerId;
        pHost.name = "Host";
        pHost.board = board;
        pHost.score = static_cast<int>(sharedGame_.score());
        pHost.level = sharedGame_.level();
        pHost.isAlive = (sharedGame_.status() != tetris::core::GameStatus::GameOver);

        tetris::net::PlayerStateDTO pClient = pHost;
        pClient.id = 2;
        pClient.name = "Client";

        su.players.push_back(std::move(pHost));
        su.players.push_back(std::move(pClient));
    }

    tetris::net::Message msg;
    msg.kind = tetris::net::MessageKind::StateUpdate;
    msg.payload = std::move(su);

    host_->broadcast(msg);
}

// ------------------ update loop ------------------

void MultiplayerGameScreen::update(Application&, float dtSeconds)
{
    if (!cfg_.isHost) {
        applyHoldInputs(dtSeconds);

        bool gotSnapshot = false;

        if (client_) {
            if (auto pl = client_->consumePlayerLeft()) {
                if (pl->wasHost) {
                    hostDisconnected_ = true;
                    matchEnded_ = true;
                    waitingRematchStart_ = false;
                }
            }

            if (!client_->isConnected()) {
                hostDisconnected_ = true;
                matchEnded_ = true;
                waitingRematchStart_ = false;
            }

            if (auto sg = client_->consumeStartGame()) {
                (void)sg;

                matchEnded_ = false;
                hostDisconnected_ = false;
                opponentDisconnected_ = false;

                matchElapsedSec_ = 0.0f;
                localMatchResult_.reset();

                waitingRematchStart_ = false;
                clientWantsRematch_ = false;

                displayTimeLeftMs_ = 0;

                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    lastState_.reset();
                }

                timeSinceLastSnapshotSec_ = 0.0f;
            }

            if (auto up = client_->consumeStateUpdate()) {
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    lastState_ = *up;
                }
                gotSnapshot = true;

                displayTimeLeftMs_ = (displayTimeLeftMs_ == 0)
                    ? up->timeLeftMs
                    : static_cast<std::uint32_t>(displayTimeLeftMs_ * 0.80f + up->timeLeftMs * 0.20f);

                turnPlayerId_ = up->turnPlayerId;
                piecesLeftThisTurn_ = up->piecesLeftThisTurn;
            }

            if (!matchEnded_) {
                if (auto mr = client_->consumeMatchResult()) {
                    localMatchResult_ = *mr;
                    matchEnded_ = true;
                }
            }
        }

        const bool shouldTimeout = (!matchEnded_);

        if (shouldTimeout && !hostDisconnected_) {
            timeSinceLastSnapshotSec_ += dtSeconds;
            if (gotSnapshot) timeSinceLastSnapshotSec_ = 0.0f;

            if (timeSinceLastSnapshotSec_ >= snapshotTimeoutSec_) {
                hostDisconnected_ = true;
                matchEnded_ = true;
                waitingRematchStart_ = false;
            }
        }

        return;
    }

    if (host_) host_->poll();

    if (!matchEnded_ && host_ && !host_->hasAnyConnectedClient()) {
        opponentDisconnected_ = true;
        matchEnded_ = true;

        tetris::net::MatchResult hostRes;
        hostRes.endTick = ++serverTick_;
        hostRes.playerId = tetris::net::NetworkHost::HostPlayerId;
        hostRes.outcome = tetris::net::MatchOutcome::Win;
        hostRes.finalScore = static_cast<int>(localGame_.score());
        localMatchResult_ = hostRes;

        host_->onMatchFinished();
        return;
    }

    if (matchEnded_) return;

    const int ms = static_cast<int>(dtSeconds * 1000.0f + 0.5f);
    auto dur = tetris::controller::GameController::Duration{ms};

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        localCtrl_.update(dur);
        oppCtrl_.update(dur);

        if (host_) {
            auto inputs = host_->consumeInputQueue();
            for (const auto& m : inputs) {
                oppCtrl_.handleAction(m.action);
            }
        }

        matchElapsedSec_ += dtSeconds;
        tryFinalizeMatchHost();

    } else {
        sharedCtrl_.update(dur);

        if (host_) {
            auto inputs = host_->consumeInputQueue();
            for (const auto& m : inputs) {
                if (m.playerId != turnPlayerId_) continue;
                lastActionPlayerId_ = m.playerId;
                sharedCtrl_.handleAction(m.action);
            }
        }

        updateSharedTurnsTurnHost();

        if (sharedGame_.status() == tetris::core::GameStatus::GameOver) {
            matchEnded_ = true;
            ++serverTick_;

            const auto hostId = tetris::net::NetworkHost::HostPlayerId;
            const auto clientId = static_cast<tetris::net::PlayerId>(2);

            const tetris::net::PlayerId loser =
                (lastActionPlayerId_ == hostId || lastActionPlayerId_ == clientId) ? lastActionPlayerId_ : hostId;

            const tetris::net::PlayerId winner = (loser == hostId) ? clientId : hostId;

            tetris::net::MatchResult hostRes;
            hostRes.endTick = serverTick_;
            hostRes.playerId = hostId;
            hostRes.finalScore = static_cast<int>(sharedGame_.score());
            hostRes.outcome = (winner == hostId) ? tetris::net::MatchOutcome::Win : tetris::net::MatchOutcome::Lose;
            localMatchResult_ = hostRes;

            tetris::net::MatchResult clientRes;
            clientRes.endTick = serverTick_;
            clientRes.playerId = clientId;
            clientRes.finalScore = static_cast<int>(sharedGame_.score());
            clientRes.outcome = (winner == clientId) ? tetris::net::MatchOutcome::Win : tetris::net::MatchOutcome::Lose;
            clientMatchResult_ = clientRes;

            if (host_) {
                tetris::net::Message msg;
                msg.kind = tetris::net::MessageKind::MatchResult;
                msg.payload = clientRes;
                host_->broadcast(msg);
                host_->onMatchFinished();
            }

            return;
        }
    }

    applyHoldInputs(dtSeconds);

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
    switch (idx) {
        case 1: return IM_COL32(  0, 255, 255, 255);
        case 2: return IM_COL32(255, 255,   0, 255);
        case 3: return IM_COL32(160,  32, 240, 255);
        case 4: return IM_COL32(  0,   0, 255, 255);
        case 5: return IM_COL32(255, 165,   0, 255);
        case 6: return IM_COL32(  0, 255,   0, 255);
        case 7: return IM_COL32(255,   0,   0, 255);
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
            if (b.cell(r, c) != tetris::core::CellState::Filled) continue;

            ImU32 col = locked;
            if (const auto t = b.cellType(r, c)) {
                col = colorFromIndex(colorIndexForTetromino(*t));
            }

            ImVec2 p0(topLeft.x + c * cell + 1, topLeft.y + r * cell + 1);
            ImVec2 p1(p0.x + cell - 2, p0.y + cell - 2);
            dl->AddRectFilled(p0, p1, col);
            dl->AddRect(p0, p1, IM_COL32(20, 20, 20, 255));
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

    ImGui::Text("Role: %s  |  Network: %s",
                cfg_.isHost ? "Host" : "Client",
                (host_ || client_) ? "ON" : "OFF");

    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        std::uint32_t leftMs = 0;

        if (client_) {
            leftMs = displayTimeLeftMs_;
            if (leftMs == 0) {
                std::lock_guard<std::mutex> lock(stateMutex_);
                if (lastState_) leftMs = lastState_->timeLeftMs;
            }
        } else {
            const float leftSec = std::max(
                0.0f,
                static_cast<float>(cfg_.timeLimitSeconds) - matchElapsedSec_
            );
            leftMs = static_cast<std::uint32_t>(leftSec * 1000.0f + 0.5f);
        }

        const std::uint32_t leftSecInt = (leftMs + 999u) / 1000u;
        const std::uint32_t mm = leftSecInt / 60u;
        const std::uint32_t ss = leftSecInt % 60u;

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

    std::optional<tetris::net::StateUpdate> snap;
    if (client_) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        snap = lastState_;
    }

    dl->AddText(ImVec2(x0, y0 - 22), IM_COL32_WHITE, "You");
    dl->AddText(ImVec2(x0 + boardPxW + margin, y0 - 22), IM_COL32_WHITE, "Opponent");

    if (client_ && snap && snap->players.size() >= 2) {
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
        drawBoardDTO(dl, snap->players[0].board, ImVec2(x0, y0), cell, true);
    } else {
        drawBoardFromGame(dl, sharedGame_, ImVec2(x0, y0), cell, true, true);
    }

    const int sharedScore = (client_ && snap && !snap->players.empty())
        ? snap->players[0].score
        : static_cast<int>(sharedGame_.score());
    const int sharedLevel = (client_ && snap && !snap->players.empty())
        ? snap->players[0].level
        : sharedGame_.level();

    float cardY = y0 + boardPxH + margin;
    float cardH = 110.0f;
    float cardW = (static_cast<float>(w) - margin * 3.0f) * 0.5f;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    // Left card: "You"
    ImGui::SetNextWindowPos(ImVec2(margin, cardY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGui::Begin("You##shared", nullptr, flags);

    bool myTurn = false;
    std::uint32_t piecesLeft = piecesLeftThisTurn_;

    if (client_ && snap) {
        const auto myId = (client_->playerId() ? *client_->playerId() : 0u);
        myTurn = (myId != 0u && snap->turnPlayerId == myId);
        piecesLeft = snap->piecesLeftThisTurn;
    } else if (cfg_.isHost) {
        myTurn = (turnPlayerId_ == 1u);
        piecesLeft = piecesLeftThisTurn_;
    }

    ImGui::Text("Turn: %s", myTurn ? "YOUR TURN" : "WAIT");
    ImGui::Text("Pieces left: %u", piecesLeft);
    ImGui::Separator();
    ImGui::Text("Shared score: %d", sharedScore);
    ImGui::Text("Shared level: %d", sharedLevel);
    ImGui::End();

    // Right card: "Opponent"
    ImGui::SetNextWindowPos(ImVec2(margin * 2 + cardW, cardY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGui::Begin("Opponent##shared", nullptr, flags);

    bool oppTurn = false;
    if (client_ && snap) {
        const auto myId = (client_->playerId() ? *client_->playerId() : 0u);
        oppTurn = (myId != 0u && snap->turnPlayerId != myId);
    } else if (cfg_.isHost) {
        oppTurn = (turnPlayerId_ != 1u);
    }

    ImGui::Text("Turn: %s", oppTurn ? "PLAYING" : "WAIT");
    ImGui::Text("Pieces left: %u", piecesLeft);
    ImGui::Separator();
    ImGui::Text("Shared score: %d", sharedScore);
    ImGui::Text("Shared level: %d", sharedLevel);
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

    if (!client_) {
        if (gs->status() != tetris::core::GameStatus::Running) {
            softDropHoldAccSec_ = 0.0f;
            leftHoldSec_ = rightHoldSec_ = 0.0f;
            leftRepeatAccSec_ = rightRepeatAccSec_ = 0.0f;
            return;
        }
    }

    if (cfg_.mode == tetris::net::GameMode::SharedTurns) {
        tetris::net::PlayerId myId = cfg_.isHost ? 1u : (client_ && client_->playerId() ? *client_->playerId() : 0u);

        tetris::net::PlayerId currentTurn = 0u;
        if (cfg_.isHost) {
            currentTurn = turnPlayerId_;
        } else {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (lastState_) currentTurn = lastState_->turnPlayerId;
        }

        if (myId == 0u || currentTurn == 0u || myId != currentTurn) {
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
    if (cfg_.mode != tetris::net::GameMode::TimeAttack) return;

    const bool hostDead = (localGame_.status() == tetris::core::GameStatus::GameOver);
    const bool oppDead  = (oppGame_.status()   == tetris::core::GameStatus::GameOver);

    const bool timeLimitEnabled = (cfg_.timeLimitSeconds > 0);
    const bool timeUp = timeLimitEnabled && (matchElapsedSec_ >= static_cast<float>(cfg_.timeLimitSeconds));

    if (!timeUp && !hostDead && !oppDead) {
        return;
    }

    matchEnded_ = true;
    ++serverTick_;

    const int hostScore = static_cast<int>(localGame_.score());
    const int oppScore  = static_cast<int>(oppGame_.score());

    tetris::net::MatchOutcome hostOutcome = tetris::net::MatchOutcome::Draw;
    tetris::net::MatchOutcome oppOutcome  = tetris::net::MatchOutcome::Draw;

    if (hostDead && !oppDead) {
        hostOutcome = tetris::net::MatchOutcome::Lose;
        oppOutcome  = tetris::net::MatchOutcome::Win;
    } else if (!hostDead && oppDead) {
        hostOutcome = tetris::net::MatchOutcome::Win;
        oppOutcome  = tetris::net::MatchOutcome::Lose;
    } else {
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

    tetris::net::MatchResult hostRes;
    hostRes.endTick = serverTick_;
    hostRes.playerId = 1;
    hostRes.outcome = hostOutcome;
    hostRes.finalScore = hostScore;
    localMatchResult_ = hostRes;

    tetris::net::MatchResult clientRes;
    clientRes.endTick = serverTick_;
    clientRes.playerId = 2;
    clientRes.outcome = oppOutcome;
    clientRes.finalScore = oppScore;
    clientMatchResult_ = clientRes;

    tetris::net::Message msg;
    msg.kind = tetris::net::MessageKind::MatchResult;
    msg.payload = clientRes;
    host_->broadcast(msg);

    if (host_) {
        host_->onMatchFinished();
    }
}

void MultiplayerGameScreen::renderMatchOverlay(Application& app, int w, int h)
{
    if (!matchEnded_) return;

    if (hostDisconnected_ && !cfg_.isHost) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520, 260), ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove;

        ImGui::Begin("Connection Lost", nullptr, flags);
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextUnformatted("HOST DISCONNECTED");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::TextUnformatted("The host left or the connection was lost.");
        ImGui::Spacing();

        if (ImGui::Button("Back to Menu", ImVec2(-1, 44))) {
            app.setScreen(std::make_unique<StartScreen>());
            ImGui::End();
            return;
        }

        ImGui::End();
        return;
    }

    if (!localMatchResult_.has_value()) {
        return;
    }

    auto outcomeToText = [](tetris::net::MatchOutcome o) -> const char* {
        switch (o) {
            case tetris::net::MatchOutcome::Win:  return "YOU WIN!";
            case tetris::net::MatchOutcome::Lose: return "YOU LOSE";
            case tetris::net::MatchOutcome::Draw: return "DRAW";
        }
        return "MATCH OVER";
    };

    const char* title = outcomeToText(localMatchResult_->outcome);

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

    if (opponentDisconnected_) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "Opponent disconnected.");
    }

    if (cfg_.mode == tetris::net::GameMode::TimeAttack && cfg_.timeLimitSeconds > 0) {
        ImGui::Text("Time limit: %u s", cfg_.timeLimitSeconds);
    }

    ImGui::Spacing();
    ImGui::Separator();

    bool opponentPresent = true;
    bool opponentReady = false;

    if (cfg_.isHost) {
        opponentPresent = host_ && host_->hasAnyConnectedClient();
        opponentReady   = host_ && host_->allConnectedClientsReadyForRematch();

        if (!opponentPresent) {
            ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "Opponent left. Rematch unavailable.");
        } else {
            ImGui::Text("Opponent rematch: %s", opponentReady ? "READY" : "NOT READY");
        }

        ImGui::Spacing();

        ImGui::BeginDisabled(!opponentPresent);
        if (ImGui::Button(hostWantsRematch_ ? "Rematch: READY (click to cancel)" : "Rematch (I am ready)", ImVec2(-1, 42))) {
            hostWantsRematch_ = !hostWantsRematch_;

            if (host_) {
                tetris::net::Message m;
                m.kind = tetris::net::MessageKind::RematchDecision;
                m.payload = tetris::net::RematchDecision{ hostWantsRematch_ };
                host_->sendTo(2u, m);
            }
        }
        ImGui::EndDisabled();

        if (opponentPresent && hostWantsRematch_ && opponentReady && host_) {
            localGame_.reset();  localGame_.start();  localCtrl_.resetTiming();
            oppGame_.reset();    oppGame_.start();    oppCtrl_.resetTiming();
            sharedGame_.reset(); sharedGame_.start(); sharedCtrl_.resetTiming();

            matchEnded_ = false;
            matchElapsedSec_ = 0.0f;
            localMatchResult_.reset();
            clientMatchResult_.reset();
            hostWantsRematch_ = false;

            host_->clearRematchFlags();
            host_->startMatch();
        }
    } else {
        if (waitingRematchStart_) {
            ImGui::TextDisabled("Waiting host to restart...");
        }

        if (ImGui::Button(clientWantsRematch_ ? "Rematch requested" : "Rematch (request)", ImVec2(-1, 42))) {
            if (client_ && !clientWantsRematch_) {
                clientWantsRematch_ = true;
                waitingRematchStart_ = true;
                client_->sendRematchDecision(true);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Back to Menu", ImVec2(-1, 42))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    ImGui::End();
}

} // namespace tetris::gui_sdl
