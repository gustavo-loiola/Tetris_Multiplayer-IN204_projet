#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

#include "network/INetworkSession.hpp"
#include "network/MultiplayerConfig.hpp"
#include "network/MessageTypes.hpp"

namespace tetris::net {

class NetworkHost {
public:
    NetworkHost(const MultiplayerConfig& config);

    void addClient(INetworkSessionPtr session);

    /// Poll all sessions for incoming messages + cleanup disconnected.
    void poll();

    std::vector<InputActionMessage> consumeInputQueue();

    std::size_t playerCount() const { return m_players.size(); }

    bool isMatchStarted() const { return m_matchStarted; }

    void startMatch();

    void broadcast(const Message& msg);
    void sendTo(PlayerId playerId, const Message& msg);

    // ---- Rematch helpers ----
    bool hasAnyConnectedClient() const;
    bool anyClientReadyForRematch() const;
    void clearRematchFlags();
    bool hasAnyClientDisconnected() const { return m_anyClientDisconnected; }
    void clearDisconnectedFlag() { m_anyClientDisconnected = false; }

private:
    struct PlayerInfo {
        PlayerId id{};
        INetworkSessionPtr session;
        std::string name;
        bool readyForRematch{false};
    };

    MultiplayerConfig m_config;
    std::unordered_map<PlayerId, PlayerInfo> m_players;
    std::vector<InputActionMessage> m_inputQueue;

    bool m_matchStarted{false};
    Tick m_startTick{0};
    PlayerId m_nextPlayerId{1};

    bool m_anyClientDisconnected{false};

    void handleIncoming(PlayerId pid, const Message& msg);
    void handleJoinRequest(PlayerId pid, INetworkSessionPtr session, const JoinRequest& req);
    void sendStartGameMessage();

    void cleanupDisconnected();
};

} // namespace tetris::net
