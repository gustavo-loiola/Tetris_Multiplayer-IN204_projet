// LobbyScreen.hpp
#pragma once

#include "gui_sdl/Screen.hpp"
#include "network/MultiplayerConfig.hpp"

#include <memory>
#include <optional>

namespace tetris::net {
class TcpServer;
class NetworkHost;
class NetworkClient;
class INetworkSession;
using INetworkSessionPtr = std::shared_ptr<INetworkSession>;
}

namespace tetris::gui_sdl {

class LobbyScreen final : public Screen {
public:
    explicit LobbyScreen(const tetris::net::MultiplayerConfig& cfg);
    ~LobbyScreen();

    void handleEvent(Application& app, const SDL_Event& e) override;
    void update(Application& app, float dtSeconds) override;
    void render(Application& app) override;

private:
    void ensureHostStarted();
    void ensureClientStarted();

    tetris::net::MultiplayerConfig cfg_;

    std::unique_ptr<tetris::net::TcpServer> server_;
    std::shared_ptr<tetris::net::NetworkHost> host_;

    tetris::net::INetworkSessionPtr clientSession_;
    std::shared_ptr<tetris::net::NetworkClient> client_;

    bool connected_{false};
    bool joinSent_{false};

    std::optional<tetris::net::PlayerId> localId_;

    char nameBuf_[32] = "Player";
};

} // namespace tetris::gui_sdl
