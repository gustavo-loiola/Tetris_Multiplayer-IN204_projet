#include "gui_sdl/LobbyScreen.hpp"

#include <imgui.h>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "gui_sdl/MultiplayerGameScreen.hpp"

#include "network/TcpServer.hpp"
#include "network/TcpSession.hpp"
#include "network/NetworkHost.hpp"
#include "network/NetworkClient.hpp"

namespace tetris::gui_sdl {

LobbyScreen::~LobbyScreen() = default;

LobbyScreen::LobbyScreen(const tetris::net::MultiplayerConfig& cfg)
    : cfg_(cfg)
{
}

void LobbyScreen::handleEvent(Application&, const SDL_Event&) {
    // ImGui-only
}

void LobbyScreen::ensureHostStarted()
{
    if (host_ && server_) return;

    host_ = std::make_shared<tetris::net::NetworkHost>(cfg_);

    server_ = std::make_unique<tetris::net::TcpServer>(
        cfg_.port,
        [this](tetris::net::INetworkSessionPtr session) {
            host_->addClient(std::move(session));
            connected_ = true; // ok para UI (não é crítico)
        }
    );

    server_->start();
}

void LobbyScreen::ensureClientStarted()
{
    if (client_) return;

    clientSession_ = tetris::net::TcpSession::createClient(cfg_.hostAddress, cfg_.port);
    if (!clientSession_ || !clientSession_->isConnected()) {
        connected_ = false;
        return;
    }

    connected_ = true;

    client_ = std::make_shared<tetris::net::NetworkClient>(clientSession_, std::string(nameBuf_));

    // manda join uma vez
    client_->start();
    joinSent_ = true;
}

void LobbyScreen::update(Application& app, float)
{
    if (cfg_.isHost) {
        ensureHostStarted();
        if (host_) host_->poll();
        return;
    }

    // client
    ensureClientStarted();

    if (client_ && client_->isJoined() && !localId_) {
        localId_ = client_->playerId();
    }

    // ✅ avanço real: quando StartGame chegar, entra na tela multiplayer
    if (client_) {
        auto sg = client_->lastStartGame();
        if (sg.has_value()) {
            // opcional: garantir que cfg_ reflita o que veio do host
            cfg_.mode = sg->mode;
            cfg_.timeLimitSeconds = sg->timeLimitSeconds;
            cfg_.piecesPerTurn = sg->piecesPerTurn;

            app.setScreen(std::make_unique<MultiplayerGameScreen>(cfg_, nullptr, client_));
            return;
        }
    }
}

void LobbyScreen::render(Application& app)
{
    int w=0, h=0;
    app.getWindowSize(w, h);

    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(580, 360), ImGuiCond_Always);

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
        ImGui::Text("Pieces/turn: %u", cfg_.piecesPerTurn);
    }

    ImGui::Separator();

    if (!cfg_.isHost) {
        ImGui::InputText("Name", nameBuf_, IM_ARRAYSIZE(nameBuf_));
        ImGui::Text("Connecting to: %s:%u", cfg_.hostAddress.c_str(), cfg_.port);
        ImGui::Text("Connection: %s", connected_ ? "Connected" : "Not connected");

        bool joined = (client_ && client_->isJoined());
        ImGui::Text("Join: %s", joined ? "Accepted" : (joinSent_ ? "Sent..." : "Not sent"));

        if (localId_) {
            ImGui::Text("Local PlayerId: %u", static_cast<unsigned>(*localId_));
        }

        bool startGame = (client_ && client_->lastStartGame().has_value());
        ImGui::Text("StartGame: %s", startGame ? "Received" : "Waiting...");
    } else {
        ImGui::Text("Listening on port: %u", cfg_.port);
        ImGui::Text("Server: %s", (server_ && server_->isRunning()) ? "Running" : "Stopped");
        ImGui::Text("Client connected: %s", connected_ ? "Yes" : "No");
        ImGui::Text("Players in host: %u", host_ ? static_cast<unsigned>(host_->playerCount()) : 0u);
    }

    ImGui::Separator();

    if (ImGui::Button("Back to Menu", ImVec2(160, 0))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    if (cfg_.isHost) {
        ImGui::SameLine();
        bool canStart = connected_ && host_ && host_->playerCount() >= 1;
        ImGui::BeginDisabled(!canStart);
        if (ImGui::Button("Start Match", ImVec2(160, 0))) {
            host_->startMatch();
            app.setScreen(std::make_unique<MultiplayerGameScreen>(cfg_, host_, nullptr));
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        ImGui::EndDisabled();
    } else {
        ImGui::SameLine();
        ImGui::BeginDisabled(true);
        ImGui::Button("Start Match (host only)", ImVec2(200, 0));
        ImGui::EndDisabled();
    }

    ImGui::End();
}

} // namespace tetris::gui_sdl
