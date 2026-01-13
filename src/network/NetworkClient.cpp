#include "network/NetworkClient.hpp"

namespace tetris::net {

NetworkClient::NetworkClient(INetworkSessionPtr session, std::string playerName)
    : m_session(std::move(session))
    , m_playerName(std::move(playerName))
{
    if (m_session) {
        m_session->setMessageHandler(
            [this](const Message& msg) { handleMessage(msg); }
        );
    }
}

void NetworkClient::start()
{
    if (!m_session || !m_session->isConnected()) return;

    JoinRequest req{ m_playerName };
    Message msg;
    msg.kind = MessageKind::JoinRequest;
    msg.payload = std::move(req);
    m_session->send(msg);
}

bool NetworkClient::isJoined() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_playerId.has_value();
}

std::optional<PlayerId> NetworkClient::playerId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_playerId;
}

void NetworkClient::sendInput(tetris::controller::InputAction action, Tick clientTick)
{
    std::optional<PlayerId> pid;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pid = m_playerId;
    }

    if (!m_session || !m_session->isConnected() || !pid) return;

    InputActionMessage payload{ *pid, clientTick, action };
    Message msg;
    msg.kind = MessageKind::InputActionMessage;
    msg.payload = std::move(payload);
    m_session->send(msg);
}

// ------------------ Peek getters ------------------

std::optional<StateUpdate> NetworkClient::lastStateUpdate() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastStateUpdate;
}

std::optional<MatchResult> NetworkClient::lastMatchResult() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastMatchResult;
}

std::optional<StartGame> NetworkClient::lastStartGame() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastStartGame;
}

std::optional<PlayerLeft> NetworkClient::lastPlayerLeft() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastPlayerLeft;
}

std::optional<ErrorMessage> NetworkClient::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

// ------------------ Consume semantics ------------------

std::optional<StateUpdate> NetworkClient::consumeStateUpdate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto out = m_lastStateUpdate;
    m_lastStateUpdate.reset();
    return out;
}

std::optional<MatchResult> NetworkClient::consumeMatchResult()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto out = m_lastMatchResult;
    m_lastMatchResult.reset();
    return out;
}

std::optional<StartGame> NetworkClient::consumeStartGame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto out = m_lastStartGame;
    m_lastStartGame.reset();
    return out;
}

std::optional<PlayerLeft> NetworkClient::consumePlayerLeft()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto out = m_lastPlayerLeft;
    m_lastPlayerLeft.reset();
    return out;
}

std::optional<ErrorMessage> NetworkClient::consumeError()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto out = m_lastError;
    m_lastError.reset();
    return out;
}

// ------------------ Message handling ------------------

void NetworkClient::handleMessage(const Message& msg)
{
    // Copy handlers out under lock, call them outside lock.
    StartGameHandler   startCb;
    StateUpdateHandler stateCb;
    MatchResultHandler resultCb;

    switch (msg.kind) {

    case MessageKind::JoinAccept: {
        const auto& m = std::get<JoinAccept>(msg.payload);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_playerId = m.assignedId;
        break;
    }

    case MessageKind::StartGame: {
        const auto& m = std::get<StartGame>(msg.payload);
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Store StartGame
            m_lastStartGame = m;

            // CRITICAL: clear stale data so client UI doesn't re-trigger old overlay
            m_lastMatchResult.reset();
            m_lastStateUpdate.reset();
            m_lastPlayerLeft.reset();
            m_lastError.reset();

            startCb = m_startGameHandler;
        }
        if (startCb) startCb(m);
        break;
    }

    case MessageKind::StateUpdate: {
        const auto& m = std::get<StateUpdate>(msg.payload);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastStateUpdate = m;
            stateCb = m_stateUpdateHandler;
        }
        if (stateCb) stateCb(m);
        break;
    }

    case MessageKind::MatchResult: {
        const auto& m = std::get<MatchResult>(msg.payload);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastMatchResult = m;
            resultCb = m_matchResultHandler;
        }
        if (resultCb) resultCb(m);
        break;
    }

    case MessageKind::PlayerLeft: {
        const auto& m = std::get<PlayerLeft>(msg.payload);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastPlayerLeft = m;
        break;
    }

    case MessageKind::Error: {
        const auto& m = std::get<ErrorMessage>(msg.payload);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = m;
        break;
    }

    default:
        break;
    }
}

} // namespace tetris::net
