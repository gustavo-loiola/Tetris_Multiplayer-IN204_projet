#pragma once

#include <wx/panel.h>
#include "core/GameState.hpp"

namespace tetris::ui::gui {

class NextPiecePanel : public wxPanel {
public:
    NextPiecePanel(wxWindow* parent, tetris::core::GameState& game);

private:
    void OnPaint(wxPaintEvent& event);

    void drawTitle(wxDC& dc);
    void drawNextTetromino(wxDC& dc);

private:
    tetris::core::GameState& game_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::ui::gui
