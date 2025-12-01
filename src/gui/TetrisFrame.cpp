#include "gui/TetrisFrame.hpp"
#include "gui/BoardPanel.hpp"

#include <wx/sizer.h>

namespace tetris::ui::gui {

wxBEGIN_EVENT_TABLE(TetrisFrame, wxFrame)
    EVT_TIMER(wxID_ANY, TetrisFrame::OnTimer)
wxEND_EVENT_TABLE()

TetrisFrame::TetrisFrame(wxWindow* parent,
                         wxWindowID id,
                         const wxString& title)
    : wxFrame(parent, id, title,
              wxDefaultPosition,
              wxSize(400, 600))
    , game_{20, 10, 0}  // rows, cols, starting level
    , controller_{game_}
    , timer_(this)
{
    setupLayout();
    setupTimer();

    // Start the game so there is an active tetromino
    game_.start();

    Centre();

    Bind(wxEVT_CHAR_HOOK, &TetrisFrame::OnKeyDown, this);
    if (boardPanel_) {
        boardPanel_->SetFocus();
    }
}

void TetrisFrame::setupLayout()
{
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    boardPanel_ = new BoardPanel(this, game_);
    mainSizer->Add(boardPanel_, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    Layout();
}

void TetrisFrame::setupTimer()
{
    timer_.Start(timerIntervalMs_);
}

void TetrisFrame::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    using Duration = tetris::controller::GameController::Duration;

    controller_.update(Duration{timerIntervalMs_});

    if (boardPanel_) {
        boardPanel_->Refresh();
    }
}

void TetrisFrame::OnKeyDown(wxKeyEvent& event)
{
    using tetris::controller::InputAction;
    const int key = event.GetKeyCode();

    switch (key) {
    case 'A':
    case WXK_LEFT:
        controller_.handleAction(InputAction::MoveLeft);
        break;
    case 'D':
    case WXK_RIGHT:
        controller_.handleAction(InputAction::MoveRight);
        break;
    case 'S':
    case WXK_DOWN:
        controller_.handleAction(InputAction::SoftDrop);
        break;
    case 'W':
    case WXK_UP:
        controller_.handleAction(InputAction::RotateCW);
        break;
    case WXK_SPACE:
        controller_.handleAction(InputAction::HardDrop);
        break;
    case 'P':
        controller_.handleAction(InputAction::PauseResume);
        break;
    case 'R':
        game_.reset();
        game_.start();
        controller_.resetTiming();
        break;
    default:
        event.Skip(); // allow others
        return;
    }

    if (boardPanel_) {
        boardPanel_->Refresh();
    }
}

} // namespace tetris::ui::gui
