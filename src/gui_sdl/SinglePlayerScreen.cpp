#include "gui_sdl/SinglePlayerScreen.hpp"

#include <algorithm>
#include <cstdint>

#include <imgui.h>
#include <SDL.h>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "controller/InputAction.hpp"
#include "core/Tetromino.hpp"
#include "core/Types.hpp"

namespace tetris::gui_sdl {

static ImU32 colorForTetromino(tetris::core::TetrominoType type)
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

static void unpackImU32(ImU32 col, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a)
{
    r = (col >> IM_COL32_R_SHIFT) & 0xFF;
    g = (col >> IM_COL32_G_SHIFT) & 0xFF;
    b = (col >> IM_COL32_B_SHIFT) & 0xFF;
    a = (col >> IM_COL32_A_SHIFT) & 0xFF;
}

static tetris::core::Tetromino computeGhost(const tetris::core::Board& board,
                                            const tetris::core::Tetromino& active)
{
    using tetris::core::Position;

    tetris::core::Tetromino ghost = active;

    while (true) {
        auto prev = ghost.origin();
        ghost.setOrigin(Position{prev.row + 1, prev.col});
        if (!board.canPlace(ghost)) {
            ghost.setOrigin(prev);
            break;
        }
    }
    return ghost;
}

SinglePlayerScreen::SinglePlayerScreen()
    : gameState_(20, 10, 0)
    , controller_(gameState_)
{
    gameState_.start();
    controller_.resetTiming();
}

void SinglePlayerScreen::dispatchAction(tetris::controller::InputAction action)
{
    controller_.handleAction(action);
}

SinglePlayerScreen::Layout SinglePlayerScreen::computeLayout(int windowW, int windowH) const
{
    Layout L{};
    const int rows = gameState_.board().rows();
    const int cols = gameState_.board().cols();

    const int margin = 20;
    const int bottomReserve = 120; // smaller reserve now that controls are compact

    // Default values; we override nextW/nextH later based on cell size.
    L.nextW = 240;
    L.nextH = 210;

    // Side-by-side attempt
    {
        const int usableW = windowW - (margin * 3) - L.nextW;
        const int usableH = windowH - (margin * 2) - bottomReserve;

        int cellFromW = usableW / cols;
        int cellFromH = usableH / rows;
        int cell = std::min(cellFromW, cellFromH);
        cell = std::clamp(cell, 16, 44);

        int boardW = cols * cell;
        int boardH = rows * cell;
        int groupW = boardW + margin + L.nextW;

        if (usableW > 0 && usableH > 0 && groupW <= windowW - margin * 2) {
            L.sideBySide = true;
            L.cell = cell;
            L.boardW = boardW;
            L.boardH = boardH;

            L.groupW = groupW;
            L.groupX = (windowW - groupW) / 2;

            L.boardX = L.groupX;
            L.nextX  = L.boardX + L.boardW + margin;

            L.boardY = margin + std::max(0, (usableH - boardH) / 2);
            L.nextY  = L.boardY;

            return L;
        }
    }

    // Stacked fallback
    {
        const int usableW = windowW - margin * 2;
        const int usableH = windowH - (margin * 3) - bottomReserve - L.nextH;

        int cellFromW = usableW / cols;
        int cellFromH = usableH / rows;
        int cell = std::min(cellFromW, cellFromH);
        cell = std::clamp(cell, 16, 44);

        L.sideBySide = false;
        L.cell = cell;
        L.boardW = cols * cell;
        L.boardH = rows * cell;

        L.groupW = std::max(L.boardW, L.nextW);
        L.groupX = (windowW - L.groupW) / 2;

        L.boardX = L.groupX + (L.groupW - L.boardW) / 2;
        L.boardY = margin;

        L.nextX  = L.groupX + (L.groupW - L.nextW) / 2;
        L.nextY  = L.boardY + L.boardH + margin;

        return L;
    }
}

void SinglePlayerScreen::handleEvent(Application& app, const SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
        switch (e.key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_a:
                dispatchAction(tetris::controller::InputAction::MoveLeft);
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                dispatchAction(tetris::controller::InputAction::MoveRight);
                break;
            case SDLK_DOWN:
            case SDLK_s:
                dispatchAction(tetris::controller::InputAction::SoftDrop);
                break;
            case SDLK_SPACE:
                dispatchAction(tetris::controller::InputAction::HardDrop);
                break;
            case SDLK_x:
            case SDLK_e:
                dispatchAction(tetris::controller::InputAction::RotateCW);
                break;
            case SDLK_z:
            case SDLK_q:
                dispatchAction(tetris::controller::InputAction::RotateCCW);
                break;
            case SDLK_p:
            case SDLK_ESCAPE:
                dispatchAction(tetris::controller::InputAction::PauseResume);
                break;
            case SDLK_BACKSPACE:
                app.setScreen(std::make_unique<StartScreen>());
                break;
            default:
                break;
        }
    }
}

