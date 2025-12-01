#include "gui/TetrisFrame.hpp"
#include "gui/BoardPanel.hpp"
#include "gui/NextPiecePanel.hpp"

#include <wx/sizer.h>
#include <wx/panel.h>

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
    , game_{20, 10, 0}
    , controller_{game_}
    , timer_{this}
{
    setupLayout();
    setupTimer();

    game_.start();
    updateStatusBar();

    Centre();

    Bind(wxEVT_CHAR_HOOK, &TetrisFrame::OnKeyDown, this);
    if (boardPanel_) {
        boardPanel_->SetFocus();
    }
}

void TetrisFrame::setupLayout()
{
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // --- Top banner: static texts for now ---
    auto* topPanel = new wxPanel(this);
    auto* topSizer = new wxBoxSizer(wxHORIZONTAL);

    scoreText_ = new wxStaticText(topPanel, wxID_ANY, "Score: 0");
    levelText_ = new wxStaticText(topPanel, wxID_ANY, "Level: 0");
    statusText_ = new wxStaticText(topPanel, wxID_ANY, "Status: Running");

    topSizer->Add(scoreText_, 0, wxRIGHT, 10);
    topSizer->Add(levelText_, 0, wxRIGHT, 10);
    topSizer->Add(statusText_, 0);

    topPanel->SetSizer(topSizer);
    mainSizer->Add(topPanel, 0, wxEXPAND | wxALL, 5);

    // --- Middle area: board on the left, next-piece on the right ---
    auto* middleSizer = new wxBoxSizer(wxHORIZONTAL);

    boardPanel_ = new BoardPanel(this, game_);
    middleSizer->Add(boardPanel_, 3, wxEXPAND | wxRIGHT, 5);

    nextPanel_ = new NextPiecePanel(this, game_);
    middleSizer->Add(nextPanel_, 1, wxEXPAND);

    mainSizer->Add(middleSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    SetSizer(mainSizer);
    Layout();

    if (boardPanel_) {
        boardPanel_->SetFocus();
    }
}

void TetrisFrame::setupTimer()
{
    timer_.Start(timerIntervalMs_);
}

void TetrisFrame::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    using Duration = tetris::controller::GameController::Duration;

    controller_.update(Duration{timerIntervalMs_});

    updateStatusBar();

    if (boardPanel_) {
        boardPanel_->Refresh();
    }
    if (nextPanel_) {        
        nextPanel_->Refresh();
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
        event.Skip();
        return;
    }

    updateStatusBar();

    if (boardPanel_) {
        boardPanel_->Refresh();
    }
    if (nextPanel_) {       
        nextPanel_->Refresh();
    }
}

void TetrisFrame::updateStatusBar()
{
    if (!scoreText_ || !levelText_ || !statusText_) {
        return;
    }

    using tetris::core::GameStatus;

    // Safer: avoid printf-style format specifiers entirely
    {
        long long scoreValue = static_cast<long long>(game_.score());
        wxString scoreStr = "Score: " + wxString::Format("%lld", scoreValue);
        scoreText_->SetLabel(scoreStr);
    }

    {
        long long levelValue = static_cast<long long>(game_.level());
        wxString levelStr = "Level: " + wxString::Format("%lld", levelValue);
        levelText_->SetLabel(levelStr);
    }

    // Status text
    wxString statusStr;
    switch (game_.status()) {
    case GameStatus::NotStarted: statusStr = "NotStarted"; break;
    case GameStatus::Running:    statusStr = "Running";    break;
    case GameStatus::Paused:     statusStr = "Paused";     break;
    case GameStatus::GameOver:   statusStr = "GameOver";   break;
    }

    statusText_->SetLabel("Status: " + statusStr);
}



} // namespace tetris::ui::gui
