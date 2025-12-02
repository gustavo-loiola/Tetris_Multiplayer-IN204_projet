#include "gui/TetrisApp.hpp"
#include "gui/TetrisFrame.hpp"
#include "gui/StartFrame.hpp"

wxIMPLEMENT_APP(tetris::ui::gui::TetrisApp);

namespace tetris::ui::gui {

bool TetrisApp::OnInit()
{
    if (!wxApp::OnInit()) {
        return false;
    }

    // Show start menu instead of directly opening single-player
    auto* frame = new StartFrame(nullptr);
    frame->Show(true);
    frame->Centre();

    return true;
}

} // namespace tetris::ui::gui
