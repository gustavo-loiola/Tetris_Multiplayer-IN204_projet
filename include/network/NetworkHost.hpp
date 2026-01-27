#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>
#include <mutex>
#include <chrono>

#include "network/INetworkSession.hpp"
#include "network/MultiplayerConfig.hpp"
#include "network/MessageTypes.hpp"

namespace tetris::net {

class NetworkHost {
public:
    explicit NetworkHost(const MultiplayerConfig& config);

    struct LobbyPlayer {
        PlayerId id;
        std::string name;
        bool connected;
    };

    void addClient(INetworkSessionPtr session);

    /// Poll sessions; detects disconnects and broadcasts PlayerLeft.
    void poll();

    std::vector<InputActionMessage> consumeInputQueue();

    std::size_t playerCount() const { return m_players.size(); }
    std::vector<LobbyPlayer> getLobbyPlayers() const;

    // Rematch handshake:
    // - each client can send RematchDecision{true/false}
    // - host should only restart if ALL connected clients want rematch
    bool allConnectedClientsReadyForRematch() const;
    bool anyClientDeclinedRematch() const;

    /// Rematch: clear readiness flags (called after match finishes or after restart)
    void clearRematchFlags();

    bool isMatchStarted() const { return m_matchStarted; }

    void startMatch();
    void onMatchFinished(); // resets m_matchStarted so StartGame can be sent again

    void broadcast(const Message& msg);
    void sendTo(PlayerId playerId, const Message& msg);

    // helpers for UI / logic
    bool hasAnyConnectedClient() const;
    std::size_t connectedClientCount() const;

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
    std::unordered_set<PlayerId> m_rematchDeclined;

    // TcpSession invokes message handlers on a background thread.
    // Protect shared state with a single small mutex.
    mutable std::mutex m_mutex;

    // KeepAlive ticker
    std::chrono::steady_clock::time_point m_lastKeepAlive{};
};

} // namespace tetris::net
