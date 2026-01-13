#include "network/NetworkHost.hpp"

#include <cassert>

namespace tetris::net {

NetworkHost::NetworkHost(const MultiplayerConfig& config)
    : m_config(config)
{
}

void NetworkHost::addClient(INetworkSessionPtr session)
{
    const PlayerId assigned = m_nextPlayerId++;

    m_players.emplace(assigned, PlayerInfo{assigned, session, "", false});

    session->setMessageHandler(
        [this, assigned](const Message& msg) {
            handleIncoming(assigned, msg);
        }
    );
}

void NetworkHost::poll()
{
    // poll connected sessions
    for (auto& [_, info] : m_players) {
        if (info.session && info.session->isConnected()) {
            info.session->poll();
        }
    }

    // cleanup disconnected clients
    cleanupDisconnected();
}

void NetworkHost::cleanupDisconnected()
{
    for (auto it = m_players.begin(); it != m_players.end(); ) {
        auto& info = it->second;
        if (!info.session || !info.session->isConnected()) {
            m_anyClientDisconnected = true;
            it = m_players.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkHost::handleIncoming(PlayerId pid, const Message& msg)
{
    if (msg.kind == MessageKind::JoinRequest) {
        const auto& req = std::get<JoinRequest>(msg.payload);
        handleJoinRequest(pid, m_players[pid].session, req);
        m_players[pid].name = req.playerName;
    }
    else if (msg.kind == MessageKind::InputActionMessage) {
        m_inputQueue.push_back(std::get<InputActionMessage>(msg.payload));
    }
}

void NetworkHost::handleJoinRequest(PlayerId pid,
                                    INetworkSessionPtr session,
                                    const JoinRequest& req)
{
    // If match already started, interpret JoinRequest as:
    // - initial join OR
    // - "ready for rematch" (post-game)
    auto it = m_players.find(pid);
    if (it != m_players.end()) {
        if (m_matchStarted) {
            it->second.readyForRematch = true;
        }
    }

    // Always reply JoinAccept (harmless even for rematch)
    Message reply;
    reply.kind = MessageKind::JoinAccept;
    reply.payload = JoinAccept{
        pid,
        "Welcome " + req.playerName
    };

    if (session) session->send(reply);
}

std::vector<InputActionMessage> NetworkHost::consumeInputQueue()
{
    auto out = std::move(m_inputQueue);
    m_inputQueue.clear();
    return out;
}

void NetworkHost::startMatch()
{
    // allow calling again for rematch: we simply resend StartGame
    m_matchStarted = true;
    m_startTick = 0;
    sendStartGameMessage();
}

void NetworkHost::sendStartGameMessage()
{
    Message msg;
    msg.kind = MessageKind::StartGame;
    msg.payload = StartGame{
        m_config.mode,
        m_config.timeLimitSeconds,
        m_config.piecesPerTurn,
        m_startTick
    };

    for (auto& [_, info] : m_players) {
        if (info.session && info.session->isConnected()) {
            info.session->send(msg);
        }
    }
}

void NetworkHost::broadcast(const Message& msg)
{
    for (auto& [_, info] : m_players) {
        if (info.session && info.session->isConnected()) {
            info.session->send(msg);
        }
    }
}

void NetworkHost::sendTo(PlayerId playerId, const Message& msg)
{
    auto it = m_players.find(playerId);
    if (it == m_players.end()) return;

    auto& info = it->second;
    if (info.session && info.session->isConnected()) {
        info.session->send(msg);
    }
}

bool NetworkHost::hasAnyConnectedClient() const
{
    for (const auto& [_, info] : m_players) {
        if (info.session && info.session->isConnected()) return true;
    }
    return false;
}

bool NetworkHost::anyClientReadyForRematch() const
{
    for (const auto& [_, info] : m_players) {
        if (info.session && info.session->isConnected() && info.readyForRematch) return true;
    }
    return false;
}

void NetworkHost::clearRematchFlags()
{
    for (auto& [_, info] : m_players) {
        info.readyForRematch = false;
    }
}

} // namespace tetris::net
