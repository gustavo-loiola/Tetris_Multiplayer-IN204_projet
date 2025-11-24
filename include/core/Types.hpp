#pragma once // Include guard

#include <cstdint> // For fixed-width integer types
#include <array> // For std::array

// Namespace for Tetris core types
namespace tetris::core {

// Position structure representing a cell in the Tetris grid
struct Position {
    int row{};
    int col{};
};

// Rotation states for Tetrominoes
enum class Rotation : std::uint8_t {
    R0   = 0,
    R90  = 1,
    R180 = 2,
    R270 = 3
};

// Function to get the next rotation state in a clockwise direction
inline Rotation nextRotation(Rotation r) {
    return static_cast<Rotation>((static_cast<std::uint8_t>(r) + 1U) % 4U);
}

// Tetromino types
enum class TetrominoType : std::uint8_t {
    I, O, T, L, J, S, Z
};

} // namespace tetris::core