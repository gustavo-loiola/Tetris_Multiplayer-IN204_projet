#pragma once

#include <string>
#include <optional>
#include <functional>
#include <mutex>

#include "network/INetworkSession.hpp"
#include "network/MessageTypes.hpp"
#include "controller/InputAction.hpp"

namespace tetris::net {

class NetworkClient {
public:
    using StartGameHandler    = std::function<void(const StartGame&)>;
    using StateUpdateHandler  = std::function<void(const StateUpdate&)>;
    using MatchResultHandler  = std::function<void(const MatchResult&)>;

    NetworkClient(INetworkSessionPtr session, std::string playerName);

    void start();
    void sendInput(tetris::controller::InputAction action, Tick clientTick);

    bool isJoined() const;
    std::optional<PlayerId> playerId() const;

    bool isConnected() const {
        return m_session && m_session->isConnected();
    }

    void setStartGameHandler(StartGameHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_startGameHandler = std::move(handler);
    }

    void setStateUpdateHandler(StateUpdateHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stateUpdateHandler = std::move(handler);
    }

    void setMatchResultHandler(MatchResultHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_matchResultHandler = std::move(handler);
    }

    // --- "Peek" semantics (do not clear) ---
    std::optional<StateUpdate>  lastStateUpdate() const;
    std::optional<MatchResult>  lastMatchResult() const;
    std::optional<StartGame>    lastStartGame() const;
    std::optional<PlayerLeft>   lastPlayerLeft() const;
    std::optional<ErrorMessage> lastError() const;

    // --- Consume semantics: returns ONCE then clears it ---
    std::optional<StateUpdate>  consumeStateUpdate();
    std::optional<MatchResult>  consumeMatchResult();
    std::optional<StartGame>    consumeStartGame();
    std::optional<PlayerLeft>   consumePlayerLeft();
    std::optional<ErrorMessage> consumeError();

private:
    void handleMessage(const Message& msg);

    INetworkSessionPtr m_session;
    std::string m_playerName;

    // Everything below can be touched from network thread -> protect with mutex.
    mutable std::mutex m_mutex;

    std::optional<PlayerId> m_playerId;

    StartGameHandler m_startGameHandler;
    std::optional<StartGame> m_lastStartGame;

    StateUpdateHandler m_stateUpdateHandler;
    std::optional<StateUpdate> m_lastStateUpdate;

    MatchResultHandler m_matchResultHandler;
    std::optional<MatchResult> m_lastMatchResult;

    std::optional<PlayerLeft> m_lastPlayerLeft;
    std::optional<ErrorMessage> m_lastError;
};

} // namespace tetris::net
