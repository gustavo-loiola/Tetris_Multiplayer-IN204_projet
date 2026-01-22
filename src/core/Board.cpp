#include "core/Board.hpp"
#include <stdexcept>

namespace tetris::core {

Board::Board(int rows, int cols)
    : rows_{rows}
    , cols_{cols}
    , grid_(rows * cols, CellState::Empty)
    , typeGrid_(rows * cols, std::nullopt)
{
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("Board dimensions must be positive");
    }
}

CellState Board::cell(int row, int col) const {
    if (!isInside(row, col)) {
        throw std::out_of_range("Board::cell out of range");
    }
    return grid_[index(row, col)];
}

void Board::setCell(int row, int col, CellState state) {
    if (!isInside(row, col)) {
        throw std::out_of_range("Board::setCell out of range");
    }
    grid_[index(row, col)] = state;
    if (state == CellState::Empty) {
        typeGrid_[index(row, col)] = std::nullopt;
    }
}

std::optional<TetrominoType> Board::cellType(int row, int col) const {
    if (!isInside(row, col)) {
        throw std::out_of_range("Board::cellType out of range");
    }
    return typeGrid_[index(row, col)];
}


bool Board::canPlace(const Tetromino& tetromino) const noexcept {
    auto blocks = tetromino.blocks();
    for (const auto& b : blocks) {
        if (!isInside(b.row, b.col)) {
            return false; // out of board
        }
        if (grid_[index(b.row, b.col)] == CellState::Filled) {
            return false; // collision
        }
    }
    return true;
}

void Board::lockTetromino(const Tetromino& tetromino) {
    auto blocks = tetromino.blocks();
    for (const auto& b : blocks) {
        if (isInside(b.row, b.col)) {
            grid_[index(b.row, b.col)] = CellState::Filled;
            typeGrid_[index(b.row, b.col)] = tetromino.type();
        }
    }
}

int Board::clearFullLines() {
    int cleared = 0;

    // Go bottom-up: when we clear, we shift everything above down
    for (int row = rows_ - 1; row >= 0; --row) {
        bool full = true;
        for (int col = 0; col < cols_; ++col) {
            if (grid_[index(row, col)] == CellState::Empty) {
                full = false;
                break;
            }
        }

        if (full) {
            // Shift rows above down by 1
            for (int r = row; r > 0; --r) {
                for (int c = 0; c < cols_; ++c) {
                    grid_[index(r, c)] = grid_[index(r - 1, c)];
                    typeGrid_[index(r, c)] = typeGrid_[index(r - 1, c)];
                }
            }
            // Clear top row
            for (int c = 0; c < cols_; ++c) {
                grid_[index(0, c)] = CellState::Empty;
                typeGrid_[index(0, c)] = std::nullopt;
            }

            ++cleared;
            ++row; // re-check this row index because we just pulled everything down
        }
    }

    return cleared;
}

bool Board::isGameOver() const noexcept {
    // Simple version: if any filled cell in row 0, we say game over.
    for (int col = 0; col < cols_; ++col) {
        if (grid_[index(0, col)] == CellState::Filled) {
            return true;
        }
    }
    return false;
}

} // namespace tetris::core
