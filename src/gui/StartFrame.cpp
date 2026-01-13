#include "gui/StartFrame.hpp"

#include <wx/panel.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/app.h>
#include <wx/textdlg.h>
#include <wx/grid.h>  // For table display

#include "gui/TetrisFrame.hpp"
#include "gui/MultiplayerConfigDialog.hpp"
#include "gui/MultiplayerFrame.hpp"

#include "network/TcpSession.hpp"
#include "network/NetworkClient.hpp"
#include "network/TcpServer.hpp"
#include "network/NetworkHost.hpp"
#include "network/HostGameSession.hpp"
#include "network/HostLoop.hpp"

namespace tetris::ui::gui {

enum {
    ID_SINGLE_PLAYER = wxID_HIGHEST + 100,
    ID_MULTIPLAYER,
    ID_EXIT_BUTTON,
    ID_REFRESH_BUTTON  // Button to refresh client list
};

wxBEGIN_EVENT_TABLE(StartFrame, wxFrame)
    EVT_BUTTON(ID_SINGLE_PLAYER, StartFrame::OnSinglePlayer)
    EVT_BUTTON(ID_MULTIPLAYER,   StartFrame::OnMultiplayer)
    EVT_BUTTON(ID_EXIT_BUTTON,   StartFrame::OnExit)
    EVT_BUTTON(ID_REFRESH_BUTTON, StartFrame::OnRefreshClients)
wxEND_EVENT_TABLE()

StartFrame::StartFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Tetris - Start",
              wxDefaultPosition, wxSize(400, 300)),
      m_server(nullptr),
      m_host(nullptr),
      m_hostPollTimer(this)
{
    Bind(wxEVT_TIMER, &StartFrame::OnHostPoll, this);
    
    auto* panel = new wxPanel(this);
    auto* vbox  = new wxBoxSizer(wxVERTICAL);

    // Add buttons
    auto* btnSingle = new wxButton(panel, ID_SINGLE_PLAYER, "Single Player");
    auto* btnMulti  = new wxButton(panel, ID_MULTIPLAYER,   "Multiplayer");
    auto* btnExit   = new wxButton(panel, ID_EXIT_BUTTON,   "Exit");

    // Status text for showing server status
    m_statusText_ = new wxStaticText(panel, wxID_ANY, "Waiting for players to join...");

    // Set up client list grid (one column: Name)
    m_clientListGrid_ = new wxGrid(panel, wxID_ANY);
    m_clientListGrid_->CreateGrid(0, 1);  // 0 rows initially, 1 column (Name)
    m_clientListGrid_->SetColLabelValue(0, "Player Name");

    // Add UI elements to the sizer
    vbox->AddStretchSpacer(1);
    vbox->Add(m_statusText_, 0, wxALIGN_CENTER | wxALL, 10);
    vbox->Add(m_clientListGrid_, 0, wxALIGN_CENTER | wxALL, 10);
    vbox->Add(btnSingle, 0, wxALIGN_CENTER | wxALL, 10);
    vbox->Add(btnMulti, 0, wxALIGN_CENTER | wxALL, 10);
    vbox->Add(btnExit, 0, wxALIGN_CENTER | wxALL, 10);
    vbox->AddStretchSpacer(1);

    panel->SetSizer(vbox);
    Centre();
}


void StartFrame::OnSinglePlayer(wxCommandEvent&)
{
    auto* gameFrame = new TetrisFrame(nullptr, wxID_ANY, "Tetris - Single Player");
    gameFrame->Show(true);
    gameFrame->Centre();

    wxTheApp->SetTopWindow(gameFrame);
    this->Destroy();
}

void StartFrame::OnMultiplayer(wxCommandEvent&)
{
    MultiplayerConfigDialog dlg(this);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto& cfg = dlg.getConfig();

    if (!cfg.isHost) {
        auto* frame = new MultiplayerFrame(nullptr, cfg);
        frame->Show();
        frame->Centre();

        wxTheApp->SetTopWindow(frame);
        this->Destroy();
        return;
}

    // --- HOST MODE ---

    // 1) Update UI immediately
    m_statusText_->SetLabel("Starting server...");
    Layout();
    Refresh();

    // 2) Create ONE host (same as the test code)
    m_host = std::make_shared<tetris::net::NetworkHost>(cfg);

    m_hostConfig = cfg;

    // 3) Create TCP server
    // Create host ONCE
    m_host = std::make_shared<tetris::net::NetworkHost>(cfg);

    m_server = std::make_shared<tetris::net::TcpServer>(
        cfg.port,
        [this](tetris::net::INetworkSessionPtr session) {
            m_host->addClient(session);

            // Ask GUI thread to refresh later
            CallAfter([this]() {
                updateClientList();
            });
        }
    );

    // 4) Start server
    m_server->start();

    // 5) Update UI to waiting state
    m_statusText_->SetLabel(
        "Server running on port " + std::to_string(cfg.port) +
        "\nWaiting for players to join..."
    );

    // Start polling host (10× per second is plenty)
    m_hostPollTimer.Start(100);
}



// Update the client list grid with the current players
void StartFrame::updateClientList()
{
    if (!m_host) {
        return;
    }

    auto players = m_host->getPlayers();

    int existingRows = m_clientListGrid_->GetNumberRows();
    if (existingRows > 0) {
        m_clientListGrid_->DeleteRows(0, existingRows);
    }

    for (const auto& [playerId, player] : players) {
        int row = m_clientListGrid_->GetNumberRows();
        m_clientListGrid_->AppendRows(1);
        m_clientListGrid_->SetCellValue(row, 0, player.name);
    }
}


void StartFrame::OnExit(wxCommandEvent&)
{
    Close(true);
}

void StartFrame::OnRefreshClients(wxCommandEvent&)
{
    // Refresh the client list in host mode by getting current players from NetworkHost
    if (m_host) {
        updateClientList();
    }
}

void StartFrame::OnHostPoll(wxTimerEvent&)
{
    if (!m_host) {
        return;
    }

    m_host->poll();        // processes JOIN messages
    updateClientList();    // now names exist
}


} // namespace tetris::ui::gui
