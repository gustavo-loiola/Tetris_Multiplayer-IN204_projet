#pragma once

#include <cstdint>

namespace tetris::core {

class LevelManager {
public:
    explicit LevelManager(int startingLevel = 0);

    int level() const noexcept { return level_; }
    std::uint64_t totalLinesCleared() const noexcept { return totalLinesCleared_; }

    // Call after lines are cleared; may increase level
    void onLinesCleared(int lines);

    void reset(int startingLevel = 0);

    // Optional: compute current fall interval in milliseconds
    int gravityIntervalMs() const noexcept;

private:
    int level_;
    std::uint64_t totalLinesCleared_;
};

} // namespace tetris::core
