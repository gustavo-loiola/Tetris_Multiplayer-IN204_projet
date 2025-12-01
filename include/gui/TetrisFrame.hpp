#pragma once

#include <wx/frame.h>
#include <wx/timer.h>
#include <wx/stattext.h>

#include "core/GameState.hpp"
#include "controller/GameController.hpp"

namespace tetris::ui::gui {

class BoardPanel;
class NextPiecePanel;

class TetrisFrame : public wxFrame {
public:
    TetrisFrame(wxWindow* parent,
                wxWindowID id,
                const wxString& title);

private:
    tetris::core::GameState game_;
    tetris::controller::GameController controller_;

    BoardPanel* boardPanel_{nullptr};
    NextPiecePanel* nextPanel_{nullptr};

    wxTimer timer_;
    int timerIntervalMs_{16};

    // banner labels
    wxStaticText* scoreText_{nullptr};
    wxStaticText* levelText_{nullptr};
    wxStaticText* statusText_{nullptr};

    // HUD animation state
    int hudFlashAccumulatorMs_;
    bool hudFlashOn_{true};

    void setupLayout();
    void setupTimer();
    void updateStatusBar(); 

    void OnTimer(wxTimerEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace tetris::ui::gui
