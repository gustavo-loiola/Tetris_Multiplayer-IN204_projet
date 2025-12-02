#include "gui/RemoteBoardPanel.hpp"

#include <wx/dcbuffer.h>

namespace tetris::gui {

wxBEGIN_EVENT_TABLE(RemoteBoardPanel, wxPanel)
    EVT_PAINT(RemoteBoardPanel::OnPaint)
wxEND_EVENT_TABLE()

RemoteBoardPanel::RemoteBoardPanel(wxWindow* parent,
                                   wxWindowID id,
                                   const wxPoint& pos,
                                   const wxSize& size,
                                   long style,
                                   const wxString& name)
    : wxPanel(parent, id, pos, size, style, name)
{
    // Use a buffered background to avoid flicker when repainting frequently.
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void RemoteBoardPanel::SetBoard(const tetris::net::BoardDTO& board)
{
    m_board = board;
    Refresh(false); // schedule a repaint
}

const tetris::net::BoardCellDTO* RemoteBoardPanel::CellAt(int row, int col) const
{
    if (m_board.width <= 0 || m_board.height <= 0) {
        return nullptr;
    }
    if (row < 0 || row >= m_board.height || col < 0 || col >= m_board.width) {
        return nullptr;
    }
    const std::size_t idx =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(m_board.width)
        + static_cast<std::size_t>(col);
    if (idx >= m_board.cells.size()) {
        return nullptr;
    }
    return &m_board.cells[idx];
}

void RemoteBoardPanel::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxAutoBufferedPaintDC dc(this);

    // Clear background
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();

    const int rows = m_board.height;
    const int cols = m_board.width;
    if (rows <= 0 || cols <= 0) {
        return; // nothing to draw yet
    }

    // Compute cell size based on current panel size
    const wxSize sz = GetClientSize();
    const int cellWidth  = sz.GetWidth()  / std::max(1, cols);
    const int cellHeight = sz.GetHeight() / std::max(1, rows);
    const int cellSize   = std::min(cellWidth, cellHeight);
    if (cellSize <= 0) {
        return;
    }

    // Simple color palette indexed by BoardCellDTO::colorIndex.
    // You can adjust this to match your single-player palette.
    static const wxColour colors[] = {
        wxColour(  0,   0,   0), // 0 = empty (unused)
        wxColour(  0, 255, 255), // 1 = I
        wxColour(  0,   0, 255), // 2 = J
        wxColour(255, 165,   0), // 3 = L
        wxColour(255, 255,   0), // 4 = O
        wxColour(  0, 255,   0), // 5 = S
        wxColour(160,  32, 240), // 6 = T
        wxColour(255,   0,   0)  // 7 = Z
    };
    constexpr int colorCount = static_cast<int>(sizeof(colors) / sizeof(colors[0]));

    dc.SetPen(wxPen(wxColour(40, 40, 40))); // grid lines

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int x = c * cellSize;
            const int y = (rows - 1 - r) * cellSize; // draw bottom row at bottom

            const auto* cell = CellAt(r, c);
            bool occupied = cell && cell->occupied;
            int colorIndex = (cell && cell->colorIndex >= 0) ? cell->colorIndex : 0;
            if (colorIndex < 0) colorIndex = 0;
            if (colorIndex >= colorCount) colorIndex = colorIndex % colorCount;

            if (occupied) {
                dc.SetBrush(wxBrush(colors[colorIndex]));
            } else {
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
            }

            dc.DrawRectangle(x, y, cellSize, cellSize);
        }
    }
}

} // namespace tetris::gui
