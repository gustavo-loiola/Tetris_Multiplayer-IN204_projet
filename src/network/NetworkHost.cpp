#include "network/NetworkHost.hpp"

#include <cassert>
#include <chrono>

namespace tetris::net {

NetworkHost::NetworkHost(const MultiplayerConfig& config)
    : m_config(config)
{
}

void NetworkHost::addClient(INetworkSessionPtr session)
{
    if (!session) return;

    PlayerId assigned{};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assigned = m_nextPlayerId++;
        m_players.emplace(assigned, PlayerInfo{assigned, session, "", true});
    }

    // Important: handler can be invoked on TcpSession background thread.
    session->setMessageHandler(
        [this, assigned](const Message& msg) {
            handleIncoming(assigned, msg);
        }
    );
}

void NetworkHost::poll()
{
    std::vector<std::pair<PlayerId, std::string>> disconnected; // (pid, reason)
    std::vector<INetworkSessionPtr> keepAliveTargets;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& [pid, info] : m_players) {
            const bool nowConnected = (info.session && info.session->isConnected());

            if (info.connected && !nowConnected) {
                info.connected = false;
                m_anyClientDisconnected = true;
                disconnected.emplace_back(pid, "DISCONNECTED");
                continue;
            }

            // TcpSession::poll is no-op, but we keep the call for interface symmetry.
            if (nowConnected && info.session) {
                info.session->poll();
            }
        }

        // KeepAlive once per second
        using clock = std::chrono::steady_clock;
        const auto now = clock::now();
        if (m_lastKeepAlive.time_since_epoch().count() == 0) {
            m_lastKeepAlive = now;
        }
        if (now - m_lastKeepAlive >= std::chrono::seconds(1)) {
            for (auto& [pid2, info2] : m_players) {
                (void)pid2;
                if (info2.session && info2.session->isConnected()) {
                    keepAliveTargets.push_back(info2.session);
                }
            }
            m_lastKeepAlive = now;
        }
    }

    // Send disconnect notifications *after* releasing the lock.
    for (const auto& [pid, reason] : disconnected) {
        onClientDisconnected(pid, reason.c_str());
    }

    // Send KeepAlive *after* releasing the lock.
    if (!keepAliveTargets.empty()) {
        Message ka;
        ka.kind = MessageKind::KeepAlive;
        ka.payload = KeepAlive{};
        for (auto& s : keepAliveTargets) {
            if (s && s->isConnected()) {
                s->send(ka);
            }
        }
    }
}

bool NetworkHost::hasAnyConnectedClient() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [pid, info] : m_players) {
        (void)pid;
        if (info.connected) return true;
    }
    return false;
}

std::size_t NetworkHost::connectedClientCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t n = 0;
    for (const auto& [pid, info] : m_players) {
        (void)pid;
        if (info.connected) ++n;
    }
    return n;
}

bool NetworkHost::consumeAnyClientDisconnected()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool v = m_anyClientDisconnected;
    m_anyClientDisconnected = false;
    return v;
}

void NetworkHost::handleIncoming(PlayerId pid, const Message& msg)
{
    // Do the smallest possible work under lock; avoid sending while locked.
    INetworkSessionPtr sessionToReply;
    Message reply{};
    bool shouldReply = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (msg.kind == MessageKind::JoinRequest) {
            const auto& req = std::get<JoinRequest>(msg.payload);

            auto it = m_players.find(pid);
            if (it == m_players.end() || !it->second.session) {
                return;
            }

            // store name
            it->second.name = req.playerName;

            // prepare JoinAccept reply
            reply.kind = MessageKind::JoinAccept;
            reply.payload = JoinAccept{ pid, "Welcome " + req.playerName };

            sessionToReply = it->second.session;
            shouldReply = true;
        }
        else if (msg.kind == MessageKind::InputActionMessage) {
            m_inputQueue.push_back(std::get<InputActionMessage>(msg.payload));
        }
        else if (msg.kind == MessageKind::RematchDecision) {
            const auto& rd = std::get<RematchDecision>(msg.payload);
            if (rd.wantsRematch) {
                m_rematchDeclined.erase(pid);
                m_rematchReady.insert(pid);
            } else {
                m_rematchReady.erase(pid);
                m_rematchDeclined.insert(pid);
            }
        }
    }

    // Send JoinAccept outside the lock (prevents lock+I/O stalls and re-entrancy issues).
    if (shouldReply && sessionToReply && sessionToReply->isConnected()) {
        sessionToReply->send(reply);
    }
}

