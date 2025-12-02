#include <catch2/catch_test_macros.hpp>

#include "network/StateUpdateMapper.hpp"
#include "core/GameState.hpp"
#include "core/Board.hpp"

TEST_CASE("StateUpdateMapper: snapshot after some moves",
          "[network][stateupdate][mapper]")
{
    using namespace tetris;

    core::GameState gs; // default 20x10

    // --- Perform some moves (exact outcome is not important) ---
    gs.start();                 // start the game (spawn first tetromino)
    gs.moveLeft();              // arbitrary input
    gs.rotateClockwise();       // arbitrary input
    gs.softDrop();              // maybe moves piece down one
    gs.tick();                  // let gravity act once

    // Snapshot to DTO
    const net::PlayerId playerId = 1u;
    const std::string playerName = "Bob";
    net::PlayerStateDTO dto =
        net::StateUpdateMapper::toPlayerDTO(playerId, playerName, gs);

    const auto& board = gs.board();

    // Sanity checks on dimensions
    REQUIRE(dto.board.width  == board.cols());
    REQUIRE(dto.board.height == board.rows());
    REQUIRE(dto.board.cells.size()
            == static_cast<std::size_t>(board.rows() * board.cols()));

    // Compute whether the *real* board has any filled cells
    bool boardHasFilled = false;
    for (int row = 0; row < board.rows(); ++row) {
        for (int col = 0; col < board.cols(); ++col) {
            if (board.cell(row, col) == core::CellState::Filled) {
                boardHasFilled = true;
                break;
            }
        }
        if (boardHasFilled) break;
    }

    // Compute whether the DTO has any occupied cells
    bool dtoHasOccupied = false;
    for (const auto& cellDto : dto.board.cells) {
        if (cellDto.occupied) {
            dtoHasOccupied = true;
            break;
        }
    }

    // The mapper should preserve the presence/absence of filled cells
    CHECK(dtoHasOccupied == boardHasFilled);
}

TEST_CASE("StateUpdateMapper: game over state",
          "[network][stateupdate][mapper]")
{
    using namespace tetris;

    core::GameState gs; // default 20x10

    // Simulate game over
    gs.start();
    while (gs.status() != core::GameStatus::GameOver) {
        gs.hardDrop();
    }

    // Snapshot to DTO
    const net::PlayerId playerId = 2u;
    const std::string playerName = "Alice";
    net::PlayerStateDTO dto =
        net::StateUpdateMapper::toPlayerDTO(playerId, playerName, gs);

    // Check isAlive flag
    CHECK(dto.isAlive == false);
}