#include <catch2/catch_test_macros.hpp>

#include "core/GameState.hpp"
#include "core/Board.hpp"
#include "core/Tetromino.hpp"

using namespace tetris::core;

TEST_CASE("GameState starts and spawns an active tetromino", "[gamestate]") {
    GameState game{20, 10, 0};

    REQUIRE(game.status() == GameStatus::NotStarted);
    REQUIRE(!game.activeTetromino().has_value());


    game.start();

    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());
    REQUIRE(game.board().rows() == 20);
    REQUIRE(game.board().cols() == 10);
}

TEST_CASE("GameState movement commands do not crash", "[gamestate]") {
    GameState game{20, 10, 0};
    game.start();

    REQUIRE(game.status() == GameStatus::Running);

    // Just ensure these calls are valid in Running state
    game.moveLeft();
    game.moveRight();
    game.softDrop();
    game.rotateClockwise();
    game.rotateCounterClockwise();
    game.hardDrop(); // may or may not cause game over depending on position

    // Game may still be running or may be game over; we don't assert that yet.
}

TEST_CASE("GameState tick causes gravity movement or locking", "[gamestate]") {
    GameState game{20, 10, 0};
    game.start();

    REQUIRE(game.status() == GameStatus::Running);
    auto before = game.activeTetromino();
    REQUIRE(before.has_value());

    bool movedOrLocked = game.tick(); // either true (moved down) or false (locked)
    (void)movedOrLocked; // we don't assert here; just ensure tick runs

    // After enough ticks, game should still behave consistently
    for (int i = 0; i < 200; ++i) {
        if (game.status() != GameStatus::Running) {
            break;
        }
        game.tick();
    }

    REQUIRE((game.status() == GameStatus::Running || game.status() == GameStatus::GameOver));
}

TEST_CASE("GameState restart resets score and status", "[gamestate]") {
    GameState game{20, 10, 0};
    game.start();

    // Make some progress
    for (int i = 0; i < 50; ++i) {
        if (game.status() != GameStatus::Running) break;
        game.softDrop();
        game.tick();
    }

    game.reset();
    REQUIRE(game.status() == GameStatus::NotStarted);
    REQUIRE(game.score() == 0);
}


// For now, we just verify high-level invariants:
// Start → Running
// Active piece exists
// Commands don’t crash
// Reset clears score and state