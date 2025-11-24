#include "core/TetrominoFactory.hpp"
#include <random>

namespace tetris::core {

TetrominoFactory::TetrominoFactory()
    : rng_{std::random_device{}()}
{
}

Tetromino TetrominoFactory::createRandom(Position origin) {
    std::uniform_int_distribution<int> dist(0, 6); // 7 types
    TetrominoType type = static_cast<TetrominoType>(dist(rng_));
    return Tetromino{type, Rotation::R0, origin};
}

} // namespace tetris::core
