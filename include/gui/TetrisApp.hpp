#pragma once

#include <wx/app.h>

namespace tetris::ui::gui {

class TetrisApp : public wxApp {
public:
    bool OnInit() override;
};

} // namespace tetris::ui::gui