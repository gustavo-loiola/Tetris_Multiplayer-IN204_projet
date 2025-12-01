#pragma once

#include <wx/frame.h>
#include <wx/timer.h>

#include "core/GameState.hpp"
#include "controller/GameController.hpp"
#include "gui/BoardPanel.hpp"

namespace tetris::ui::gui {

class BoardPanel;

class TetrisFrame : public wxFrame {
public:
    TetrisFrame(wxWindow* parent,
                wxWindowID id,
                const wxString& title);

private:
    void setupLayout();

private:
    tetris::core::GameState game_;
    tetris::controller::GameController controller_;

    BoardPanel* boardPanel_{nullptr};

    wxTimer timer_;
    int timerIntervalMs_{16}; // ~60 FPS

    void setupTimer(); 

    void OnTimer(wxTimerEvent& event); 
    void OnKeyDown(wxKeyEvent& event); 

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::ui::gui
