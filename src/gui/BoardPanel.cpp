#include "gui/BoardPanel.hpp"

#include <wx/dcbuffer.h> // for double-buffering
#include <algorithm>

namespace tetris::ui::gui {

wxBEGIN_EVENT_TABLE(BoardPanel, wxPanel)
    EVT_PAINT(BoardPanel::OnPaint)
wxEND_EVENT_TABLE()

BoardPanel::BoardPanel(wxWindow* parent, tetris::core::GameState& game)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxBORDER_SIMPLE | wxFULL_REPAINT_ON_RESIZE)
    , game_{game}
{
    SetBackgroundStyle(wxBG_STYLE_PAINT); // needed for wxBufferedPaintDC
}

void BoardPanel::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    drawBoard(dc);
    drawActiveTetromino(dc);
}

void BoardPanel::drawBoard(wxDC& dc)
{
    const auto& board = game_.board();
    const int rows = board.rows();
    const int cols = board.cols();

    if (rows <= 0 || cols <= 0) {
        return;
    }

    // Compute cell size based on panel size
    const wxSize size = GetClientSize();
    const int cellWidth  = size.GetWidth()  / cols;
    const int cellHeight = size.GetHeight() / rows;
    const int cellSize = std::min(cellWidth, cellHeight);

    // Center the board
    const int offsetX = (size.GetWidth()  - cellSize * cols) / 2;
    const int offsetY = (size.GetHeight() - cellSize * rows) / 2;

    // Draw background grid + locked blocks
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            wxRect cellRect(
                offsetX + c * cellSize,
                offsetY + r * cellSize,
                cellSize,
                cellSize
            );

            // Cell border
            dc.SetPen(*wxLIGHT_GREY_PEN);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(cellRect);

            // Locked blocks
            if (board.cell(r, c) == tetris::core::CellState::Filled) {
                dc.SetBrush(*wxBLUE_BRUSH);
                dc.SetPen(*wxBLACK_PEN);
                dc.DrawRectangle(cellRect.Deflate(1, 1));
            }
        }
    }
}

void BoardPanel::drawActiveTetromino(wxDC& dc)
{
    using namespace tetris::core;

    const auto& board = game_.board();
    const int rows = board.rows();
    const int cols = board.cols();

    if (!game_.activeTetromino()) {
        return;
    }

    const wxSize size = GetClientSize();
    const int cellWidth  = size.GetWidth()  / cols;
    const int cellHeight = size.GetHeight() / rows;
    const int cellSize = std::min(cellWidth, cellHeight);

    const int offsetX = (size.GetWidth()  - cellSize * cols) / 2;
    const int offsetY = (size.GetHeight() - cellSize * rows) / 2;

    auto blocks = game_.activeTetromino()->blocks();

    dc.SetBrush(*wxRED_BRUSH);
    dc.SetPen(*wxBLACK_PEN);

    for (const auto& b : blocks) {
        if (b.row < 0 || b.row >= rows || b.col < 0 || b.col >= cols) {
            continue;
        }

        wxRect cellRect(
            offsetX + b.col * cellSize,
            offsetY + b.row * cellSize,
            cellSize,
            cellSize
        );

        dc.DrawRectangle(cellRect.Deflate(1, 1));
    }
}

} // namespace tetris::ui::gui
