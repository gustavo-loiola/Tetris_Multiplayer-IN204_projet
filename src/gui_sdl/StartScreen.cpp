#include "gui_sdl/StartScreen.hpp"
#include "gui_sdl/Application.hpp"
#include "gui_sdl/SinglePlayerScreen.hpp"
#include "gui_sdl/MultiplayerConfigScreen.hpp"

#include <algorithm>
#include <cstdint>

#include <SDL.h>
#include <imgui.h>

namespace tetris::gui_sdl {

// Reuse the same palette as gameplay (ImU32 for convenience)
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

static void unpackImU32(ImU32 col, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)
{
    r = (col >> IM_COL32_R_SHIFT) & 0xFF;
    g = (col >> IM_COL32_G_SHIFT) & 0xFF;
    b = (col >> IM_COL32_B_SHIFT) & 0xFF;
}

StartScreen::StartScreen()
{
    std::random_device rd;
    rng_ = std::mt19937{rd()};
}

void StartScreen::handleEvent(Application&, const SDL_Event&) {}

void StartScreen::update(Application& app, float dtSeconds)
{
    int w = 0, h = 0;
    app.getWindowSize(w, h);
    ensureInitialized(w, h);
    updatePieces(dtSeconds, w, h);
}

void StartScreen::render(Application& app)
{
    int w = 0, h = 0;
    app.getWindowSize(w, h);
    ensureInitialized(w, h);

    // ---- SDL background animation (behind menu) ----
    renderPieces(app.renderer());

    // darken slightly so menu is readable
    SDL_Renderer* r = app.renderer();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 70);
    SDL_Rect dim{0, 0, w, h};
    SDL_RenderFillRect(r, &dim);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // ---- ImGui start menu (on top) ----
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("Tetris", nullptr, flags);
    //ImGui::Begin("Start Screen", nullptr, flags);
    //ImGui::TextUnformatted("IN204 - Tetris Project");
    ImGui::Separator();

    if (ImGui::Button("Single Player", ImVec2(-1, 44))) {
        app.setScreen(std::make_unique<SinglePlayerScreen>());
        ImGui::End();
        return;
    }

    if (ImGui::Button("Multiplayer", ImVec2(-1, 44))) {
        app.setScreen(std::make_unique<MultiplayerConfigScreen>());
        ImGui::End();
        return;
    }

    if (ImGui::Button("Quit", ImVec2(-1, 40))) {
        app.requestQuit();
        ImGui::End();
        return;
    }

    ImGui::End();
}

// -------------------- background internals --------------------

void StartScreen::ensureInitialized(int w, int h)
{
    if (!initialized_) {
        rebuildPieces(w, h);
        initialized_ = true;
        lastW_ = w;
        lastH_ = h;
        return;
    }

    // If window changed a lot, rebuild amount of pieces for density
    // (keep it cheap: only rebuild when size change is meaningful)
    if (std::abs(w - lastW_) > 160 || std::abs(h - lastH_) > 160) {
        rebuildPieces(w, h);
        lastW_ = w;
        lastH_ = h;
    }
}

int StartScreen::targetPieceCount(int w, int h) const
{
    // Density-based: ~1 piece per 250x250 area, clamped
    const float area = float(w) * float(h);
    int n = int(area / (250.0f * 250.0f));
    return std::clamp(n, 6, 22);
}

float StartScreen::frand(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng_);
}

tetris::core::TetrominoType StartScreen::randomType()
{
    std::uniform_int_distribution<int> dist(0, 6);
    return static_cast<tetris::core::TetrominoType>(dist(rng_));
}

tetris::core::Rotation StartScreen::randomRotation()
{
    std::uniform_int_distribution<int> dist(0, 3);
    return static_cast<tetris::core::Rotation>(dist(rng_));
}

void StartScreen::rebuildPieces(int w, int h)
{
    const int desired = targetPieceCount(w, h);
    pieces_.clear();
    pieces_.reserve(desired);

    for (int i = 0; i < desired; ++i) {
        FallingPiece p;
        respawnPiece(p, w, h, /*spawnAbove=*/true);

        // spread them out vertically so it's not synchronized
        p.yPx = frand(-float(h), float(h));
        pieces_.push_back(p);
    }
}

