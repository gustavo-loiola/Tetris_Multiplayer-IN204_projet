#pragma once

#include <wx/frame.h>

namespace tetris::ui::gui {

class StartFrame : public wxFrame {
public:
    explicit StartFrame(wxWindow* parent);

private:
    void OnSinglePlayer(wxCommandEvent& evt);
    void OnMultiplayer(wxCommandEvent& evt);
    void OnExit(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::ui::gui
