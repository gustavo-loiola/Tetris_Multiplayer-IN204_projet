#pragma once

#include <wx/panel.h>
#include <wx/timer.h>
#include <vector>

#include "network/MessageTypes.hpp" // for BoardDTO, BoardCellDTO

namespace tetris::gui {

/// Simple wxWidgets panel that renders a Tetris board from a network BoardDTO.
/// It does not own any game logic; it just draws what it is given.
class RemoteBoardPanel : public wxPanel {
public:
    explicit RemoteBoardPanel(wxWindow* parent,
                              wxWindowID id = wxID_ANY,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxDefaultSize,
                              long style = wxTAB_TRAVERSAL,
                              const wxString& name = wxASCII_STR("RemoteBoardPanel"));

    /// Update the board contents from a network DTO and trigger a repaint.
    void SetBoard(const tetris::net::BoardDTO& board);

protected:
    void OnPaint(wxPaintEvent& event);

private:
    tetris::net::BoardDTO m_board;

    // Helper to get a cell safely; returns nullptr if out of range.
    const tetris::net::BoardCellDTO* CellAt(int row, int col) const;

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::gui
