#include "controller/GameController.hpp"

namespace tetris::controller {

GameController::GameController(tetris::core::GameState& game)
    : game_{game}
{
}

void GameController::handleAction(PlayerAction action) {
    using core::GameStatus;

    // If the game is over, only allow a reset from outside;
    // controller doesn't auto-reset here.
    if (game_.status() == GameStatus::GameOver) {
        if (action == PlayerAction::PauseResume) {
            // people might want to restart; external code should call game_.reset() + game_.start()
        }
        return;
    }

    switch (action) {
    case PlayerAction::MoveLeft:
        game_.moveLeft();
        break;
    case PlayerAction::MoveRight:
        game_.moveRight();
        break;
    case PlayerAction::SoftDrop:
        game_.softDrop();
        break;
    case PlayerAction::HardDrop:
        game_.hardDrop();
        // After a hard drop, we typically reset gravity accumulator
        // so the next piece won't instantly tick.
        accumulated_ = Duration{0};
        break;
    case PlayerAction::RotateCW:
        game_.rotateClockwise();
        break;
    case PlayerAction::RotateCCW:
        game_.rotateCounterClockwise();
        break;
    case PlayerAction::PauseResume:
        if (game_.status() == GameStatus::Running) {
            game_.pause();
        } else if (game_.status() == GameStatus::Paused) {
            game_.resume();
        }
        break;
    }
}

void GameController::update(Duration elapsed) {
    using core::GameStatus;

    if (game_.status() != GameStatus::Running) {
        return;
    }

    accumulated_ += elapsed;

    const int intervalMs = game_.gravityIntervalMs();
    if (intervalMs <= 0) {
        return;
    }

    Duration interval{intervalMs};

    // If a lot of time passed (lag), we might need several ticks
    while (accumulated_ >= interval && game_.status() == GameStatus::Running) {
        bool moved = game_.tick();
        (void)moved; // we don't use this info yet
        accumulated_ -= interval;
    }
}

void GameController::resetTiming() {
    accumulated_ = Duration{0};
}

} // namespace tetris::controller


// This is exactly the logic your GUI will use later:

// A timer fires every, say, 16ms (60 FPS).

// It calls controller.update(16ms).

// When enough time has accumulated to equal gravityIntervalMs(), the controller calls game_.tick() one or more times.