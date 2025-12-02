#include "network/NetworkClient.hpp"
#include "network/Serialization.hpp" // maybe, not strictly required

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
    if (!m_session || !m_session->isConnected()) {
        return;
    }
    JoinRequest req{ m_playerName };
    Message msg;
    msg.kind = MessageKind::JoinRequest;
    msg.payload = std::move(req);
    m_session->send(msg);
}

void NetworkClient::sendInput(tetris::controller::InputAction action, Tick clientTick)
{
    if (!m_session || !m_session->isConnected() || !m_playerId) {
        return;
    }

    InputActionMessage payload{
        *m_playerId,
        clientTick,
        action
    };
    Message msg;
    msg.kind = MessageKind::InputActionMessage;
    msg.payload = std::move(payload);
    m_session->send(msg);
}

void NetworkClient::handleMessage(const Message& msg)
{
    switch (msg.kind) {
    case MessageKind::JoinAccept: {
        const auto& m = std::get<JoinAccept>(msg.payload);
        m_playerId = m.assignedId;
        break;
    }
    case MessageKind::StateUpdate: {
        const auto& m = std::get<StateUpdate>(msg.payload);
        m_lastStateUpdate = m;
        if (m_stateUpdateHandler) {
            m_stateUpdateHandler(*m_lastStateUpdate);
        }
        break;
    }
    case MessageKind::MatchResult: {
        const auto& m = std::get<MatchResult>(msg.payload);
        m_lastMatchResult = m;
        if (m_matchResultHandler) {
            m_matchResultHandler(*m_lastMatchResult);
        }
        break;
    }
    default:
        // Other messages (StartGame, Error, etc.) can be handled later
        break;
    }
}


} // namespace tetris::net
