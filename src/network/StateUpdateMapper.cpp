#include "network/StateUpdateMapper.hpp"

#include "core/Board.hpp"

namespace tetris::net {

using tetris::core::Board;
using tetris::core::CellState;
using tetris::core::GameStatus;

PlayerStateDTO StateUpdateMapper::toPlayerDTO(
    PlayerId playerId,
    const std::string& playerName,
    const tetris::core::GameState& gs)
{
    PlayerStateDTO dto;
    dto.id   = playerId;
    dto.name = playerName;

    dto.score = static_cast<int>(gs.score()); // Your score() is uint64_t; cast for DTO.
    dto.level = gs.level();

    // Alive if game is not over.
    dto.isAlive = (gs.status() != GameStatus::GameOver);

    // --- Board snapshot ---
    const Board& board = gs.board();
    dto.board.width  = board.cols(); // width = number of columns
    dto.board.height = board.rows(); // height = number of rows

    const int width  = dto.board.width;
    const int height = dto.board.height;

    dto.board.cells.clear();
    dto.board.cells.resize(width * height);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const auto state = board.cell(row, col);
            const bool occupied = (state == CellState::Filled);

            BoardCellDTO cellDto;
            cellDto.occupied   = occupied;
            cellDto.colorIndex = occupied ? 0 : -1;

            const int index = row * width + col;
            dto.board.cells[index] = cellDto;
        }
    }

    return dto;
}

} // namespace tetris::net
