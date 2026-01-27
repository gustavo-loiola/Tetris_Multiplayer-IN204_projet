#include "core/Tetromino.hpp"

namespace tetris::core {

Tetromino::Tetromino(TetrominoType type, Rotation rotation, Position origin)
    : type_{type}, rotation_{rotation}, origin_{origin}
{
}

void Tetromino::rotateClockwise() noexcept {
    rotation_ = nextRotation(rotation_);
}

void Tetromino::rotateCounterClockwise() noexcept {
    // 3 clockwise rotations = 1 counter-clockwise
    rotation_ = static_cast<Rotation>(
        (static_cast<std::uint8_t>(rotation_) + 3U) % 4U
    );
}

Tetromino::Shape Tetromino::blocks() const noexcept {
    Shape rel = shapeFor(type_, rotation_);
    Shape abs{};
    for (int i = 0; i < BlockCount; ++i) {
        abs[i].row = origin_.row + rel[i].row;
        abs[i].col = origin_.col + rel[i].col;
    }
    return abs;
}

Tetromino::Shape Tetromino::shapeFor(TetrominoType type, Rotation rotation) noexcept {
    using S = Tetromino::Shape;

    switch (type) {
    case TetrominoType::I:
        switch (rotation) {
        case Rotation::R0:
        case Rotation::R180:
            // Horizontal I:  [ ][ ][ ][ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {0, 2}
            }};
        case Rotation::R90:
        case Rotation::R270:
            // Vertical I:
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {2, 0}
            }};
        }
        break;

    case TetrominoType::O:
        // O piece is same in all rotations: 2x2 block
        return S{{
            {0, 0}, {0, 1},
            {1, 0}, {1, 1}
        }};

    case TetrominoType::T:
        switch (rotation) {
        case Rotation::R0:
            //   [ ]
            // [ ][T][ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {1, 0}
            }};
        case Rotation::R90:
            // [ ]
            // [T][ ]
            // [ ]
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {0, 1}
            }};
        case Rotation::R180:
            // [ ][T][ ]
            //   [ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {-1, 0}
            }};
        case Rotation::R270:
            //   [ ]
            // [T][ ]
            //   [ ]
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {0, -1}
            }};
        }
        break;

    case TetrominoType::L:
        switch (rotation) {
        case Rotation::R0:
            //     [ ]
            // [ ][L][ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {1, 1}
            }};
        case Rotation::R90:
            // [ ]
            // [ ]
            // [L][ ]
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {1, -1}
            }};
        case Rotation::R180:
            // [L][ ][ ]
            // [ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {-1, -1}
            }};
        case Rotation::R270:
            // [L][ ]
            //   [ ]
            //   [ ]
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {-1, 1}
            }};
        }
        break;

    case TetrominoType::J:
        switch (rotation) {
        case Rotation::R0:
            // [ ]
            // [J][ ][ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {1, -1}
            }};
        case Rotation::R90:
            // [J][ ]
            // [ ]
            // [ ]
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {-1, -1}
            }};
        case Rotation::R180:
            // [ ][ ][J]
            //       [ ]
            return S{{
                {0, -1}, {0, 0}, {0, 1}, {-1, 1}
            }};
        case Rotation::R270:
            //   [ ]
            //   [ ]
            // [J][ ]
            return S{{
                {-1, 0}, {0, 0}, {1, 0}, {1, 1}
            }};
        }
        break;

    case TetrominoType::S:
        switch (rotation) {
        case Rotation::R0:
        case Rotation::R180:
            //   [S][ ]
            // [ ][S]
            return S{{
                {0, 0}, {0, 1}, {1, -1}, {1, 0}
            }};
        case Rotation::R90:
        case Rotation::R270:
            // [ ]
            // [S][ ]
            //   [S]
            return S{{
                {-1, 0}, {0, 0}, {0, 1}, {1, 1}
            }};
        }
        break;

    case TetrominoType::Z:
        switch (rotation) {
        case Rotation::R0:
        case Rotation::R180:
            // [Z][ ]
            //   [Z][ ]
            return S{{
                {0, -1}, {0, 0}, {1, 0}, {1, 1}
            }};
        case Rotation::R90:
        case Rotation::R270:
            //   [ ]
            // [Z][ ]
            // [ ]
            return S{{
                {-1, 1}, {0, 0}, {0, 1}, {1, 0}
            }};
        }
        break;
    }

    // Fallback (should never happen)
    return S{{ {0,0}, {0,0}, {0,0}, {0,0} }};
}

} // namespace tetris::core
