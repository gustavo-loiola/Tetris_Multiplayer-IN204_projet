#pragma once

#include "core/GameState.hpp"
#include <chrono>

namespace tetris::controller {

enum class PlayerAction {
    MoveLeft,
    MoveRight,
    SoftDrop,
    HardDrop,
    RotateCW,
    RotateCCW,
    PauseResume
};

class GameController {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::milliseconds;

    explicit GameController(tetris::core::GameState& game);

    // Called by UI or main loop when some input happens
    void handleAction(PlayerAction action);

    // Called periodically with elapsed time since last call.
    // It accumulates time and performs gravity ticks when the
    // accumulated time exceeds the current gravity interval.
    void update(Duration elapsed);

    // Reset timing accumulator (e.g. when game is reset)
    void resetTiming();

private:
    tetris::core::GameState& game_;
    Duration accumulated_{0};
};

} // namespace tetris::controller
