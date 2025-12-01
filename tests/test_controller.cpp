// tests/test_controller.cpp

#include <catch2/catch_test_macros.hpp>

#include "core/GameState.hpp"
#include "core/Board.hpp"
#include "core/Types.hpp"
#include "controller/GameController.hpp"
#include "controller/InputAction.hpp"

using tetris::core::GameState;
using tetris::core::GameStatus;
using tetris::controller::GameController;
using tetris::controller::InputAction;

TEST_CASE("GameController maps lateral input actions to GameState movement", "[controller]")
{
    GameState game{20, 10, 0}; // rows, cols, startingLevel
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    auto before = game.activeTetromino()->origin();

    // Move left
    controller.handleAction(InputAction::MoveLeft);
    auto afterLeft = game.activeTetromino()->origin();

    REQUIRE(afterLeft.row == before.row);
    REQUIRE(afterLeft.col == before.col - 1);

    // Move right (back to original column)
    controller.handleAction(InputAction::MoveRight);
    auto afterRight = game.activeTetromino()->origin();

    REQUIRE(afterRight.row == before.row);
    REQUIRE(afterRight.col == before.col);
}

TEST_CASE("GameController toggles pause/resume", "[controller]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);

    controller.handleAction(InputAction::PauseResume);
    REQUIRE(game.status() == GameStatus::Paused);

    controller.handleAction(InputAction::PauseResume);
    REQUIRE(game.status() == GameStatus::Running);
}

TEST_CASE("GameController update applies gravity based on GameState interval", "[controller]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const int intervalMs = game.gravityIntervalMs();
    REQUIRE(intervalMs > 0);

    auto before = game.activeTetromino()->origin();

    // Simular passagem de exatamente 1 intervalo de gravidade
    controller.update(GameController::Duration{intervalMs});

    REQUIRE(game.activeTetromino().has_value());
    auto after = game.activeTetromino()->origin();

    // A peça deve ter descido pelo menos uma linha
    REQUIRE(after.row > before.row);
    REQUIRE(after.col == before.col);
}

TEST_CASE("GameController update does not move piece when paused", "[controller]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const int intervalMs = game.gravityIntervalMs();
    REQUIRE(intervalMs > 0);

    auto before = game.activeTetromino()->origin();

    // Pausar o jogo
    controller.handleAction(InputAction::PauseResume);
    REQUIRE(game.status() == GameStatus::Paused);

    // Mesmo com tempo passando, a peça não deve se mover
    controller.update(GameController::Duration{intervalMs * 3});

    REQUIRE(game.activeTetromino().has_value());
    auto after = game.activeTetromino()->origin();

    REQUIRE(after.row == before.row);
    REQUIRE(after.col == before.col);
}

TEST_CASE("GameController handles multiple gravity ticks on large elapsed time", "[controller]")
{
    GameState game{20, 10, 0};
    GameController controller{game};

    game.start();
    REQUIRE(game.status() == GameStatus::Running);
    REQUIRE(game.activeTetromino().has_value());

    const int intervalMs = game.gravityIntervalMs();
    REQUIRE(intervalMs > 0);

    auto before = game.activeTetromino()->origin();

    // Forçar um delta grande, por exemplo 3 intervalos
    const int factor = 3;
    controller.update(GameController::Duration{intervalMs * factor});

    REQUIRE(game.activeTetromino().has_value());
    auto after = game.activeTetromino()->origin();

    // Em condições normais (peça spawnando próximo do topo),
    // ela deve ter descido várias linhas.
    REQUIRE(after.row >= before.row + 1);
    REQUIRE(after.col == before.col);
}
