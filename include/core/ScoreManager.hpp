#pragma once

#include <cstdint>

namespace tetris::core {

class ScoreManager {
public:
    void addLinesCleared(int lines, int level);

    std::uint64_t score() const noexcept { return score_; }

    void reset() noexcept { score_ = 0; }

private:
    std::uint64_t score_{0};
};

} // namespace tetris::core
