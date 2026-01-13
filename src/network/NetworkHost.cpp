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

    m_players.emplace(assigned, PlayerInfo{assigned, session, "", true});

    session->setMessageHandler(
        [this, assigned](const Message& msg) {
            handleIncoming(assigned, msg);
        }
    );
}

void NetworkHost::poll()
{
    for (auto& [pid, info] : m_players) {
        const bool nowConnected = (info.session && info.session->isConnected());

        // Detect transition connected -> disconnected
        if (info.connected && !nowConnected) {
            info.connected = false;
            m_anyClientDisconnected = true;
            onClientDisconnected(pid, "DISCONNECTED");
            continue;
        }

        if (nowConnected) {
            info.session->poll(); // TcpSession no-op, ok
        }
    }
}

bool NetworkHost::hasAnyConnectedClient() const
{
    for (const auto& [pid, info] : m_players) {
        (void)pid;
        if (info.connected) return true;
    }
    return false;
}

std::size_t NetworkHost::connectedClientCount() const
{
    std::size_t n = 0;
    for (const auto& [pid, info] : m_players) {
        (void)pid;
        if (info.connected) ++n;
    }
    return n;
}

bool NetworkHost::consumeAnyClientDisconnected()
{
    const bool v = m_anyClientDisconnected;
    m_anyClientDisconnected = false;
    return v;
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

void NetworkHost::handleJoinRequest(PlayerId assigned,
                                   INetworkSessionPtr session,
                                   const JoinRequest& req)
{
    assert(session);

    // IMPORTANT: a JoinRequest also acts as "I am ready for rematch"
    // (client re-sends JoinRequest when they click Rematch)
    m_rematchReady.insert(assigned);

    Message reply;
    reply.kind = MessageKind::JoinAccept;
    reply.payload = JoinAccept{
        assigned,
        "Welcome " + req.playerName
    };

    session->send(reply);
}

std::vector<InputActionMessage> NetworkHost::consumeInputQueue()
{
    auto out = std::move(m_inputQueue);
    m_inputQueue.clear();
    return out;
}

void NetworkHost::startMatch()
{
    if (m_matchStarted) return;
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

    broadcast(msg);
}

void NetworkHost::broadcast(const Message& msg)
{
    for (auto& [pid, info] : m_players) {
        (void)pid;
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

void NetworkHost::onClientDisconnected(PlayerId pid, const char* reason)
{
    // Also remove from rematch-ready list
    m_rematchReady.erase(pid);

    Message msg;
    msg.kind = MessageKind::PlayerLeft;

    PlayerLeft pl;
    pl.playerId = pid;
    pl.wasHost = false;
    pl.reason = reason ? reason : "DISCONNECTED";

    msg.payload = std::move(pl);
    broadcast(msg);
}

bool NetworkHost::anyClientReadyForRematch() const
{
    // any connected player (not host id=1) marked ready
    for (const auto& [pid, info] : m_players) {
        if (!info.connected) continue;
        if (pid == 1) continue; // host reserved
        if (m_rematchReady.find(pid) != m_rematchReady.end()) {
            return true;
        }
    }
    return false;
}

void NetworkHost::clearRematchFlags()
{
    m_rematchReady.clear();
}

} // namespace tetris::net
