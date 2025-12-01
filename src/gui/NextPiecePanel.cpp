#include "gui/NextPiecePanel.hpp"
#include "gui/ColorTheme.hpp"

#include <wx/dcbuffer.h>
#include <wx/font.h>
#include <algorithm>

namespace tetris::ui::gui {

wxBEGIN_EVENT_TABLE(NextPiecePanel, wxPanel)
    EVT_PAINT(NextPiecePanel::OnPaint)
wxEND_EVENT_TABLE()

NextPiecePanel::NextPiecePanel(wxWindow* parent, tetris::core::GameState& game)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxBORDER_SIMPLE | wxFULL_REPAINT_ON_RESIZE)
    , game_{game}
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void NextPiecePanel::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    drawTitle(dc);
    drawNextTetromino(dc);
}

void NextPiecePanel::drawTitle(wxDC& dc)
{
    wxString text = "Next";

    wxFont font = GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(font);

    dc.SetTextForeground(*wxWHITE);

    wxSize size = GetClientSize();
    wxSize textSize = dc.GetTextExtent(text);

    int x = (size.GetWidth() - textSize.GetWidth()) / 2;
    int y = 5; // topo

    dc.DrawText(text, x, y);
}

void NextPiecePanel::drawNextTetromino(wxDC& dc)
{
    using namespace tetris::core;

    auto const& maybeNext = game_.nextTetromino();
    if (!maybeNext) {
        return;
    }

    // Pegar os blocos da peça
    Tetromino t = *maybeNext;
    auto blocks = t.blocks();
    if (blocks.empty()) {
        return;
    }

    // Normalizar para uma pequena grade, ex: 4x4
    int minRow = blocks[0].row;
    int maxRow = blocks[0].row;
    int minCol = blocks[0].col;
    int maxCol = blocks[0].col;

    for (const auto& b : blocks) {
        minRow = std::min(minRow, b.row);
        maxRow = std::max(maxRow, b.row);
        minCol = std::min(minCol, b.col);
        maxCol = std::max(maxCol, b.col);
    }

    // Tamanho da área de desenho abaixo do título
    wxSize size = GetClientSize();
    const int topMargin = 30; // espaço para título
    const int availableWidth  = size.GetWidth();
    const int availableHeight = size.GetHeight() - topMargin;

    using tetris::ui::gui::Theme;

    // Fundo do painel "Next"
    dc.SetBrush(wxBrush(Theme::nextBackground()));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

    const int previewRows = 4;
    const int previewCols = 4;

    const int cellWidth  = availableWidth  / previewCols;
    const int cellHeight = availableHeight / previewRows;
    const int cellSize = std::min(cellWidth, cellHeight);

    if (cellSize <= 0) {
        return;
    }

    // Centralizar a grade 4x4
    const int offsetX = (availableWidth  - cellSize * previewCols) / 2;
    const int offsetY = topMargin + (availableHeight - cellSize * previewRows) / 2;

    // Preencher fundo levemente cinza
    dc.SetPen(wxPen(Theme::nextGrid()));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    for (int r = 0; r < previewRows; ++r) {
        for (int c = 0; c < previewCols; ++c) {
            wxRect rect(
                offsetX + c * cellSize,
                offsetY + r * cellSize,
                cellSize,
                cellSize
            );
            dc.DrawRectangle(rect);
        }
    }

    // Desenhar blocos da peça, mapeados para dentro da grade 4x4
    const wxColour color = tetris::ui::gui::colorForTetromino(t.type());
    dc.SetBrush(wxBrush(color));
    dc.SetPen(*wxBLACK_PEN);

    // Mapear a bounding box da peça para o centro da grade 4x4
    const int pieceWidth  = (maxCol - minCol + 1);
    const int pieceHeight = (maxRow - minRow + 1);

    const int offsetCols = (previewCols - pieceWidth)  / 2;
    const int offsetRows = (previewRows - pieceHeight) / 2;

    for (const auto& b : blocks) {
        int localRow = b.row - minRow + offsetRows;
        int localCol = b.col - minCol + offsetCols;

        if (localRow < 0 || localRow >= previewRows ||
            localCol < 0 || localCol >= previewCols) {
            continue;
        }

        wxRect rect(
            offsetX + localCol * cellSize,
            offsetY + localRow * cellSize,
            cellSize,
            cellSize
        );

        dc.DrawRectangle(rect.Deflate(1, 1));
    }
}

} // namespace tetris::ui::gui
