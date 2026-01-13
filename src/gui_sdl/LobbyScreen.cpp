#include "gui_sdl/LobbyScreen.hpp"

#include <imgui.h>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "gui_sdl/MultiplayerGameScreen.hpp"

namespace tetris::gui_sdl {

LobbyScreen::LobbyScreen(const tetris::net::MultiplayerConfig& cfg)
    : cfg_(cfg)
{
}

void LobbyScreen::handleEvent(Application&, const SDL_Event&) {
    // UI-only for now.
}

void LobbyScreen::update(Application&, float) {
    // No networking hooked yet, so we keep placeholders.
    // When you wire NetworkHost/NetworkClient later, this screen will:
    // - attempt connect/listen
    // - track connected_ and remoteReady_
}

void LobbyScreen::render(Application& app)
{
    int w=0, h=0;
    app.getWindowSize(w, h);

    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 320), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("Lobby", nullptr, flags);

    ImGui::Text("Role: %s", cfg_.isHost ? "Host" : "Client");
    ImGui::Text("Mode: %s", (cfg_.mode == tetris::net::GameMode::TimeAttack) ? "Time Attack" : "Shared Turns");

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        ImGui::Text("Time limit: %u seconds", cfg_.timeLimitSeconds);
    } else {
        ImGui::Text("Pieces per turn: %u", cfg_.piecesPerTurn);
    }

    ImGui::Separator();

    // Connection status placeholder
    if (cfg_.isHost) {
        ImGui::Text("Listening on port: %u", cfg_.port);
    } else {
        ImGui::Text("Connecting to: %s:%u", cfg_.hostAddress.c_str(), cfg_.port);
    }

    ImGui::Text("Connection: %s", connected_ ? "Connected" : "Not connected (not wired yet)");

    ImGui::Separator();

    ImGui::TextUnformatted("Ready status:");
    ImGui::Checkbox("I am ready", &localReady_);

    // Remote (placeholder)
    ImGui::Checkbox("Opponent ready (placeholder)", &remoteReady_);

    bool canStart = connected_ && localReady_ && remoteReady_ && cfg_.isHost;

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Back to Menu", ImVec2(160, 0))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    ImGui::SameLine();

    if (ImGui::Button("Start (UI Test)", ImVec2(160, 0))) {
        app.setScreen(std::make_unique<MultiplayerGameScreen>(cfg_));
        ImGui::End();
        return;
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!canStart);
    if (ImGui::Button("Start Match", ImVec2(160, 0))) {
        app.setScreen(std::make_unique<MultiplayerGameScreen>(cfg_));
        ImGui::End();
        return;
    }

    ImGui::EndDisabled();

    ImGui::End();
}

} // namespace tetris::gui_sdl
