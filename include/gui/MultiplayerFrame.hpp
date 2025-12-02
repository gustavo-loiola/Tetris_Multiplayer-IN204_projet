#pragma once

#include <memory>
#include <optional>

#include <wx/frame.h>
#include <wx/timer.h>
#include <wx/stattext.h>

#include "network/NetworkClient.hpp"
#include "network/MessageTypes.hpp"
#include "gui/RemoteBoardPanel.hpp"

namespace tetris::gui {

/// Frame used on the *client* for multiplayer games.
/// It displays the authoritative board sent by the host via StateUpdate.
class MultiplayerFrame : public wxFrame {
public:
    MultiplayerFrame(std::shared_ptr<tetris::net::NetworkClient> client,
                     wxWindow* parent = nullptr,
                     wxWindowID id = wxID_ANY,
                     const wxString& title = wxASCII_STR("Multiplayer Tetris"),
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxSize(400, 800),
                     long style = wxDEFAULT_FRAME_STYLE);

    ~MultiplayerFrame() override = default;

private:
    std::shared_ptr<tetris::net::NetworkClient> m_client;

    RemoteBoardPanel* m_boardPanel{nullptr};
    wxStaticText*     m_scoreText{nullptr};
    wxStaticText*     m_levelText{nullptr};
    wxStaticText*     m_statusText{nullptr};

    wxTimer m_refreshTimer;

    // Latest StateUpdate received from NetworkClient (cached view).
    std::optional<tetris::net::StateUpdate> m_lastUpdate;

    void OnClose(wxCloseEvent& event);
    void OnTimer(wxTimerEvent& event);

    /// Called by NetworkClient's StateUpdate handler.
    void OnStateUpdate(const tetris::net::StateUpdate& update);

    /// Extract the local player's state from the latest StateUpdate.
    const tetris::net::PlayerStateDTO* FindLocalPlayerState() const;

    /// Called by NetworkClient's MatchResult handler.
    void OnMatchResult(const tetris::net::MatchResult& result);

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::gui
