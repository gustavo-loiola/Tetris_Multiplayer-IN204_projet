#include "gui/MultiplayerFrame.hpp"

#include <wx/sizer.h>
#include <wx/msgdlg.h>

namespace tetris::gui {

using namespace tetris::net;

wxBEGIN_EVENT_TABLE(MultiplayerFrame, wxFrame)
    EVT_CLOSE(MultiplayerFrame::OnClose)
    EVT_TIMER(wxID_ANY, MultiplayerFrame::OnTimer)
wxEND_EVENT_TABLE()

MultiplayerFrame::MultiplayerFrame(std::shared_ptr<NetworkClient> client,
                                   wxWindow* parent,
                                   wxWindowID id,
                                   const wxString& title,
                                   const wxPoint& pos,
                                   const wxSize& size,
                                   long style)
    : wxFrame(parent, id, title, pos, size, style)
    , m_client(std::move(client))
    , m_refreshTimer(this)
{
    // Basic layout: board on top, HUD labels below.
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    m_boardPanel = new RemoteBoardPanel(this, wxID_ANY, wxDefaultPosition, wxSize(300, 600));
    rootSizer->Add(m_boardPanel, 1, wxEXPAND | wxALL, 5);

    auto* hudSizer = new wxBoxSizer(wxHORIZONTAL);
    m_scoreText  = new wxStaticText(this, wxID_ANY, wxASCII_STR("Score: 0"));
    m_levelText  = new wxStaticText(this, wxID_ANY, wxASCII_STR("Level: 0"));
    m_statusText = new wxStaticText(this, wxID_ANY, wxASCII_STR("Status: Connecting..."));

    hudSizer->Add(m_scoreText,  0, wxRIGHT, 10);
    hudSizer->Add(m_levelText,  0, wxRIGHT, 10);
    hudSizer->Add(m_statusText, 1, wxLEFT,  10);

    rootSizer->Add(hudSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(rootSizer);
    Layout();

    // Subscribe to StateUpdate events from the client.
    if (m_client) {
        m_client->setStateUpdateHandler(
            [this](const StateUpdate& update) {
                // This lambda is executed in whatever thread calls injectIncoming / poll.
                // In your architecture, that should be the GUI/main thread.
                OnStateUpdate(update);
            }
        );

        m_client->setMatchResultHandler(
            [this](const MatchResult& result) {
                OnMatchResult(result);
            }
        );
    }

    // Start a UI refresh timer (e.g. 30Hz). It's cheap and avoids missing repaints.
    m_refreshTimer.Start(33); // ~30 fps
}

void MultiplayerFrame::OnClose(wxCloseEvent& event)
{
    // Stop the timer to avoid callbacks after destruction.
    if (m_refreshTimer.IsRunning()) {
        m_refreshTimer.Stop();
    }

    // You can add client-side disconnect logic here later if needed.
    Destroy();
    event.Skip();
}

void MultiplayerFrame::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    // When the timer fires, we just repaint from the latest cached update.
    const PlayerStateDTO* local = FindLocalPlayerState();
    if (!local) {
        return;
    }

    // Update board
    m_boardPanel->SetBoard(local->board);

    // Update HUD text
    m_scoreText->SetLabel(
        wxString::Format(wxASCII_STR("Score: %d"), local->score)
    );
    m_levelText->SetLabel(
        wxString::Format(wxASCII_STR("Level: %d"), local->level)
    );

    const char* statusStr = local->isAlive ? "Playing" : "Game Over";
    m_statusText->SetLabel(wxASCII_STR(statusStr));
}

void MultiplayerFrame::OnStateUpdate(const StateUpdate& update)
{
    // Cache the latest update; the timer callback will render it.
    m_lastUpdate = update;

    // Optionally trigger an immediate refresh for snappier UI.
    const PlayerStateDTO* local = FindLocalPlayerState();
    if (local) {
        m_boardPanel->SetBoard(local->board);

        m_scoreText->SetLabel(
            wxString::Format(wxASCII_STR("Score: %d"), local->score)
        );
        m_levelText->SetLabel(
            wxString::Format(wxASCII_STR("Level: %d"), local->level)
        );
        const char* statusStr = local->isAlive ? "Playing" : "Game Over";
        m_statusText->SetLabel(wxASCII_STR(statusStr));
    }
}

const PlayerStateDTO* MultiplayerFrame::FindLocalPlayerState() const
{
    if (!m_client) {
        return nullptr;
    }
    auto idOpt = m_client->playerId();
    if (!idOpt || !m_lastUpdate.has_value()) {
        return nullptr;
    }
    PlayerId myId = *idOpt;

    const auto& update = *m_lastUpdate;
    for (const auto& p : update.players) {
        if (p.id == myId) {
            return &p;
        }
    }
    return nullptr;
}

void MultiplayerFrame::OnMatchResult(const MatchResult& result)
{
    // Only care about our own result.
    if (!m_client) {
        return;
    }
    auto idOpt = m_client->playerId();
    if (!idOpt || *idOpt != result.playerId) {
        return;
    }

    // Stop refreshing; match is over.
    if (m_refreshTimer.IsRunning()) {
        m_refreshTimer.Stop();
    }

    // Update status text and show a message box.
    const char* outcomeStr = nullptr;
    switch (result.outcome) {
    case MatchOutcome::Win:  outcomeStr = "You Win!";  break;
    case MatchOutcome::Lose: outcomeStr = "You Lose."; break;
    case MatchOutcome::Draw: outcomeStr = "Draw.";     break;
    default:                 outcomeStr = "Match finished."; break;
    }

    if (m_statusText) {
        m_statusText->SetLabel(wxString::FromUTF8(outcomeStr));
    }

    wxString msg;
    msg << outcomeStr
        << "\nFinal score: " << result.finalScore;

    wxMessageBox(msg,
                 "Match result",
                 wxOK | wxICON_INFORMATION,
                 this);
}

} // namespace tetris::gui
