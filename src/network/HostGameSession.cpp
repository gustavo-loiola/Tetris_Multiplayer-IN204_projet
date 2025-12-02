#include "network/HostGameSession.hpp"

#include "core/MatchRules.hpp" // for SharedTurnRules
#include "network/MessageTypes.hpp"

namespace tetris::net {

HostGameSession::HostGameSession(NetworkHost& host,
                                 const MultiplayerConfig& config,
                                 std::unique_ptr<tetris::core::IMatchRules> rules)
    : m_host(host)
    , m_config(config)
    , m_rules(std::move(rules))
{
}

void HostGameSession::start(Tick startTick,
                            const std::vector<tetris::core::PlayerSnapshot>& initialPlayers)
{
    if (m_started) {
        return;
    }

    m_started   = true;
    m_finished  = false;
    m_startTick = startTick;

    if (m_rules) {
        m_rules->initializePlayers(initialPlayers);
        m_rules->onMatchStart(startTick);
    }

    // Let NetworkHost notify clients (StartGame message etc.)
    m_host.startMatch();
}

void HostGameSession::onPieceLocked(PlayerId currentPlayerId,
                                    const std::vector<tetris::core::PlayerSnapshot>& players)
{
    if (!m_started || m_finished || !m_rules) {
        return;
    }

    m_rules->onPieceLocked(currentPlayerId, players);
}

std::vector<MatchResult>
HostGameSession::update(Tick currentTick,
                        const std::vector<tetris::core::PlayerSnapshot>& players)
{
    if (!m_started || !m_rules) {
        return {};
    }

    // Allow NetworkHost to process any pending network events.
    m_host.poll();

    auto results = m_rules->update(currentTick, players);
    if (!results.empty()) {
        m_finished = true;
    }
    return results;
}

std::vector<InputActionMessage> HostGameSession::consumePendingInputs()
{
    return m_host.consumeInputQueue();
}

bool HostGameSession::isInputAllowed(PlayerId playerId) const
{
    // If match hasn't started or is already finished, we ignore inputs.
    if (!m_started || m_finished || !m_rules) {
        return false;
    }

    // For modes other than SharedTurns (e.g., TimeAttack), all players
    // are allowed to act at any time.
    if (m_rules->mode() != GameMode::SharedTurns) {
        return true;
    }

    // For SharedTurns, only the current player should be allowed to
    // control the shared board. SharedTurnRules exposes currentPlayer()
    // as a convenience (not part of the interface), so we downcast
    // carefully and fall back to "allow" if something is unexpected.
    auto* sharedRules = dynamic_cast<tetris::core::SharedTurnRules*>(m_rules.get());
    if (!sharedRules) {
        // Defensive: in case the rules pointer is not actually a SharedTurnRules
        // instance even though mode() == SharedTurns. Rather than breaking input
        // entirely, we allow all players as a fallback.
        return true;
    }

    return sharedRules->currentPlayer() == playerId;
}

void HostGameSession::broadcastStateUpdate(const StateUpdate& update)
{
    Message msg;
    msg.kind    = MessageKind::StateUpdate;
    msg.payload = update;

    m_host.broadcast(msg);
}


} // namespace tetris::net
