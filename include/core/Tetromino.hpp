#pragma once // Include guard

#include "Types.hpp" // For Position, Rotation, TetrominoType
#include <array> // For std::array

// Namespace for Tetris core types
namespace tetris::core {

// Represents a Tetromino piece in Tetris
class Tetromino {
public:
    static constexpr int BlockCount = 4;

    using Shape = std::array<Position, BlockCount>;

    Tetromino(TetrominoType type, Rotation rotation, Position origin);

    TetrominoType type() const noexcept { return type_; }
    Rotation rotation() const noexcept { return rotation_; }
    Position origin() const noexcept { return origin_; }

    void setOrigin(Position p) noexcept { origin_ = p; }
    void rotateClockwise() noexcept;
    void rotateCounterClockwise() noexcept;

    // Positions of the 4 blocks in board coordinates
    Shape blocks() const noexcept;

private:
    TetrominoType type_;
    Rotation rotation_;
    Position origin_; // position of the piece on the board (pivot or reference)

    // For a given type + rotation, returns block offsets relative to origin (0,0)
    static Shape shapeFor(TetrominoType type, Rotation rotation) noexcept;
};

} // namespace tetris::core
