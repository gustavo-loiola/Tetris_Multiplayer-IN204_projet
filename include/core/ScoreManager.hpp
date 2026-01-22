#pragma once

#include <cstdint>

namespace tetris::core {

class ScoreManager {
public:
    void addLinesCleared(int lines, int level);

    // Drop points (small bonus)
    // Soft drop: +1 per cell
    // Hard drop: +2 per cell
    void addSoftDropCells(int cells);
    void addHardDropCells(int cells);

    std::uint64_t score() const noexcept { return score_; }

    void reset() noexcept { score_ = 0; }

private:
    std::uint64_t score_{0};
};

} // namespace tetris::core