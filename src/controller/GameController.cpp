#include "controller/GameController.hpp"

namespace tetris::controller {

GameController::GameController(tetris::core::GameState& game)
    : game_{game}
{
}

void GameController::handleAction(InputAction  action) {
    using core::GameStatus;

    const auto status = game_.status();

    // Note: when GameOver, we deliberately ignore all actions here.
    // External code (UI) should call game_.reset() + game_.start()
    // and then resetTiming() when the player wants to restart.
    if (game_.status() == GameStatus::GameOver) {
        return;
    }

    switch (action) {
    case InputAction::MoveLeft:
        if (status == GameStatus::Running) {
            game_.moveLeft();
        }
        break;
    case InputAction::MoveRight:
        if (status == GameStatus::Running) {
            game_.moveRight();
        }
        break;
    case InputAction::SoftDrop:
        if (status == GameStatus::Running) {
            game_.softDrop();
        }
        break;
    case InputAction::HardDrop:
        if (status == GameStatus::Running) {
            game_.hardDrop();
            // After a hard drop, reset gravity accumulator so the next
            // piece does not instantly tick.
            accumulated_ = Duration{0};
        }
        break;
    case InputAction::RotateCW:
        if (status == GameStatus::Running) {
            game_.rotateClockwise();
        }
        break;
    case InputAction::RotateCCW:
        if (status == GameStatus::Running) {
            game_.rotateCounterClockwise();
        }
        break;
    case InputAction::PauseResume:
        if (status == GameStatus::Running) {
            game_.pause();
        } else if (status == GameStatus::Paused) {
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

    const int intervalMs = game_.gravityIntervalMs(); // forward to LevelManager
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