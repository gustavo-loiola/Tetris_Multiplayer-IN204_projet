#include "gui/BoardPanel.hpp"

#include <wx/dcbuffer.h> // for double-buffering
#include <wx/colour.h>
#include <wx/font.h>
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
    drawOverlay(dc);
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

void BoardPanel::drawOverlay(wxDC& dc)
{
    using tetris::core::GameStatus;

    const GameStatus status = game_.status();
    if (status == GameStatus::Running) {
        return; // nada a desenhar
    }

    const wxSize size = GetClientSize();
    const int width  = size.GetWidth();
    const int height = size.GetHeight();

    // Fundo semi-transparente (simples: uma faixa escura no meio)
    wxBrush overlayBrush(wxColour(0, 0, 0, 128)); // preto semi-transparente
    wxPen   overlayPen(*wxTRANSPARENT_PEN);

    dc.SetBrush(overlayBrush);
    dc.SetPen(overlayPen);

    // Retângulo central ocupando 50% da altura
    const int rectWidth  = width * 3 / 4;
    const int rectHeight = height / 3;
    const int rectX      = (width  - rectWidth)  / 2;
    const int rectY      = (height - rectHeight) / 2;

    dc.DrawRectangle(rectX, rectY, rectWidth, rectHeight);

    // Texto: "PAUSED" ou "GAME OVER"
    wxString text;
    if (status == GameStatus::Paused) {
        text = "PAUSED";
    } else if (status == GameStatus::GameOver) {
        text = "GAME OVER";
    } else {
        return;
    }

    // Fonte um pouco maior
    wxFont font = GetFont();
    font.SetPointSize(font.GetPointSize() + 8);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(font);

    dc.SetTextForeground(*wxWHITE);

    wxSize textSize = dc.GetTextExtent(text);
    const int textX = rectX + (rectWidth  - textSize.GetWidth())  / 2;
    const int textY = rectY + (rectHeight - textSize.GetHeight()) / 2;

    dc.DrawText(text, textX, textY);
}

} // namespace tetris::ui::gui
