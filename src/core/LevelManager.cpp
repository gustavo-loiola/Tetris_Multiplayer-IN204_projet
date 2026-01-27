#include "core/LevelManager.hpp"
#include <algorithm>

namespace tetris::core {

LevelManager::LevelManager(int startingLevel)
    : level_{startingLevel}
    , totalLinesCleared_{0}
{
}

void LevelManager::onLinesCleared(int lines) {
    if (lines <= 0) return;

    totalLinesCleared_ += static_cast<std::uint64_t>(lines);

    // example rule: every 10 lines -> +1 level
    int newLevel = static_cast<int>(totalLinesCleared_ / 10);
    if (newLevel > level_) {
        level_ = newLevel;
    }
}

void LevelManager::reset(int startingLevel) {
    level_ = startingLevel;
    totalLinesCleared_ = 0;
}

int LevelManager::gravityIntervalMs() const noexcept {
    // Very simple speed curve: start at ~800ms, decrease by 70ms per level,
    // but never below 50ms.
    int base = 800;
    int interval = base - level_ * 70;
    return std::max(interval, 50);
}

} // namespace tetris::core
