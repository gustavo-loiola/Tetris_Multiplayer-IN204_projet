#include "gui/MultiplayerConfigDialog.hpp"

#include <wx/sizer.h>
#include <wx/stattext.h>

enum {
    ID_MODE_BOX = wxID_HIGHEST + 1,
    ID_TIME_LIMIT_SPIN,
    ID_PIECES_PER_TURN_SPIN,
    ID_HOST_RADIO,
    ID_JOIN_RADIO,
};

wxBEGIN_EVENT_TABLE(MultiplayerConfigDialog, wxDialog)
    EVT_RADIOBUTTON(ID_HOST_RADIO, MultiplayerConfigDialog::OnHostSelected)
    EVT_RADIOBUTTON(ID_JOIN_RADIO, MultiplayerConfigDialog::OnJoinSelected)
    EVT_RADIOBOX(ID_MODE_BOX, MultiplayerConfigDialog::OnModeChanged)
    EVT_BUTTON(wxID_OK, MultiplayerConfigDialog::OnOk)
wxEND_EVENT_TABLE()

MultiplayerConfigDialog::MultiplayerConfigDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Multiplayer Settings",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // --- Host / Join selection ---
    {
        auto* hostJoinSizer = new wxBoxSizer(wxHORIZONTAL);

        m_radioHost = new wxRadioButton(this, ID_HOST_RADIO, "Host game", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        m_radioJoin = new wxRadioButton(this, ID_JOIN_RADIO, "Join game");

        hostJoinSizer->Add(m_radioHost, 0, wxRIGHT, 10);
        hostJoinSizer->Add(m_radioJoin, 0);

        mainSizer->Add(hostJoinSizer, 0, wxALL, 10);
    }

    // --- Game mode selection ---
    {
        wxString choices[2];
        choices[0] = "Competitive Time Attack";
        choices[1] = "Shared Turns";

        m_modeBox = new wxRadioBox(
            this,
            ID_MODE_BOX,
            "Game mode",
            wxDefaultPosition,
            wxDefaultSize,
            2,
            choices,
            1,
            wxRA_SPECIFY_ROWS
        );

        mainSizer->Add(m_modeBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    }

    // --- Mode-specific settings ---
    {
        auto* grid = new wxFlexGridSizer(2, 2, 5, 5);
        grid->AddGrowableCol(1, 1);

        // Time limit (seconds) for Time Attack
        grid->Add(new wxStaticText(this, wxID_ANY, "Time limit (seconds):"),
                  0, wxALIGN_CENTER_VERTICAL);
        m_timeLimitSpin = new wxSpinCtrl(
            this, ID_TIME_LIMIT_SPIN,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxSP_ARROW_KEYS,
            0, 3600, 180
        );
        grid->Add(m_timeLimitSpin, 1, wxEXPAND);

        // Pieces per turn for Shared Turns
        grid->Add(new wxStaticText(this, wxID_ANY, "Pieces per turn:"),
                  0, wxALIGN_CENTER_VERTICAL);
        m_piecesPerTurnSpin = new wxSpinCtrl(
            this, ID_PIECES_PER_TURN_SPIN,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxSP_ARROW_KEYS,
            1, 10, 1
        );
        grid->Add(m_piecesPerTurnSpin, 1, wxEXPAND);

        mainSizer->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    }

    // --- Network settings (host + join) ---
    {
        auto* netSizer = new wxFlexGridSizer(2, 2, 5, 5);
        netSizer->AddGrowableCol(1, 1);

        netSizer->Add(new wxStaticText(this, wxID_ANY, "Host address (when joining):"),
                      0, wxALIGN_CENTER_VERTICAL);
        m_hostAddressCtrl = new wxTextCtrl(this, wxID_ANY, "127.0.0.1");
        netSizer->Add(m_hostAddressCtrl, 1, wxEXPAND);

        netSizer->Add(new wxStaticText(this, wxID_ANY, "Port:"),
                      0, wxALIGN_CENTER_VERTICAL);
        m_portSpin = new wxSpinCtrl(
            this, wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxSP_ARROW_KEYS,
            1024, 65535, 5000
        );
        netSizer->Add(m_portSpin, 1, wxEXPAND);

        mainSizer->Add(netSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    }

    // --- Buttons ---
    {
        auto* buttonSizer = new wxStdDialogButtonSizer();
        auto* okButton     = new wxButton(this, wxID_OK);
        auto* cancelButton = new wxButton(this, wxID_CANCEL);

        buttonSizer->AddButton(okButton);
        buttonSizer->AddButton(cancelButton);
        buttonSizer->Realize();

        mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxALL, 10);
    }

    SetSizerAndFit(mainSizer);
    CentreOnParent();

    // Default selections
    m_radioHost->SetValue(true);
    m_radioJoin->SetValue(false);

    m_modeBox->SetSelection(0); // Time Attack
    // Explicitly sync m_config with the default UI state
    m_config.isHost          = true;
    m_config.mode            = tetris::net::GameMode::TimeAttack;
    m_config.timeLimitSeconds = static_cast<std::uint32_t>(m_timeLimitSpin->GetValue());
    m_config.piecesPerTurn    = static_cast<std::uint32_t>(m_piecesPerTurnSpin->GetValue());
    m_config.hostAddress      = m_hostAddressCtrl->GetValue().ToStdString();
    m_config.port             = static_cast<std::uint16_t>(m_portSpin->GetValue());
    
    UpdateModeControls();
    UpdateHostJoinControls();
}

void MultiplayerConfigDialog::OnHostSelected(wxCommandEvent&)
{
    m_config.isHost = true;
    UpdateHostJoinControls();
}

void MultiplayerConfigDialog::OnJoinSelected(wxCommandEvent&)
{
    m_config.isHost = false;
    UpdateHostJoinControls();
}

void MultiplayerConfigDialog::OnModeChanged(wxCommandEvent&)
{
    if (m_modeBox->GetSelection() == 0) {
        m_config.mode = tetris::net::GameMode::TimeAttack;
    } else {
        m_config.mode = tetris::net::GameMode::SharedTurns;
    }
    UpdateModeControls();
}

void MultiplayerConfigDialog::UpdateModeControls()
{
    const bool isTimeAttack = (m_modeBox->GetSelection() == 0);

    m_timeLimitSpin->Enable(isTimeAttack);
    m_piecesPerTurnSpin->Enable(!isTimeAttack);
}

void MultiplayerConfigDialog::UpdateHostJoinControls()
{
    // For now we just visually hint:
    // - Host: address still visible but conceptually ignored.
    // - Join: address used.
    // You could gray-out fields differently if you want.
    if (m_config.isHost) {
        m_hostAddressCtrl->Enable(false);
    } else {
        m_hostAddressCtrl->Enable(true);
    }
}

void MultiplayerConfigDialog::OnOk(wxCommandEvent& evt)
{
    // Collect all values into m_config
    m_config.timeLimitSeconds = static_cast<std::uint32_t>(m_timeLimitSpin->GetValue());
    m_config.piecesPerTurn    = static_cast<std::uint32_t>(m_piecesPerTurnSpin->GetValue());

    m_config.hostAddress = m_hostAddressCtrl->GetValue().ToStdString();
    m_config.port        = static_cast<std::uint16_t>(m_portSpin->GetValue());

    // Let wxWidgets close the dialog with wxID_OK
    evt.Skip();
}
