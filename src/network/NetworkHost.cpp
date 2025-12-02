#include "network/NetworkHost.hpp"


#include <cassert>

namespace tetris::net {

NetworkHost::NetworkHost(const MultiplayerConfig& config)
    : m_config(config)
{
    // if (config.mode == GameMode::TimeAttack) {
    //     // Convert seconds -> ticks: host defines tick duration later.
    //     // For now: 1 sec = 1000 ticks → can adjust when timer integrated.
    //     m_rules = std::make_unique<tetris::core::TimeAttackRules>(
    //         config.timeLimitSeconds * 1000
    //     );
    // }
    // No rules here anymore; HostGameSession owns IMatchRules.
}

void NetworkHost::addClient(INetworkSessionPtr session)
{
    const PlayerId assigned = m_nextPlayerId++;

    // A new player must send JoinRequest first,
    // but we associate a pid immediately to track messages safely.
    m_players.emplace(assigned, PlayerInfo{assigned, session, ""});

    // Register message handler
    session->setMessageHandler(
        [this, assigned](const Message& msg) {
            handleIncoming(assigned, msg);
        }
    );
}

void NetworkHost::poll()
{
    for (auto& [_, info] : m_players) {
        if (info.session->isConnected()) {
            info.session->poll();
        }
    }
}

void NetworkHost::handleIncoming(PlayerId pid, const Message& msg)
{
    if (msg.kind == MessageKind::JoinRequest) {
        const auto& req = std::get<JoinRequest>(msg.payload);
        handleJoinRequest(m_players[pid].session, req);
        m_players[pid].name = req.playerName;
    }
    else if (msg.kind == MessageKind::InputActionMessage) {
        m_inputQueue.push_back(std::get<InputActionMessage>(msg.payload));
    }
}

void NetworkHost::handleJoinRequest(INetworkSessionPtr session,
                                    const JoinRequest& req)
{
    PlayerId assigned = 0;
    for (const auto& p : m_players) {
        if (p.second.session == session) {
            assigned = p.first;
            break;
        }
    }
    assert(assigned != 0);

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

    m_startTick = 0; // TODO integrate with real game clock
    //m_rules->onMatchStart(m_startTick);

    sendStartGameMessage();
}

void NetworkHost::sendStartGameMessage()
{
    Message msg;
    msg.kind = MessageKind::StartGame;
    msg.payload = StartGame{
        //m_rules->mode(),
        m_config.mode,
        m_config.timeLimitSeconds,
        m_config.piecesPerTurn,
        m_startTick
    };

    for (auto& [_, info] : m_players) {
        info.session->send(msg);
    }
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

} // namespace tetris::net
