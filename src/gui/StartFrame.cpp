#include "gui/StartFrame.hpp"

#include <wx/panel.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/app.h>
#include <wx/textdlg.h>

#include "gui/TetrisFrame.hpp"
#include "gui/MultiplayerConfigDialog.hpp"
#include "gui/MultiplayerFrame.hpp"  

#include "network/TcpSession.hpp"   
#include "network/NetworkClient.hpp" 

namespace tetris::ui::gui {

enum {
    ID_SINGLE_PLAYER = wxID_HIGHEST + 100,
    ID_MULTIPLAYER,
    ID_EXIT_BUTTON
};

wxBEGIN_EVENT_TABLE(StartFrame, wxFrame)
    EVT_BUTTON(ID_SINGLE_PLAYER, StartFrame::OnSinglePlayer)
    EVT_BUTTON(ID_MULTIPLAYER,   StartFrame::OnMultiplayer)
    EVT_BUTTON(ID_EXIT_BUTTON,   StartFrame::OnExit)
wxEND_EVENT_TABLE()

StartFrame::StartFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Tetris - Start",
              wxDefaultPosition, wxSize(400, 300))
{
    auto* panel = new wxPanel(this);
    auto* vbox  = new wxBoxSizer(wxVERTICAL);

    auto* btnSingle = new wxButton(panel, ID_SINGLE_PLAYER, "Single Player");
    auto* btnMulti  = new wxButton(panel, ID_MULTIPLAYER,   "Multiplayer");
    auto* btnExit   = new wxButton(panel, ID_EXIT_BUTTON,   "Exit");

    vbox->AddStretchSpacer(1);
    vbox->Add(btnSingle, 0, wxALIGN_CENTER | wxALL, 10);
    vbox->Add(btnMulti,  0, wxALIGN_CENTER | wxALL, 10);
    vbox->Add(btnExit,   0, wxALIGN_CENTER | wxALL, 10);
    vbox->AddStretchSpacer(1);

    panel->SetSizer(vbox);
    Centre();
}

void StartFrame::OnSinglePlayer(wxCommandEvent&)
{
    auto* gameFrame = new TetrisFrame(nullptr, wxID_ANY, "Tetris - Single Player");
    gameFrame->Show(true);
    gameFrame->Centre();

    // Make the game frame the new top window so that when it closes,
    // the application knows it can terminate.
    wxTheApp->SetTopWindow(gameFrame);

    // Destroy the start frame instead of just hiding it,
    // so we don't keep a hidden, zombie window around.
    this->Destroy();
}

void StartFrame::OnMultiplayer(wxCommandEvent&)
{
    MultiplayerConfigDialog dlg(this);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto& cfg = dlg.getConfig();

    if (cfg.isHost) {
        // For now, we only implement the client "join" path in the GUI.
        // Hosting will be wired in a later step (it requires starting
        // TcpServer + NetworkHost + HostGameSession + HostLoop).
        wxMessageBox("Host mode from the GUI is not wired yet.\n\n"
                     "You can start a host process separately, then use this\n"
                     "dialog to join it as a client.",
                     "Host mode not implemented",
                     wxOK | wxICON_INFORMATION,
                     this);
        return;
    }

    // --- Client / Join path ---

    // Ask the user for a player name. You can later move this field into the dialog if you prefer.
    wxString defaultName = "Player";
    wxString playerName = wxGetTextFromUser(
        "Enter your player name:",
        "Player name",
        defaultName,
        this
    );

    if (playerName.IsEmpty()) {
        // User cancelled name entry; abort joining.
        return;
    }

    // Create a TCP client session to the selected host:port.
    auto session = tetris::net::TcpSession::createClient(cfg.hostAddress, cfg.port);
    if (!session || !session->isConnected()) {
        wxMessageBox(
            wxString::Format("Could not connect to host %s:%u",
                             cfg.hostAddress, cfg.port),
            "Connection error",
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    // Create NetworkClient and send JoinRequest.
    auto client = std::make_shared<tetris::net::NetworkClient>(
        session,
        playerName.ToStdString()
    );
    client->start();

    // Open the MultiplayerFrame, which will listen for StateUpdate messages
    // and render the authoritative game state from the host.
    auto* multiFrame = new tetris::gui::MultiplayerFrame(client, nullptr);
    multiFrame->Show(true);
    multiFrame->Centre();

    // Make the multiplayer frame the new top window so that when it closes,
    // the application knows it can terminate.
    wxTheApp->SetTopWindow(multiFrame);

    // Destroy the start frame instead of just hiding it,
    // so we don't keep a hidden, zombie window around.
    this->Destroy();
}

void StartFrame::OnExit(wxCommandEvent&)
{
    Close(true);
}

} // namespace tetris::ui::gui