void SinglePlayerScreen::update(Application&, float dtSeconds)
{
    const int ms = static_cast<int>(dtSeconds * 1000.0f + 0.5f);
    controller_.update(tetris::controller::GameController::Duration{ms});

    if (gameState_.status() != tetris::core::GameStatus::Running) {
        softDropHoldAccumulatorSec_ = 0.0f;
        leftHoldSec_ = rightHoldSec_ = 0.0f;
        leftRepeatAccSec_ = rightRepeatAccSec_ = 0.0f;
        return;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    // Soft drop hold
    const bool downHeld = (keys[SDL_SCANCODE_DOWN] != 0) || (keys[SDL_SCANCODE_S] != 0);
    if (!downHeld) {
        softDropHoldAccumulatorSec_ = 0.0f;
    } else {
        softDropHoldAccumulatorSec_ += dtSeconds;
        while (softDropHoldAccumulatorSec_ >= softDropRepeatSec_) {
            dispatchAction(tetris::controller::InputAction::SoftDrop);
            softDropHoldAccumulatorSec_ -= softDropRepeatSec_;
        }
    }

    // Side hold DAS/ARR
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
                dispatchAction(tetris::controller::InputAction::MoveLeft);
                leftRepeatAccSec_ -= sideArrSec_;
            }
        } else {
            leftRepeatAccSec_ = 0.0f;
        }
        rightHoldSec_ = 0.0f;
        rightRepeatAccSec_ = 0.0f;
    }

    if (rightHeld) {
        rightHoldSec_ += dtSeconds;
        if (rightHoldSec_ >= sideDasSec_) {
            rightRepeatAccSec_ += dtSeconds;
            while (rightRepeatAccSec_ >= sideArrSec_) {
                dispatchAction(tetris::controller::InputAction::MoveRight);
                rightRepeatAccSec_ -= sideArrSec_;
            }
        } else {
            rightRepeatAccSec_ = 0.0f;
        }
        leftHoldSec_ = 0.0f;
        leftRepeatAccSec_ = 0.0f;
    }
}

void SinglePlayerScreen::render(Application& app)
{
    int winW = 0, winH = 0;
    app.getWindowSize(winW, winH);

    Layout L = computeLayout(winW, winH);

    // Responsive right-column sizing based on cell size
    const int margin = 16;
    const int nextW = std::clamp(L.cell * 8, 220, 340);
    const int nextH = std::clamp(L.cell * 7 + 60, 200, 300); // preview + hint
    const int gameW = nextW;

    // Game height will be auto-sized by ImGui (no scrollbars), but we still set a reasonable max
    const int gameH = 0; // 0 => autosize

    // Right column position stays next to board
    const int rightX = L.nextX;
    const int topY   = L.boardY;

    const int gameX = rightX;
    const int gameY = topY;

    const int nextX = rightX;
    const int nextY = gameY + 170 + margin; // enough room for Game content

    // Controls under Next
    const int ctrlW = nextW;
    const int ctrlH = 160;
    int ctrlX = rightX;
    int ctrlY = nextY + nextH + margin;

    // If not enough room under next, pin to bottom but never overlap next
    if (ctrlY + ctrlH > winH - margin) {
        ctrlY = std::max(nextY + nextH + margin, winH - ctrlH - margin);
    }

    SDL_Renderer* renderer = app.renderer();

    renderBoard(renderer, L.boardX, L.boardY, L.cell);
    renderBoardOverlayText(L);

    renderNextPieceWindow(nextX, nextY, nextW, nextH);
    renderHUD(app, winW, winH, L, gameX, gameY, gameW, gameH, ctrlX, ctrlY, ctrlW, ctrlH);
}