void StartScreen::respawnPiece(FallingPiece& p, int w, int h, bool spawnAbove)
{
    p.type = randomType();
    p.rot  = randomRotation();

    // block size scales gently with window height (but clamped)
    int base = int(std::clamp(h / 40, 14, 26));
    p.cellPx = base;

    // spawn X with some margin so it doesn't clip too much
    p.xPx = frand(-50.0f, float(w) - 50.0f);
    p.yPx = spawnAbove ? frand(-float(h) * 0.3f - 240.0f, -60.0f) : frand(0.0f, float(h));

    // speed: proportional to size; add randomness
    p.vy = frand(60.0f, 160.0f) * (p.cellPx / 18.0f);

    // slight drift
    p.vx = frand(-18.0f, 18.0f);

    // base alpha (max opacity)
    p.alpha = (std::uint8_t)std::clamp(int(frand(70.0f, 135.0f)), 50, 170);

    // --- fade lifecycle ---
    p.lifeSec = 0.0f;
    p.fadeInSec  = frand(0.20f, 0.55f);
    p.fadeOutSec = frand(0.30f, 0.70f);

    // start fading out shortly before leaving the bottom
    p.respawnY = float(h) + frand(40.0f, 160.0f);
}

void StartScreen::updatePieces(float dt, int w, int h)
{
    for (auto& p : pieces_) {
        p.lifeSec += dt;

        p.yPx += p.vy * dt;
        p.xPx += p.vx * dt;

        // gentle wrap horizontally
        if (p.xPx < -120.0f) p.xPx = float(w) + 120.0f;
        if (p.xPx > float(w) + 120.0f) p.xPx = -120.0f;

        // When it has gone far enough down, allow fade-out then respawn
        // We respawn after fade-out duration has elapsed *past* respawnY
        if (p.yPx > p.respawnY) {
            // If piece has had enough time to complete fade-out after passing respawn line, respawn
            // (we measure "time since crossing" by using (y - respawnY) / vy as an estimate)
            float timePast = (p.vy > 1e-3f) ? (p.yPx - p.respawnY) / p.vy : 999.0f;
            if (timePast >= p.fadeOutSec) {
                respawnPiece(p, w, h, /*spawnAbove=*/true);
            }
        }
    }
}

void StartScreen::renderPieces(SDL_Renderer* r)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (const auto& p : pieces_) {
        // Use core::Tetromino to get correct silhouettes
        tetris::core::Tetromino t(p.type, p.rot, tetris::core::Position{0, 0});
        const auto blocks = t.blocks();

        const ImU32 col = colorForTetromino(p.type);
        std::uint8_t rr, gg, bb;
        unpackImU32(col, rr, gg, bb);

        // ---- Fade factor ----
        // Fade in based on lifeSec
        float fadeIn = 1.0f;
        if (p.fadeInSec > 1e-3f)
            fadeIn = std::clamp(p.lifeSec / p.fadeInSec, 0.0f, 1.0f);

        // Fade out when past respawnY (use distance past respawnY mapped to fadeOutSec)
        float fadeOut = 1.0f;
        if (p.yPx > p.respawnY && p.fadeOutSec > 1e-3f) {
            float timePast = (p.vy > 1e-3f) ? (p.yPx - p.respawnY) / p.vy : 999.0f;
            fadeOut = 1.0f - std::clamp(timePast / p.fadeOutSec, 0.0f, 1.0f);
        }

        float fade = fadeIn * fadeOut;
        std::uint8_t aFill = (std::uint8_t)std::clamp(int(float(p.alpha) * fade), 0, 255);

        // Fill
        SDL_SetRenderDrawColor(r, rr, gg, bb, aFill);

        // Outline slightly stronger, but scaled by same fade
        std::uint8_t oa = (std::uint8_t)std::clamp(int(float(std::min<int>(p.alpha + 70, 220)) * fade), 0, 255);

        // Compute bounding box in "cell space" for centering
        int minC = blocks[0].col, maxC = blocks[0].col;
        int minR = blocks[0].row, maxR = blocks[0].row;
        for (const auto& b : blocks) {
            minC = std::min(minC, b.col); maxC = std::max(maxC, b.col);
            minR = std::min(minR, b.row); maxR = std::max(maxR, b.row);
        }

        // Center the piece around (xPx, yPx) even when offsets are negative
        const float pieceW = float(maxC - minC + 1) * p.cellPx;
        const float pieceH = float(maxR - minR + 1) * p.cellPx;
        const float originX = p.xPx - pieceW * 0.5f - float(minC) * p.cellPx;
        const float originY = p.yPx - pieceH * 0.5f - float(minR) * p.cellPx;

        // Draw 4 blocks
        for (const auto& b : blocks) {
            SDL_Rect rc;
            rc.x = int(originX + float(b.col) * p.cellPx);
            rc.y = int(originY + float(b.row) * p.cellPx);
            rc.w = p.cellPx - 1;
            rc.h = p.cellPx - 1;

            SDL_RenderFillRect(r, &rc);

            SDL_SetRenderDrawColor(r, rr, gg, bb, oa);
            SDL_RenderDrawRect(r, &rc);

            // restore fill color for next block
            SDL_SetRenderDrawColor(r, rr, gg, bb, aFill);
        }
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

} // namespace tetris::gui_sdl
