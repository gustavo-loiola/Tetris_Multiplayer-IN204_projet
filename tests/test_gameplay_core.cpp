#include <catch2/catch_test_macros.hpp>

#include "core/GameState.hpp"
#include "controller/GameController.hpp"
#include "controller/InputAction.hpp"

using tetris::core::GameState;
using tetris::core::GameStatus;
using tetris::controller::GameController;
using tetris::controller::InputAction;

TEST_CASE("GameplayLoop: start spawns active tetromino and enters Running", "[gameplay][loop]")
{
    GameState game{20, 10, 0};

    REQUIRE(game.status() == GameStatus::NotStarted);
    REQUIRE_FALSE(game.activeTetromino().has_value());

    game.start();

    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());
    CHECK(game.board().rows() == 20);
    CHECK(game.board().cols() == 10);
}

TEST_CASE("GameplayLoop: basic player actions do not crash in Running", "[gameplay][loop][actions]")
{
    GameState game{20, 10, 0};
    game.start();
    REQUIRE(game.status() == GameStatus::Running);

    // Just ensure calls are valid in Running
    game.moveLeft();
    game.moveRight();
    game.softDrop();
    game.rotateClockwise();
    game.rotateCounterClockwise();
    game.hardDrop();

    REQUIRE((game.status() == GameStatus::Running || game.status() == GameStatus::GameOver));
}

TEST_CASE("GameplayLoop: GameController maps lateral inputs to GameState movement", "[gameplay][controller][input]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const auto before = game.activeTetromino()->origin();

    controller.handleAction(InputAction::MoveLeft);
    REQUIRE(game.activeTetromino().has_value());
    const auto afterLeft = game.activeTetromino()->origin();
    CHECK(afterLeft.row == before.row);
    CHECK(afterLeft.col == before.col - 1);

    controller.handleAction(InputAction::MoveRight);
    REQUIRE(game.activeTetromino().has_value());
    const auto afterRight = game.activeTetromino()->origin();
    CHECK(afterRight.row == before.row);
    CHECK(afterRight.col == before.col);
}

TEST_CASE("GameplayLoop: PauseResume toggles paused and blocks gravity updates", "[gameplay][controller][pause]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const int intervalMs = game.gravityIntervalMs();
    REQUIRE(intervalMs > 0);

    const auto before = game.activeTetromino()->origin();

    // Pause
    controller.handleAction(InputAction::PauseResume);
    REQUIRE(game.status() == GameStatus::Paused);

    // Even with time passing, position shouldn't change while paused
    controller.update(GameController::Duration{intervalMs * 3});

    REQUIRE(game.activeTetromino().has_value());
    const auto after = game.activeTetromino()->origin();

    CHECK(after.row == before.row);
    CHECK(after.col == before.col);

    // Resume
    controller.handleAction(InputAction::PauseResume);
    REQUIRE(game.status() == GameStatus::Running);
}

TEST_CASE("GameplayLoop: GameController update applies gravity (piece moves down)", "[gameplay][controller][gravity]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const int intervalMs = game.gravityIntervalMs();
    REQUIRE(intervalMs > 0);

    const auto before = game.activeTetromino()->origin();

    controller.update(GameController::Duration{intervalMs});

    REQUIRE(game.activeTetromino().has_value());
    const auto after = game.activeTetromino()->origin();

    // Typically should go down at least one row (unless immediately locked/spawn edge case)
    CHECK(after.col == before.col);
    CHECK(after.row >= before.row);
    CHECK((after.row > before.row || game.lockedPieces() > 0));
}

TEST_CASE("GameplayLoop: large elapsed time triggers multiple gravity ticks (or locks)", "[gameplay][controller][gravity][multi]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const int intervalMs = game.gravityIntervalMs();
    REQUIRE(intervalMs > 0);

    const auto before = game.activeTetromino()->origin();
    const auto lockedBefore = game.lockedPieces();

    controller.update(GameController::Duration{intervalMs * 3});

    // Either piece went down, or it locked (increasing lockedPieces)
    CHECK((game.status() == GameStatus::Running || game.status() == GameStatus::GameOver));
    CHECK((game.lockedPieces() > lockedBefore) ||
          (game.activeTetromino().has_value() && game.activeTetromino()->origin().row >= before.row));
}

TEST_CASE("GameplayLoop: tick runs repeatedly and game stays consistent", "[gameplay][loop][tick]")
{
    GameState game{20, 10, 0};
    game.start();

    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    // Run a bunch of ticks; should never crash and should end Running or GameOver
    for (int i = 0; i < 250; ++i) {
        if (game.status() != GameStatus::Running) break;
        game.tick();
    }

    REQUIRE((game.status() == GameStatus::Running || game.status() == GameStatus::GameOver));
}

TEST_CASE("GameplayLoop: reset clears score and returns to NotStarted", "[gameplay][loop][reset]")
{
    GameState game{20, 10, 0};
    game.start();

    // Make some progress
    for (int i = 0; i < 50; ++i) {
        if (game.status() != GameStatus::Running) break;
        game.softDrop();
        game.tick();
    }

    game.reset();

    CHECK(game.status() == GameStatus::NotStarted);
    CHECK(game.score() == 0);
    CHECK_FALSE(game.activeTetromino().has_value());
}