void SinglePlayerScreen::renderBoard(SDL_Renderer* renderer, int x, int y, int cellSize) const
{
    const auto& board = gameState_.board();
    const int rows = board.rows();
    const int cols = board.cols();

    SDL_SetRenderDrawColor(renderer, 12, 12, 16, 255);
    SDL_Rect boardRect{x, y, cols * cellSize, rows * cellSize};
    SDL_RenderFillRect(renderer, &boardRect);

    SDL_SetRenderDrawColor(renderer, 40, 40, 55, 255);
    for (int r = 0; r <= rows; ++r) {
        SDL_RenderDrawLine(renderer, x, y + r * cellSize, x + cols * cellSize, y + r * cellSize);
    }
    for (int c = 0; c <= cols; ++c) {
        SDL_RenderDrawLine(renderer, x + c * cellSize, y, x + c * cellSize, y + rows * cellSize);
    }

    SDL_SetRenderDrawColor(renderer, 90, 90, 95, 255);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board.cell(r, c) != tetris::core::CellState::Filled) continue;

            // If we know which tetromino filled this cell, color it.
            const auto t = board.cellType(r, c);
            ImU32 col = t ? colorForTetromino(*t) : IM_COL32(90, 90, 95, 255);

            std::uint8_t rr, gg, bb, aa;
            unpackImU32(col, rr, gg, bb, aa);
            SDL_SetRenderDrawColor(renderer, rr, gg, bb, aa);

            SDL_Rect cell{x + c * cellSize + 1, y + r * cellSize + 1, cellSize - 2, cellSize - 2};
            SDL_RenderFillRect(renderer, &cell);
        }
    }

    if (gameState_.activeTetromino().has_value()) {
        const auto& t = *gameState_.activeTetromino();

        // Ghost
        auto ghost = computeGhost(board, t);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 70);
        for (const auto& b : ghost.blocks()) {
            if (b.row < 0) continue;
            SDL_Rect rct{x + b.col * cellSize + 2, y + b.row * cellSize + 2, cellSize - 4, cellSize - 4};
            SDL_RenderDrawRect(renderer, &rct);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // Active
        const ImU32 col = colorForTetromino(t.type());
        std::uint8_t rr, gg, bb, aa;
        unpackImU32(col, rr, gg, bb, aa);
        SDL_SetRenderDrawColor(renderer, rr, gg, bb, aa);

        for (const auto& b : t.blocks()) {
            if (b.row < 0) continue;
            SDL_Rect cell{x + b.col * cellSize + 1, y + b.row * cellSize + 1, cellSize - 2, cellSize - 2};
            SDL_RenderFillRect(renderer, &cell);
        }
    }

    if (gameState_.status() == tetris::core::GameStatus::Paused ||
        gameState_.status() == tetris::core::GameStatus::GameOver)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}

void SinglePlayerScreen::renderBoardOverlayText(const Layout& L) const
{
    using tetris::core::GameStatus;

    auto status = gameState_.status();
    if (status != GameStatus::Paused && status != GameStatus::GameOver) return;

    const char* msg = (status == GameStatus::Paused) ? "PAUSED" : "GAME OVER";
    const char* hint = (status == GameStatus::Paused)
        ? "Press P or ESC to resume"
        : "Use Restart button on the right";

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float cx = L.boardX + L.boardW * 0.5f;
    const float cy = L.boardY + L.boardH * 0.5f;

    // Scale card by cell size for responsiveness
    const float scale = std::clamp(L.cell / 28.0f, 0.75f, 1.25f);
    const float cardW = std::min(420.0f * scale, L.boardW * 0.90f);
    const float cardH = 120.0f * scale;

    ImVec2 p0(cx - cardW * 0.5f, cy - cardH * 0.5f);
    ImVec2 p1(cx + cardW * 0.5f, cy + cardH * 0.5f);
    dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 175), 10.0f);

    ImGuiIO& io = ImGui::GetIO();
    ImFont* bigFont = (io.Fonts->Fonts.Size > 1) ? io.Fonts->Fonts[1] : ImGui::GetFont();
    const float bigSize = bigFont->FontSize * scale;

    ImVec2 tSize = bigFont->CalcTextSizeA(bigSize, FLT_MAX, 0.0f, msg);
    dl->AddText(bigFont, bigSize,
                ImVec2(cx - tSize.x * 0.5f, cy - 44.0f * scale),
                IM_COL32(255, 255, 255, 255),
                msg);

    ImVec2 hSize = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(cx - hSize.x * 0.5f, cy + 18.0f * scale),
                IM_COL32(220, 220, 220, 255),
                hint);
}

