#pragma once

#include "gui_sdl/Screen.hpp"

#include <vector>
#include <random>

#include "core/Types.hpp"       // TetrominoType, Rotation
#include "core/Tetromino.hpp"   // Tetromino geometry

namespace tetris::gui_sdl {

class StartScreen final : public Screen {
public:
    StartScreen(); // NEW
    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    struct FallingPiece {
        tetris::core::TetrominoType type{};
        tetris::core::Rotation rot{};
        float xPx{};       // top-left origin in pixels
        float yPx{};
        float vy{};        // pixels/sec
        float vx{};        // pixels/sec (small drift)
        int cellPx{18};    // block size in pixels
        std::uint8_t alpha{80}; // fill alpha

        float lifeSec{0.0f};       // time since spawn
        float fadeInSec{0.35f};    // fade in duration
        float fadeOutSec{0.45f};   // fade out duration
        float respawnY{0.0f};      // y threshold to start fade-out
    };

    // background simulation state
    bool initialized_{false};
    int lastW_{0};
    int lastH_{0};

    std::mt19937 rng_;
    std::vector<FallingPiece> pieces_;

    void ensureInitialized(int w, int h);
    void rebuildPieces(int w, int h);
    void respawnPiece(FallingPiece& p, int w, int h, bool spawnAbove);
    void updatePieces(float dt, int w, int h);
    void renderPieces(SDL_Renderer* r);

    // helpers
    int targetPieceCount(int w, int h) const;
    tetris::core::TetrominoType randomType();
    tetris::core::Rotation randomRotation();
    float frand(float a, float b);
};

} // namespace tetris::gui_sdl
