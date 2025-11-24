#pragma once

#include "Types.hpp"
#include "Tetromino.hpp"
#include <random>

namespace tetris::core {

class TetrominoFactory {
public:
    TetrominoFactory();

    // Create next random piece with given origin
    Tetromino createRandom(Position origin);

    // For deterministic testing, you can later inject a custom RNG
private:
    std::mt19937 rng_;
};

} // namespace tetris::core
