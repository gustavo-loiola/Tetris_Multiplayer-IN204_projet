#pragma once

#include <wx/panel.h>

#include "core/GameState.hpp"

namespace tetris::ui::gui {

class BoardPanel : public wxPanel {
public:
    BoardPanel(wxWindow* parent, tetris::core::GameState& game);

private:
    void OnPaint(wxPaintEvent& event);

    void drawBoard(wxDC& dc);
    void drawActiveTetromino(wxDC& dc);
    void drawOverlay(wxDC& dc);

private:
    tetris::core::GameState& game_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::ui::gui
