#include "gui_sdl/MultiplayerGameScreen.hpp"

#include <imgui.h>
#include <algorithm>
#include <random>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "controller/InputAction.hpp"

namespace tetris::gui_sdl {

static float clampf(float v, float a, float b) {
    return std::max(a, std::min(b, v));
}

ImU32 MultiplayerGameScreen::colorForType(tetris::core::TetrominoType type)
{
    using tetris::core::TetrominoType;
    switch (type) {
        case TetrominoType::I: return IM_COL32(  0, 255, 255, 255);
        case TetrominoType::O: return IM_COL32(255, 255,   0, 255);
        case TetrominoType::T: return IM_COL32(160,  32, 240, 255);
        case TetrominoType::J: return IM_COL32(  0,   0, 255, 255);
        case TetrominoType::L: return IM_COL32(255, 165,   0, 255);
        case TetrominoType::S: return IM_COL32(  0, 255,   0, 255);
        case TetrominoType::Z: return IM_COL32(255,   0,   0, 255);
    }
    return IM_COL32(200, 200, 200, 255);
}

MultiplayerGameScreen::MultiplayerGameScreen(const tetris::net::MultiplayerConfig& cfg)
    : cfg_(cfg)
    , localGame_(20, 10, 0)
    , localCtrl_(localGame_)
    , oppGame_(20, 10, 0)
    , oppCtrl_(oppGame_)
    , sharedGame_(20, 10, 0)
    , sharedCtrl_(sharedGame_)
{
    // start all games (we will use only the ones needed per mode)
    localGame_.start();
    localCtrl_.resetTiming();

    oppGame_.start();
    oppCtrl_.resetTiming();

    sharedGame_.start();
    sharedCtrl_.resetTiming();
}

void MultiplayerGameScreen::handleEvent(Application& app, const SDL_Event& e)
{
    if (e.type != SDL_KEYDOWN || e.key.repeat != 0)
        return;

    const SDL_Keycode key = e.key.keysym.sym;

    // Always allow returning to menu
    if (key == SDLK_BACKSPACE) {
        app.setScreen(std::make_unique<StartScreen>());
        return;
    }

    // Determine which local GameState/Controller is active
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

    // Allow pause/resume even if not Running
    if (key == SDLK_p || key == SDLK_ESCAPE) {
        gc->handleAction(tetris::controller::InputAction::PauseResume);
        return;
    }

    // Don't allow gameplay actions while paused or game over
    if (gs->status() != tetris::core::GameStatus::Running)
        return;

    // First-tap action (hold repeats are handled in update/applyHoldInputs)
    dispatchLocalAction(*gs, *gc, key);
}

void MultiplayerGameScreen::dispatchLocalAction(tetris::core::GameState& gs,
                                               tetris::controller::GameController& gc,
                                               SDL_Keycode key)
{
    using tetris::controller::InputAction;

    // Ignore movement when not running (but allow pause toggle)
    if (gs.status() != tetris::core::GameStatus::Running) {
        if (key == SDLK_p || key == SDLK_ESCAPE)
            gc.handleAction(InputAction::PauseResume);
        return;
    }

    switch (key) {
        // move (arrows OR A/D)
        case SDLK_LEFT:
        case SDLK_a:
            gc.handleAction(InputAction::MoveLeft);
            break;

        case SDLK_RIGHT:
        case SDLK_d:
            gc.handleAction(InputAction::MoveRight);
            break;

        // soft drop (Down OR S)
        case SDLK_DOWN:
        case SDLK_s:
            gc.handleAction(InputAction::SoftDrop);
            break;

        case SDLK_SPACE:
            gc.handleAction(InputAction::HardDrop);
            break;

        case SDLK_z:
        case SDLK_q:
            gc.handleAction(InputAction::RotateCCW);
            break;

        case SDLK_x:
        case SDLK_e:
            gc.handleAction(InputAction::RotateCW);
            break;

        case SDLK_p:
        case SDLK_ESCAPE:
            gc.handleAction(InputAction::PauseResume);
            break;

        default:
            break;
    }
}

void MultiplayerGameScreen::stepOpponentAI(float dtSeconds)
{
    // Only for TimeAttack test: generate sparse inputs so opponent “plays”.
    if (cfg_.mode != tetris::net::GameMode::TimeAttack)
        return;

    if (oppGame_.status() != tetris::core::GameStatus::Running)
        return;

    oppInputAcc_ += dtSeconds;

    // every ~0.20s try one input
    if (oppInputAcc_ < 0.20f)
        return;

    oppInputAcc_ = 0.0f;

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 9);
    int r = dist(rng);

