#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>

#include "network/INetworkSession.hpp"
#include "network/MultiplayerConfig.hpp"
#include "network/MessageTypes.hpp"

namespace tetris::net {

class NetworkHost {
public:
    explicit NetworkHost(const MultiplayerConfig& config);

    void addClient(INetworkSessionPtr session);

    /// Poll sessions; detects disconnects and broadcasts PlayerLeft.
    void poll();

    std::vector<InputActionMessage> consumeInputQueue();

    std::size_t playerCount() const { return m_players.size(); }

    /// Rematch: true if at least one connected client is ready (re-sent JoinRequest)
    bool anyClientReadyForRematch() const;

    /// Rematch: clear readiness flags (called after host restarts)
    void clearRematchFlags();

    bool isMatchStarted() const { return m_matchStarted; }

    void startMatch();

    void broadcast(const Message& msg);
    void sendTo(PlayerId playerId, const Message& msg);

    // ✅ helpers para UI / lógica
    bool hasAnyConnectedClient() const;
    std::size_t connectedClientCount() const;

    // opcional: para o MultiplayerGameScreen saber se houve saída
    bool consumeAnyClientDisconnected();

    static constexpr PlayerId HostPlayerId = 1;

private:
    struct PlayerInfo {
        PlayerId id{};
        INetworkSessionPtr session;
        std::string name;
        bool connected{true};
    };

    MultiplayerConfig m_config;
    std::unordered_map<PlayerId, PlayerInfo> m_players;
    std::vector<InputActionMessage> m_inputQueue;

    bool m_matchStarted{false};
    Tick m_startTick{0};

    PlayerId m_nextPlayerId{2}; // começa em 2 para reservar 1 ao host

    bool m_anyClientDisconnected{false};

    void handleIncoming(PlayerId pid, const Message& msg);
    void handleJoinRequest(PlayerId assigned, INetworkSessionPtr session, const JoinRequest& req);
    void sendStartGameMessage();
    void onClientDisconnected(PlayerId pid, const char* reason);

    std::unordered_set<PlayerId> m_rematchReady;
};

} // namespace tetris::net
