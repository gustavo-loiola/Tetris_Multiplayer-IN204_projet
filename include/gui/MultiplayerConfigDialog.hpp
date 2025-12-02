#pragma once

#include <wx/dialog.h>
#include <wx/radiobut.h>
#include <wx/radiobox.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>

#include "network/MultiplayerConfig.hpp"

class MultiplayerConfigDialog : public wxDialog {
public:
    explicit MultiplayerConfigDialog(wxWindow* parent);

    /// Returns the configuration selected by the user.
    /// Valid only if the dialog was closed with wxID_OK.
    const tetris::net::MultiplayerConfig& getConfig() const { return m_config; }

private:
    void OnHostSelected(wxCommandEvent& evt);
    void OnJoinSelected(wxCommandEvent& evt);
    void OnModeChanged(wxCommandEvent& evt);
    void OnOk(wxCommandEvent& evt);

    void UpdateModeControls();
    void UpdateHostJoinControls();

    tetris::net::MultiplayerConfig m_config;

    wxRadioButton* m_radioHost{nullptr};
    wxRadioButton* m_radioJoin{nullptr};

    wxRadioBox*   m_modeBox{nullptr};
    wxSpinCtrl*   m_timeLimitSpin{nullptr};
    wxSpinCtrl*   m_piecesPerTurnSpin{nullptr};

    wxTextCtrl*   m_hostAddressCtrl{nullptr};
    wxSpinCtrl*   m_portSpin{nullptr};

    wxDECLARE_EVENT_TABLE();
};
