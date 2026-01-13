#pragma once

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/timer.h>
#include <memory>
#include "network/TcpServer.hpp"
#include "network/NetworkHost.hpp"


namespace tetris::ui::gui {

class StartFrame : public wxFrame {
public:
    StartFrame(wxWindow* parent);
    wxTimer m_hostPollTimer;
private:
    std::optional<tetris::net::MultiplayerConfig> m_hostConfig;

    // Member variables for server, host, UI components
    std::shared_ptr<tetris::net::TcpServer> m_server;
    std::shared_ptr<tetris::net::NetworkHost> m_host;
    
    wxStaticText* m_statusText_;
    wxGrid* m_clientListGrid_;

    // Event handlers
    void OnSinglePlayer(wxCommandEvent&);
    void OnMultiplayer(wxCommandEvent&);
    void OnExit(wxCommandEvent&);
    void OnRefreshClients(wxCommandEvent&);  // Add the refresh button handler
    
    void updateClientList();  // Method to update the client list in the UI

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::ui::gui
