#include "gui/TetrisFrame.hpp"
#include "gui/BoardPanel.hpp"
#include "gui/NextPiecePanel.hpp"
#include "gui/ColorTheme.hpp"

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
    , timerIntervalMs_{16}
    , scoreText_{nullptr}
    , levelText_{nullptr}
    , statusText_{nullptr}
    , hudFlashAccumulatorMs_{0} 
    , hudFlashOn_{true}           
{
    SetBackgroundColour(tetris::ui::gui::Theme::windowBackground());

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

    // --- Top banner ---
    auto* topPanel = new wxPanel(this);
    auto* topSizer = new wxBoxSizer(wxHORIZONTAL);

    scoreText_ = new wxStaticText(topPanel, wxID_ANY, "Score: 0");
    levelText_ = new wxStaticText(topPanel, wxID_ANY, "Level: 0");
    statusText_ = new wxStaticText(topPanel, wxID_ANY, "Status: Running");

    topPanel->SetBackgroundColour(wxColour(30, 30, 30));

    wxFont baseFont = GetFont();
    baseFont.SetPointSize(baseFont.GetPointSize() + 1);
    baseFont.SetWeight(wxFONTWEIGHT_BOLD);

    scoreText_->SetFont(baseFont);
    levelText_->SetFont(baseFont);
    statusText_->SetFont(baseFont);

    topSizer->Add(scoreText_, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 15);
    topSizer->Add(levelText_, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 15);
    topSizer->AddStretchSpacer(1); // empurra status pro lado direito
    topSizer->Add(statusText_, 0, wxALIGN_CENTER_VERTICAL);

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

    // Update game state and UI
    hudFlashAccumulatorMs_ += timerIntervalMs_;
    if (hudFlashAccumulatorMs_ >= 250) { // 250 ms toggle
        hudFlashAccumulatorMs_ = 0;
        hudFlashOn_ = !hudFlashOn_;
    }

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
    using tetris::ui::gui::Theme;

    // Score / Level sempre brancos do tema
    {
        long long scoreValue = static_cast<long long>(game_.score());
        wxString scoreStr = "Score: " + wxString::Format("%lld", scoreValue);
        scoreText_->SetLabel(scoreStr);
        scoreText_->SetForegroundColour(Theme::statusText());
    }

    {
        long long levelValue = static_cast<long long>(game_.level());
        wxString levelStr = "Level: " + wxString::Format("%lld", levelValue);
        levelText_->SetLabel(levelStr);
        levelText_->SetForegroundColour(Theme::statusText());
    }

    // Status: texto + cor (com animação)
    wxString statusStr;
    wxColour statusColor = Theme::statusText();

    const auto status = game_.status();

    switch (status) {
    case GameStatus::NotStarted:
        statusStr = "NotStarted";
        statusColor = Theme::statusText();
        break;

    case GameStatus::Running:
        statusStr = "Running";
        statusColor = Theme::statusRunning();
        break;

    case GameStatus::Paused:
        statusStr = "Paused";
        // piscar entre amarelo forte e cor neutra
        statusColor = hudFlashOn_ ? Theme::statusPaused()
                                  : Theme::statusText();
        break;

    case GameStatus::GameOver:
        statusStr = "GameOver";
        // piscar entre vermelho forte e cor neutra
        statusColor = hudFlashOn_ ? Theme::statusGameOver()
                                  : Theme::statusText();
        break;
    }

    statusText_->SetLabel("Status: " + statusStr);
    statusText_->SetForegroundColour(statusColor);

    auto* topPanel = statusText_->GetParent();
    if (topPanel && topPanel->GetSizer()) {
        topPanel->GetSizer()->Layout();
    }
    if (GetSizer()) {
        GetSizer()->Layout();
    }
}

} // namespace tetris::ui::gui