    using tetris::controller::InputAction;
    // simple bias: more left/right, occasional rotations, rare hard drop
    if (r <= 3) oppCtrl_.handleAction(InputAction::MoveLeft);
    else if (r <= 7) oppCtrl_.handleAction(InputAction::MoveRight);
    else if (r == 8) oppCtrl_.handleAction(InputAction::RotateCW);
    else oppCtrl_.handleAction(InputAction::SoftDrop);
}

void MultiplayerGameScreen::update(Application&, float dtSeconds)
{
    // convert dt to ms for your controller
    const int ms = static_cast<int>(dtSeconds * 1000.0f + 0.5f);
    auto dur = tetris::controller::GameController::Duration{ms};

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        localCtrl_.update(dur);
        oppCtrl_.update(dur);

        // UI-only opponent simulation (if you still have it)
        stepOpponentAI(dtSeconds);
    } else {
        sharedCtrl_.update(dur);
    }

    // IMPORTANT: hold-to-repeat (Down/S + Left/Right/A/D)
    // This must exist as a method on MultiplayerGameScreen.
    applyHoldInputs(dtSeconds);
}

void MultiplayerGameScreen::render(Application& app)
{
    int w = 0, h = 0;
    app.getWindowSize(w, h);

    renderTopBar(app, w, h);

    if (cfg_.mode == tetris::net::GameMode::TimeAttack)
        renderTimeAttackLayout(app, w, h);
    else
        renderSharedTurnsLayout(app, w, h);
}

void MultiplayerGameScreen::renderTopBar(Application& app, int w, int h) const
{
    (void)app;
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w - 20), 48), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##mp_topbar", nullptr, flags);

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        ImGui::Text("Mode: Time Attack  |  Limit: %us", cfg_.timeLimitSeconds);
    } else {
        ImGui::Text("Mode: Shared Turns  |  Pieces/turn: %u", cfg_.piecesPerTurn);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 170);
    ImGui::TextUnformatted("Backspace: Menu");

    ImGui::End();
}

void MultiplayerGameScreen::drawBoardFromGame(ImDrawList* dl,
                                             const tetris::core::GameState& gs,
                                             ImVec2 topLeft,
                                             float cell,
                                             bool drawBorder,
                                             bool drawActive) const
{
    const auto& b = gs.board();
    const int rows = b.rows();
    const int cols = b.cols();

    ImVec2 size(cols * cell, rows * cell);
    ImU32 bg = IM_COL32(12, 12, 16, 255);
    ImU32 grid = IM_COL32(40, 40, 55, 255);
    ImU32 locked = IM_COL32(90, 90, 95, 255);

    dl->AddRectFilled(topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y), bg, 6.0f);

    // grid
    for (int r = 0; r <= rows; ++r) {
        float y = topLeft.y + r * cell;
        dl->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + size.x, y), grid);
    }
    for (int c = 0; c <= cols; ++c) {
        float x = topLeft.x + c * cell;
        dl->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + size.y), grid);
    }

    // locked cells (board has no color info => grey)
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (b.cell(r, c) == tetris::core::CellState::Filled) {
                ImVec2 p0(topLeft.x + c * cell + 1, topLeft.y + r * cell + 1);
                ImVec2 p1(p0.x + cell - 2, p0.y + cell - 2);
                dl->AddRectFilled(p0, p1, locked);
            }
        }
    }

    // active piece
    if (drawActive && gs.activeTetromino().has_value()) {
        const auto& t = *gs.activeTetromino();
        ImU32 col = colorForType(t.type());
        for (const auto& block : t.blocks()) {
            if (block.row < 0) continue;
            ImVec2 p0(topLeft.x + block.col * cell + 1, topLeft.y + block.row * cell + 1);
            ImVec2 p1(p0.x + cell - 2, p0.y + cell - 2);
            dl->AddRectFilled(p0, p1, col);
            dl->AddRect(p0, p1, IM_COL32(20, 20, 20, 255));
        }
    }

    if (drawBorder) {
        dl->AddRect(topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y), IM_COL32(120,120,130,255), 6.0f, 0, 2.0f);
    }
}

