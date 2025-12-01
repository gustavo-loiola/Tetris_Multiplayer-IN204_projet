#pragma once

#include <wx/colour.h>
#include "core/Types.hpp"  // para TetrominoType

namespace tetris::ui::gui {

inline wxColour colorForTetromino(tetris::core::TetrominoType type)
{
    using tetris::core::TetrominoType;

    switch (type) {
    case TetrominoType::I: return wxColour(  0, 255, 255); // cyan
    case TetrominoType::O: return wxColour(255, 255,   0); // yellow
    case TetrominoType::T: return wxColour(160,  32, 240); // purple
    case TetrominoType::J: return wxColour(  0,   0, 255); // blue
    case TetrominoType::L: return wxColour(255, 165,   0); // orange
    case TetrominoType::S: return wxColour(  0, 255,   0); // green
    case TetrominoType::Z: return wxColour(255,   0,   0); // red
    }

    // fallback se algo der errado
    return wxColour(200, 200, 200);
}

} // namespace tetris::ui::gui
