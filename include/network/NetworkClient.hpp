#pragma once

#include <string>
#include <optional>
#include <functional>

#include "network/INetworkSession.hpp"
#include "controller/InputAction.hpp"

namespace tetris::net {

/// Simple client-side helper that:
/// - Sends JoinRequest when started.
/// - Receives JoinAccept and stores assigned PlayerId.
/// - Sends InputActionMessage to the host.
/// - Receives StateUpdate and exposes it to UI via callback / getter.
/// - Receives MatchResult and exposes it to UI via callback / getter.
class NetworkClient {
public:
    using StateUpdateHandler  = std::function<void(const StateUpdate&)>;
    using MatchResultHandler  = std::function<void(const MatchResult&)>;

    NetworkClient(INetworkSessionPtr session, std::string playerName);

    /// Send a JoinRequest to the host.
    void start();

    /// Send an input action to the host (only valid after join accepted).
    void sendInput(tetris::controller::InputAction action, Tick clientTick);

    /// For tests or UI to check if the client successfully joined.
    bool isJoined() const { return m_playerId.has_value(); }
    std::optional<PlayerId> playerId() const { return m_playerId; }

    /// Register a callback to be notified whenever a new StateUpdate arrives.
    void setStateUpdateHandler(StateUpdateHandler handler) {
        m_stateUpdateHandler = std::move(handler);
    }

    /// Optional: latest StateUpdate received from host.
    std::optional<StateUpdate> lastStateUpdate() const { return m_lastStateUpdate; }

    /// Register a callback to be notified when a MatchResult is received.
    void setMatchResultHandler(MatchResultHandler handler) {
        m_matchResultHandler = std::move(handler);
    }

    /// Optional: latest MatchResult received from host.
    std::optional<MatchResult> lastMatchResult() const { return m_lastMatchResult; }

private:
    void handleMessage(const Message& msg);

    INetworkSessionPtr m_session;
    std::string m_playerName;
    std::optional<PlayerId> m_playerId;

    StateUpdateHandler  m_stateUpdateHandler;
    std::optional<StateUpdate> m_lastStateUpdate;

    MatchResultHandler  m_matchResultHandler;
    std::optional<MatchResult> m_lastMatchResult;
};

} // namespace tetris::net