void MultiplayerGameScreen::renderTimeAttackLayout(Application& app, int w, int h)
{
    (void)app;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // layout
    const float top = 68.0f;
    const float margin = 18.0f;

    float availableW = static_cast<float>(w) - margin * 3.0f;
    float availableH = static_cast<float>(h) - top - margin;

    // allocate scoreboard panel on the right
    const float panelW = 300.0f;
    const float boardsW = availableW - panelW - margin;
    const float boardW = boardsW * 0.5f - margin * 0.5f;

    // compute cell size
    const int cols = localGame_.board().cols();
    const int rows = localGame_.board().rows();
    float cell = std::min(boardW / cols, availableH / rows);
    cell = clampf(cell, 16.0f, 42.0f);

    float boardPxW = cols * cell;
    float boardPxH = rows * cell;

    // center boards block horizontally
    float x0 = (static_cast<float>(w) - (boardPxW * 2 + margin + panelW + margin)) * 0.5f;
    if (x0 < margin) x0 = margin;

    float y0 = top + (availableH - boardPxH) * 0.5f;
    if (y0 < top) y0 = top;

    // labels + boards
    dl->AddText(ImVec2(x0, y0 - 22), IM_COL32_WHITE, "You");
    dl->AddText(ImVec2(x0 + boardPxW + margin, y0 - 22), IM_COL32_WHITE, "Opponent");

    drawBoardFromGame(dl, localGame_, ImVec2(x0, y0), cell, true, true);
    drawBoardFromGame(dl, oppGame_,   ImVec2(x0 + boardPxW + margin, y0), cell, true, true);

    // scoreboard window
    float panelX = x0 + boardPxW * 2 + margin * 2;
    float panelY = y0;
    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, boardPxH), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Scoreboard", nullptr, flags);

    ImGui::Text("You: %llu", (unsigned long long)localGame_.score());
    ImGui::Text("Opp: %llu", (unsigned long long)oppGame_.score());
    ImGui::Separator();
    ImGui::Text("Your level: %d", localGame_.level());
    ImGui::Text("Opp  level: %d", oppGame_.level());
    ImGui::Separator();
    ImGui::TextDisabled("Offline test mode (core-driven).");
    ImGui::TextDisabled("Later: StateUpdate from network.");

    ImGui::End();
}