std::vector<InputActionMessage> NetworkHost::consumeInputQueue()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto out = std::move(m_inputQueue);
    m_inputQueue.clear();
    return out;
}

void NetworkHost::startMatch()
{
    Message msg;
    std::vector<INetworkSessionPtr> targets;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_matchStarted) return;
        m_matchStarted = true;

        m_startTick = 0;

        msg.kind = MessageKind::StartGame;
        msg.payload = StartGame{
            m_config.mode,
            m_config.timeLimitSeconds,
            m_config.piecesPerTurn,
            m_startTick
        };

        for (auto& [pid, info] : m_players) {
            (void)pid;
            if (info.session && info.session->isConnected()) {
                targets.push_back(info.session);
            }
        }
    }

    for (auto& s : targets) {
        if (s && s->isConnected()) {
            s->send(msg);
        }
    }
}

void NetworkHost::broadcast(const Message& msg)
{
    std::vector<INetworkSessionPtr> targets;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [pid, info] : m_players) {
            (void)pid;
            if (info.session && info.session->isConnected()) {
                targets.push_back(info.session);
            }
        }
    }

    for (auto& s : targets) {
        if (s && s->isConnected()) {
            s->send(msg);
        }
    }
}

void NetworkHost::sendTo(PlayerId playerId, const Message& msg)
{
    INetworkSessionPtr target;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_players.find(playerId);
        if (it == m_players.end()) return;
        if (it->second.session && it->second.session->isConnected()) {
            target = it->second.session;
        }
    }

    if (target && target->isConnected()) {
        target->send(msg);
    }
}

void NetworkHost::onClientDisconnected(PlayerId pid, const char* reason)
{
    Message msg;
    std::vector<INetworkSessionPtr> targets;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_rematchReady.erase(pid);
        m_rematchDeclined.erase(pid);

        msg.kind = MessageKind::PlayerLeft;
        PlayerLeft pl;
        pl.playerId = pid;
        pl.wasHost = false;
        pl.reason = reason ? reason : "DISCONNECTED";
        msg.payload = std::move(pl);

        for (auto& [pid2, info2] : m_players) {
            (void)pid2;
            if (info2.session && info2.session->isConnected()) {
                targets.push_back(info2.session);
            }
        }
    }

    for (auto& s : targets) {
        if (s && s->isConnected()) {
            s->send(msg);
        }
    }
}

bool NetworkHost::allConnectedClientsReadyForRematch() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    bool anyClient = false;
    for (const auto& [pid, info] : m_players) {
        if (!info.connected) continue;
        if (pid == HostPlayerId) continue;
        anyClient = true;

        if (m_rematchDeclined.find(pid) != m_rematchDeclined.end()) {
            return false;
        }
        if (m_rematchReady.find(pid) == m_rematchReady.end()) {
            return false;
        }
    }
    return anyClient;
}

bool NetworkHost::anyClientDeclinedRematch() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [pid, info] : m_players) {
        if (!info.connected) continue;
        if (pid == HostPlayerId) continue;
        if (m_rematchDeclined.find(pid) != m_rematchDeclined.end()) {
            return true;
        }
    }
    return false;
}

void NetworkHost::clearRematchFlags()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rematchReady.clear();
    m_rematchDeclined.clear();
}

void NetworkHost::onMatchFinished()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_matchStarted = false;
    m_rematchReady.clear();
    m_rematchDeclined.clear();
}

} // namespace tetris::net
