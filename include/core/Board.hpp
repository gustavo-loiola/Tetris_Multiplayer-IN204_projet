#pragma once

#include "Types.hpp"
#include "Tetromino.hpp"
#include <vector>
#include <optional>

namespace tetris::core {

enum class CellState : std::uint8_t {
    Empty,
    Filled // You can later store color / type info if needed
};

class Board {
public:
    Board(int rows, int cols);

    int rows() const noexcept { return rows_; }
    int cols() const noexcept { return cols_; }

    CellState cell(int row, int col) const;
    void setCell(int row, int col, CellState state);

    // If the cell is filled, returns which tetromino type filled it (if known).
    // This is used by GUIs to render colored locked blocks.
    std::optional<TetrominoType> cellType(int row, int col) const;

    // Check if tetromino can be placed (no collision with walls or filled cells)
    bool canPlace(const Tetromino& tetromino) const noexcept;

    // Lock tetromino into the board (mark its blocks as Filled)
    void lockTetromino(const Tetromino& tetromino);

    // Clear full lines, return number of cleared lines
    int clearFullLines();

    // True if any filled cell is in the "spawn zone" (we define spawn rows later)
    bool isGameOver() const noexcept;

private:
    int rows_;
    int cols_;
    std::vector<CellState> grid_; // rows_ * cols_
    std::vector<std::optional<TetrominoType>> typeGrid_; // same size as grid_

    int index(int row, int col) const noexcept {
        return row * cols_ + col;
    }

    bool isInside(int row, int col) const noexcept {
        return row >= 0 && row < rows_ && col >= 0 && col < cols_;
    }
};

} // namespace tetris::core
