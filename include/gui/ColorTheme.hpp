#pragma once

#include <wx/colour.h>
#include "core/Types.hpp"  // TetrominoType

namespace tetris::ui::gui {

struct Theme {
    // Fundo geral da janela
    static wxColour windowBackground() { return wxColour(20, 20, 20); }

    // Tabuleiro
    static wxColour boardBackground()  { return wxColour(10, 10, 10); }
    static wxColour boardGrid()        { return wxColour(60, 60, 60); }

    // Blocos travados (como não temos tipo no Board ainda, uma cor neutra)
    static wxColour lockedBlockFill()  { return wxColour(80, 80, 80); }
    static wxColour lockedBlockBorder(){ return wxColour(30, 30, 30); }

    // Painel "Next"
    static wxColour nextBackground()   { return wxColour(25, 25, 25); }
    static wxColour nextGrid()         { return wxColour(70, 70, 70); }

    // Overlay de pause / game over
    static wxColour overlayBackground(){ return wxColour(0, 0, 0, 160); }
    static wxColour overlayText()      { return *wxWHITE; }

    // Barra de status (texto normal)
    static wxColour statusText()       { return *wxWHITE; }

    static wxColour statusRunning()    { return wxColour(0, 220, 0);   }
    static wxColour statusPaused()     { return wxColour(255, 200, 0); }
    static wxColour statusGameOver()   { return wxColour(255, 60, 60); }
};

// Cores por tipo de tetromino
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
    return wxColour(200, 200, 200);
}

} // namespace tetris::ui::gui
