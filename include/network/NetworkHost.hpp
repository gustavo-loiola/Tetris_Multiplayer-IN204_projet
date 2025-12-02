#pragma once

#include <unordered_map>
#include <vector>
#include <memory>

#include "network/INetworkSession.hpp"
#include "network/MultiplayerConfig.hpp"
#include "network/MessageTypes.hpp"
#include "core/MatchRules.hpp"

namespace tetris::net {

/// Host-authoritative manager for multiplayer sessions.
class NetworkHost {
public:
    NetworkHost(const MultiplayerConfig& config);
    
    /// Add a new client connection via a session.
    void addClient(INetworkSessionPtr session);

    /// Poll all sessions for incoming messages.
    void poll();

    /// Access queued input actions for authoritative controllers.
    std::vector<InputActionMessage> consumeInputQueue();

    /// For UI or tests: how many players joined/accepted.
    std::size_t playerCount() const { return m_players.size(); }

    bool isMatchStarted() const { return m_matchStarted; }

    /// Start the match: sends StartGame to all players.
    void startMatch();

    /// send a message to all connected players.
    void broadcast(const Message& msg);

private:
    struct PlayerInfo {
        PlayerId id;
        INetworkSessionPtr session;
        std::string name;
    };

    MultiplayerConfig m_config;
    std::unordered_map<PlayerId, PlayerInfo> m_players;
    std::vector<InputActionMessage> m_inputQueue;

    //std::unique_ptr<tetris::core::IMatchRules> m_rules;
    bool m_matchStarted{false};
    Tick m_startTick{0};

    PlayerId m_nextPlayerId{1};

    void handleIncoming(PlayerId pid, const Message& msg);
    void handleJoinRequest(INetworkSessionPtr session, const JoinRequest& req);
    void sendStartGameMessage();
};

} // namespace tetris::net
