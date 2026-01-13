#include "gui_sdl/MultiplayerConfigScreen.hpp"

#include <cstring>
#include <imgui.h>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "gui_sdl/LobbyScreen.hpp" // added below

namespace tetris::gui_sdl {

MultiplayerConfigScreen::MultiplayerConfigScreen()
{
    // Initialize from defaults
    roleIndex_ = cfg_.isHost ? 0 : 1;
    modeIndex_ = (cfg_.mode == tetris::net::GameMode::TimeAttack) ? 0 : 1;
    port_ = static_cast<int>(cfg_.port);
    timeLimitSec_ = static_cast<int>(cfg_.timeLimitSeconds);
    piecesPerTurn_ = static_cast<int>(cfg_.piecesPerTurn);

    std::strncpy(hostAddressBuf_, cfg_.hostAddress.c_str(), sizeof(hostAddressBuf_) - 1);
    hostAddressBuf_[sizeof(hostAddressBuf_) - 1] = '\0';
}

void MultiplayerConfigScreen::handleEvent(Application&, const SDL_Event&) {
    // No special SDL handling; ImGui handles UI.
}

void MultiplayerConfigScreen::update(Application&, float) {
    // No simulation here.
}

void MultiplayerConfigScreen::render(Application& app)
{
    int w = 0, h = 0;
    app.getWindowSize(w, h);

    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("Multiplayer Configuration", nullptr, flags);

    ImGui::TextUnformatted("Choose host/join and game mode parameters.");
    ImGui::Separator();

    // Host/Join
    ImGui::TextUnformatted("Role:");
    ImGui::RadioButton("Host", &roleIndex_, 0); ImGui::SameLine();
    ImGui::RadioButton("Join", &roleIndex_, 1);

    ImGui::Spacing();

    // Network settings
    ImGui::TextUnformatted("Network:");
    ImGui::InputText("Host address (join only)", hostAddressBuf_, IM_ARRAYSIZE(hostAddressBuf_));
    ImGui::InputInt("Port", &port_);
    if (port_ < 1) port_ = 1;
    if (port_ > 65535) port_ = 65535;

    ImGui::Spacing();

    const bool isJoin = (roleIndex_ == 1);

    // Mode selection
    ImGui::TextUnformatted("Game mode (host decides):");

    ImGui::BeginDisabled(isJoin);
    const char* modes[] = { "Time Attack", "Shared Turns" };
    ImGui::Combo("Mode", &modeIndex_, modes, IM_ARRAYSIZE(modes));

    if (modeIndex_ == 0) {
        ImGui::InputInt("Time limit (seconds)", &timeLimitSec_);
        if (timeLimitSec_ < 0) timeLimitSec_ = 0;
    } else {
        ImGui::InputInt("Pieces per turn", &piecesPerTurn_);
        if (piecesPerTurn_ < 1) piecesPerTurn_ = 1;
    }
    ImGui::EndDisabled();

    if (isJoin) {
        ImGui::TextDisabled("These settings will be provided by the host when the match starts.");
    }

    ImGui::Separator();

    // Buttons
    if (ImGui::Button("Back", ImVec2(120, 0))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    ImGui::SameLine();

    if (ImGui::Button("Continue", ImVec2(160, 0))) {
        // Build config from UI fields
        cfg_.isHost = (roleIndex_ == 0);
        cfg_.hostAddress = hostAddressBuf_;
        cfg_.port = static_cast<std::uint16_t>(port_);

       if (cfg_.isHost) {
            // Host decides rules
            if (modeIndex_ == 0) {
                cfg_.mode = tetris::net::GameMode::TimeAttack;
                cfg_.timeLimitSeconds = static_cast<std::uint32_t>(timeLimitSec_);
                cfg_.piecesPerTurn = 0;
            } else {
                cfg_.mode = tetris::net::GameMode::SharedTurns;
                cfg_.piecesPerTurn = static_cast<std::uint32_t>(piecesPerTurn_);
                cfg_.timeLimitSeconds = 0;
            }
        } else {
            // Joiner: rules come from host's StartGame (Lobby/Game screen)
            // Keep cfg_.mode/timeLimitSeconds/piecesPerTurn as-is (or set to safe defaults)
        }

        app.setScreen(std::make_unique<LobbyScreen>(cfg_));
        ImGui::End();
        return;
    }

    ImGui::End();
}

} // namespace tetris::gui_sdl
