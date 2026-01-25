#include "gui_sdl/LobbyScreen.hpp"

#include <imgui.h>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"
#include "gui_sdl/MultiplayerGameScreen.hpp"
#include "gui_sdl/MultiplayerConfigScreen.hpp"

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

    client_ = std::make_shared<tetris::net::NetworkClient>(clientSession_, cfg_.playerName);

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
    ImGui::SetNextWindowSize(ImVec2(400, 240), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("Lobby", nullptr, flags);

    ImGui::Text("Role: %s", cfg_.isHost ? "Host" : "Client");
    if (cfg_.isHost) {
        ImGui::Text("Mode: %s", (cfg_.mode == tetris::net::GameMode::TimeAttack) ? "Time Attack" : "Shared Turns");
        if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
            ImGui::Text("Time limit: %u seconds", cfg_.timeLimitSeconds);
        } else {
            ImGui::Text("Pieces/turn: %u", cfg_.piecesPerTurn);
        }
    } else {
        ImGui::TextDisabled("Match rules are chosen by the host.");
    }

    ImGui::Separator();

    if (!cfg_.isHost) {
        ImGui::Text("Connecting to: %s:%u", cfg_.hostAddress.c_str(), cfg_.port);
        ImGui::Text("Connection: %s", connected_ ? "Connected" : "Not connected");

        bool joined = (client_ && client_->isJoined());
        ImGui::Text("Join: %s", joined ? "Accepted" : (joinSent_ ? "Sent..." : "Not sent"));

        if (localId_) {
            ImGui::Text("Local PlayerId: %u", static_cast<unsigned>(*localId_));
        }

        bool startGame = (client_ && client_->lastStartGame().has_value());
        ImGui::Text("StartGame: %s", startGame ? "Received" : "Waiting...");

        // --------- Match rules (host authoritative) ---------
        ImGui::SeparatorText("Match rules (host)");

        if (client_) {
            auto sg = client_->lastStartGame();
            if (sg.has_value()) {
                ImGui::Text("Mode: %s",
                    (sg->mode == tetris::net::GameMode::TimeAttack) ? "Time Attack" : "Shared Turns");

                if (sg->mode == tetris::net::GameMode::TimeAttack) {
                    ImGui::Text("Time limit: %u seconds", sg->timeLimitSeconds);
                } else {
                    ImGui::Text("Pieces/turn: %u", sg->piecesPerTurn);
                }
            } else {
                ImGui::TextDisabled("Waiting rules from host (will arrive with StartGame)...");
            }
        } else {
            ImGui::TextDisabled("Client not initialized yet.");
        }
    } else {
        ImGui::Text("Listening on port: %u", cfg_.port);
        ImGui::Text("Server: %s", (server_ && server_->isRunning()) ? "Running" : "Stopped");
        ImGui::Text("Client connected: %s", connected_ ? "Yes" : "No");
        ImGui::Text("Players in lobby: %u", host_ ? static_cast<unsigned>(host_->playerCount()+1) : 0u);
        if (ImGui::BeginTable("PlayersTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            // Header row
            ImGui::TableSetupColumn(
                "ID",
                ImGuiTableColumnFlags_WidthFixed,
                40.0f   // largura em pixels
            );
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();

            // Show host first
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", 1); // host ID
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", cfg_.playerName.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("Host");

            // Then show connected clients
            const auto players = host_->getLobbyPlayers();
            for (const auto& p : players) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", static_cast<unsigned>(p.id));
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", p.name.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", p.connected ? "Connected" : "Disconnected");
            }

            ImGui::EndTable();
        }

    }

    ImGui::Separator();

    // Centering calculations for the bottom buttons

    // Labels depend on role
    const char* backLabel  = "Back";
    const char* startLabel = cfg_.isHost
        ? "Start Match"
        : "Start Match (host only)";

    // Compute button widths from text + padding
    const ImGuiStyle& style = ImGui::GetStyle();

    float backWidth =
        ImGui::CalcTextSize(backLabel).x + style.FramePadding.x * 2.0f;

    float startWidth =
        ImGui::CalcTextSize(startLabel).x + style.FramePadding.x * 2.0f;

    float spacing = style.ItemSpacing.x;

    // Total width of the button row
    float totalWidth = backWidth + spacing + startWidth;

    // Center horizontally
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - totalWidth) * 0.5f);

    // ---- Buttons ----
    if (ImGui::Button(backLabel)) {
        app.setScreen(std::make_unique<MultiplayerConfigScreen>());
        ImGui::End();
        return;
    }

    ImGui::SameLine();

    if (cfg_.isHost) {
        bool canStart = connected_ && host_ && host_->playerCount() >= 1;
        ImGui::BeginDisabled(!canStart);
        if (ImGui::Button(startLabel)) {
            host_->startMatch();
            app.setScreen(std::make_unique<MultiplayerGameScreen>(cfg_, host_, nullptr));
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(true);
        ImGui::Button(startLabel);
        ImGui::EndDisabled();
    }

    ImGui::End();

}

} // namespace tetris::gui_sdl