void MultiplayerGameScreen::renderSharedTurnsLayout(Application& app, int w, int h)
{
    (void)app;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float top = 68.0f;
    const float margin = 18.0f;

    float availableW = static_cast<float>(w) - margin * 2.0f;
    float availableH = static_cast<float>(h) - top - margin * 2.0f - 120.0f; // reserve player cards

    const int cols = sharedGame_.board().cols();
    const int rows = sharedGame_.board().rows();

    float cell = std::min(availableW / cols, availableH / rows);
    cell = clampf(cell, 16.0f, 42.0f);

    float boardPxW = cols * cell;
    float boardPxH = rows * cell;

    float x0 = (static_cast<float>(w) - boardPxW) * 0.5f;
    float y0 = top + (availableH - boardPxH) * 0.5f;
    if (y0 < top) y0 = top;

    dl->AddText(ImVec2(x0 + boardPxW * 0.5f - 80, y0 - 22), IM_COL32_WHITE, "Shared Board (offline test)");
    drawBoardFromGame(dl, sharedGame_, ImVec2(x0, y0), cell, true, true);

    // bottom cards
    float cardY = y0 + boardPxH + margin;
    float cardH = 110.0f;
    float cardW = (static_cast<float>(w) - margin * 3.0f) * 0.5f;

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowPos(ImVec2(margin, cardY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGui::Begin("You##shared", nullptr, flags);
    ImGui::Text("Score: %llu", (unsigned long long)sharedGame_.score());
    ImGui::Text("Level: %d", sharedGame_.level());
    ImGui::TextDisabled("Ready: (UI-only)");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(margin * 2 + cardW, cardY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGui::Begin("Opponent##shared", nullptr, flags);
    ImGui::Text("Score: %llu", (unsigned long long)sharedGame_.score());
    ImGui::Text("Level: %d", sharedGame_.level());
    ImGui::TextDisabled("Ready: (UI-only)");
    ImGui::End();
}

void MultiplayerGameScreen::applyHoldInputs(float dtSeconds)
{
    // Only apply holds if local game is running
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

    if (gs->status() != tetris::core::GameStatus::Running) {
        // reset holds when paused/gameover
        softDropHoldAccSec_ = 0.0f;
        leftHoldSec_ = rightHoldSec_ = 0.0f;
        leftRepeatAccSec_ = rightRepeatAccSec_ = 0.0f;
        return;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    // --- Soft drop hold (Down or S) ---
    const bool downHeld = (keys[SDL_SCANCODE_DOWN] != 0) || (keys[SDL_SCANCODE_S] != 0);
    if (!downHeld) {
        softDropHoldAccSec_ = 0.0f;
    } else {
        softDropHoldAccSec_ += dtSeconds;
        while (softDropHoldAccSec_ >= softDropRepeatSec_) {
            gc->handleAction(tetris::controller::InputAction::SoftDrop);
            softDropHoldAccSec_ -= softDropRepeatSec_;
        }
    }

    // --- Side hold (Left/Right or A/D) with DAS/ARR ---
    const bool leftHeld  = (keys[SDL_SCANCODE_LEFT] != 0) || (keys[SDL_SCANCODE_A] != 0);
    const bool rightHeld = (keys[SDL_SCANCODE_RIGHT] != 0) || (keys[SDL_SCANCODE_D] != 0);

    // If both held, do nothing (prevents jitter)
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

    // Left repeating
    if (leftHeld) {
        leftHoldSec_ += dtSeconds;

        if (leftHoldSec_ >= sideDasSec_) {
            leftRepeatAccSec_ += dtSeconds;
            while (leftRepeatAccSec_ >= sideArrSec_) {
                gc->handleAction(tetris::controller::InputAction::MoveLeft);
                leftRepeatAccSec_ -= sideArrSec_;
            }
        } else {
            leftRepeatAccSec_ = 0.0f;
        }

        // reset right
        rightHoldSec_ = 0.0f;
        rightRepeatAccSec_ = 0.0f;
    }

    // Right repeating
    if (rightHeld) {
        rightHoldSec_ += dtSeconds;

        if (rightHoldSec_ >= sideDasSec_) {
            rightRepeatAccSec_ += dtSeconds;
            while (rightRepeatAccSec_ >= sideArrSec_) {
                gc->handleAction(tetris::controller::InputAction::MoveRight);
                rightRepeatAccSec_ -= sideArrSec_;
            }
        } else {
            rightRepeatAccSec_ = 0.0f;
        }

        // reset left
        leftHoldSec_ = 0.0f;
        leftRepeatAccSec_ = 0.0f;
    }
}

} // namespace tetris::gui_sdl
