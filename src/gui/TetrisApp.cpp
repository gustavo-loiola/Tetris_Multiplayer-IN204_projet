#include "gui/TetrisApp.hpp"
#include "gui/TetrisFrame.hpp"

// #include <wx/log.h>
// #include <wx/msgdlg.h>

wxIMPLEMENT_APP(tetris::ui::gui::TetrisApp);

namespace tetris::ui::gui {

bool TetrisApp::OnInit()
{
    // Simple visual check to prove OnInit is being called
    // wxMessageBox("TetrisApp::OnInit called", "Debug", wxOK | wxICON_INFORMATION); //temp
    
    if (!wxApp::OnInit()) {
        return false;
    }

    auto* frame = new TetrisFrame(nullptr, wxID_ANY, "Tetris - Single Player");
    frame->Show(true);
    frame->Centre();
    
    return true;
}

} // namespace tetris::ui::gui
