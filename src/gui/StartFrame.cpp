#include "gui/StartFrame.hpp"

#include <wx/panel.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>

#include "gui/TetrisFrame.hpp"
#include "gui/MultiplayerConfigDialog.hpp"

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
    // Existing single-player frame
    auto* frame = new TetrisFrame(nullptr, wxID_ANY, "Tetris - Single Player");
    frame->Show(true);
    frame->Centre();
    this->Hide();
}

void StartFrame::OnMultiplayer(wxCommandEvent&)
{
    MultiplayerConfigDialog dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        const auto& cfg = dlg.getConfig();

        wxString msg;
        msg << "Mode: "
            << (cfg.mode == tetris::net::GameMode::TimeAttack ? "Time Attack" : "Shared Turns")
            << "\nAs " << (cfg.isHost ? "Host" : "Client")
            << "\nTime limit: " << cfg.timeLimitSeconds
            << " s"
            << "\nPieces per turn: " << cfg.piecesPerTurn
            << "\nHost address: " << cfg.hostAddress
            << "\nPort: " << cfg.port;

        wxMessageBox(msg, "Multiplayer configuration",
                     wxOK | wxICON_INFORMATION, this);

        // Week 3: start host/client and open multiplayer game frame here.
    }
}

void StartFrame::OnExit(wxCommandEvent&)
{
    Close(true);
}

} // namespace tetris::ui::gui