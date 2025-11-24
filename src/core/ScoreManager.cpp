#include "core/ScoreManager.hpp"

namespace tetris::core {

void ScoreManager::addLinesCleared(int lines, int level) {
    if (lines <= 0) return;

    const int l = level + 1; // formula uses (level + 1)
    int base = 0;
    switch (lines) {
    case 1: base = 40;   break;
    case 2: base = 100;  break;
    case 3: base = 300;  break;
    case 4: base = 1200; break;
    default:
        // more than 4 lines at once shouldn't happen in Tetris, ignore
        return;
    }

    score_ += static_cast<std::uint64_t>(base * l);
}

} // namespace tetris::core