void SinglePlayerScreen::renderNextPieceWindow(int x, int y, int w, int h) const
{
    ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("Next Piece", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    const float pad = 25.0f;
    const float areaW = (float)w - pad * 2.0f;
    const float areaH = (float)h - 60.0f;
    const float side = (std::min)(areaW, areaH);

    const ImVec2 a0(wp.x + pad + (areaW - side) * 0.5f, wp.y + pad);
    const ImVec2 a1(a0.x + side, a0.y + side);

    dl->AddRect(a0, a1, IM_COL32(120, 120, 120, 120), 6.0f);
    for (int i = 1; i < 4; ++i) {
        float t = side * (i / 4.5f);
        dl->AddLine(ImVec2(a0.x + t, a0.y), ImVec2(a0.x + t, a1.y), IM_COL32(120, 120, 120, 50));
        dl->AddLine(ImVec2(a0.x, a0.y + t), ImVec2(a1.x, a0.y + t), IM_COL32(120, 120, 120, 50));
    }

    if (!gameState_.nextTetromino().has_value()) {
        ImGui::TextUnformatted("No next piece");
        ImGui::End();
        return;
    }

    const auto& next = *gameState_.nextTetromino();
    const auto blocks = next.blocks();
    const ImU32 col = colorForTetromino(next.type());

    int minR = blocks[0].row, maxR = blocks[0].row;
    int minC = blocks[0].col, maxC = blocks[0].col;
    for (const auto& b : blocks) {
        minR = std::min(minR, b.row); maxR = std::max(maxR, b.row);
        minC = std::min(minC, b.col); maxC = std::max(maxC, b.col);
    }

    const float cell = side / 4.5f;
    const float pieceW = (maxC - minC + 1) * cell;
    const float pieceH = (maxR - minR + 1) * cell;
    const float ox = a0.x + (side - pieceW) * 0.5f - minC * cell;
    const float oy = a0.y + (side - pieceH) * 0.5f - minR * cell;

    for (const auto& b : blocks) {
        const float bx = ox + b.col * cell;
        const float by = oy + b.row * cell;
        dl->AddRectFilled(ImVec2(bx + 1, by + 1), ImVec2(bx + cell - 2, by + cell - 2), col);
        dl->AddRect(ImVec2(bx + 1, by + 1), ImVec2(bx + cell - 2, by + cell - 2), IM_COL32(20, 20, 20, 255));
    }

    ImGui::Dummy(ImVec2(0, (float)h - 60.0f));

    ImGui::End();
}

void SinglePlayerScreen::renderHUD(Application& app, int /*windowW*/, int /*windowH*/, const Layout&,
                                  int gameX, int gameY, int gameW, int /*gameH*/,
                                  int ctrlX, int ctrlY, int ctrlW, int ctrlH)
{
    ImGuiWindowFlags fixedFlags =
        ImGuiWindowFlags_NoMove;

    // Game: auto-size to content (no scrollbars)
    ImGui::SetNextWindowPos(ImVec2((float)gameX, (float)gameY), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2((float)gameW, 0.0f), ImVec2((float)gameW, 300.0f));
    ImGui::Begin("Game", nullptr,
                 fixedFlags | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Score: %llu", (unsigned long long)gameState_.score());
    ImGui::Text("Level: %d", gameState_.level());
    ImGui::Text("Locked pieces: %llu", (unsigned long long)gameState_.lockedPieces());

    ImGui::Separator();

    switch (gameState_.status()) {
        case tetris::core::GameStatus::Running:
            ImGui::Text("Status: Running");
            break;
        case tetris::core::GameStatus::Paused:
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Status: Paused");
            break;
        case tetris::core::GameStatus::GameOver:
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Status: Game Over");
            break;
        default:
            ImGui::Text("Status: Not Started");
            break;
    }

    ImGui::Separator();

    if (gameState_.status() == tetris::core::GameStatus::GameOver) {
        if (ImGui::Button("Restart", ImVec2(-1, 0))) {
            gameState_.reset();
            gameState_.start();
            controller_.resetTiming();
        }
    }

    if (ImGui::Button("Back to Menu", ImVec2(-1, 0))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    ImGui::End();

    // Controls: separate window, but never overlaps Next (render() ensures Y is safe)
    ImGui::SetNextWindowPos(ImVec2((float)ctrlX, (float)ctrlY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)ctrlW, (float)ctrlH), ImGuiCond_Always);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls", nullptr, fixedFlags);

    ImGui::TextUnformatted("A/D or Left/Right: Move (hold)");
    ImGui::TextUnformatted("S or Down: Soft drop (hold)");
    ImGui::TextUnformatted("Space: Hard drop");
    ImGui::TextUnformatted("Z/Q: Rotate CCW");
    ImGui::TextUnformatted("X/E: Rotate CW");
    ImGui::TextUnformatted("P or ESC: Pause/Resume");
    ImGui::TextUnformatted("Backspace: Menu");

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::End();
}

} // namespace tetris::gui_sdl
