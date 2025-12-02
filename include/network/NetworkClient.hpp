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
class NetworkClient {
public:
    using StateUpdateHandler = std::function<void(const StateUpdate&)>;

    NetworkClient(INetworkSessionPtr session, std::string playerName);

    /// Send a JoinRequest to the host.
    void start();

    /// Send an input action to the host (only valid after join accepted).
    void sendInput(tetris::controller::InputAction action, Tick clientTick);

    /// For tests or UI to check if the client successfully joined.
    bool isJoined() const { return m_playerId.has_value(); }
    std::optional<PlayerId> playerId() const { return m_playerId; }

    /// Register a callback to be notified whenever a new StateUpdate arrives.
    /// UI code (e.g., MultiplayerFrame) can use this to redraw.
    void setStateUpdateHandler(StateUpdateHandler handler) {
        m_stateUpdateHandler = std::move(handler);
    }

    /// Optional: latest StateUpdate received from host.
    std::optional<StateUpdate> lastStateUpdate() const { return m_lastStateUpdate; }

private:
    void handleMessage(const Message& msg);

    INetworkSessionPtr m_session;
    std::string m_playerName;
    std::optional<PlayerId> m_playerId;

    StateUpdateHandler m_stateUpdateHandler;
    std::optional<StateUpdate> m_lastStateUpdate;
};

} // namespace tetris::net
