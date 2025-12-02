#pragma once

#include <vector>
#include <memory>

#include "network/NetworkHost.hpp"
#include "network/MultiplayerConfig.hpp"
#include "core/MatchRules.hpp"
#include "network/MessageTypes.hpp"

namespace tetris::net {

/// High-level host-side orchestrator:
/// - coordinates NetworkHost and IMatchRules
/// - does NOT depend on UI or concrete GameState
/// External code is responsible for:
///   - creating PlayerSnapshot list from actual GameStates
///   - calling onPieceLocked when a piece locks
class HostGameSession {
public:
    HostGameSession(NetworkHost& host,
                    const MultiplayerConfig& config,
                    std::unique_ptr<tetris::core::IMatchRules> rules);

    /// Initialize players for the rules and start the match (including
    /// notifying all clients via NetworkHost::startMatch()).
    void start(Tick startTick,
               const std::vector<tetris::core::PlayerSnapshot>& initialPlayers);

    /// Inform the rules that the current player's piece has locked.
    /// Used for SharedTurns; TimeAttackRules will ignore it.
    void onPieceLocked(PlayerId currentPlayerId,
                       const std::vector<tetris::core::PlayerSnapshot>& players);

    /// Advance the session logic:
    /// - polls the NetworkHost (so it can process incoming messages)
    /// - runs the rules update
    /// Returns:
    ///   - empty vector if match still running
    ///   - non-empty vector of MatchResult when match ends
    std::vector<MatchResult>
    update(Tick currentTick,
           const std::vector<tetris::core::PlayerSnapshot>& players);

    /// Convenience: delegate to host input queue.
    std::vector<InputActionMessage> consumePendingInputs();

    /// For SharedTurns, only the "current" player is allowed to send
    /// effective inputs. For other modes (e.g. TimeAttack), all players
    /// are allowed to act.
    bool isInputAllowed(PlayerId playerId) const;

    /// helper used by HostLoop to notify all clients about the
    /// current authoritative game state.
    void broadcastStateUpdate(const StateUpdate& update);

    bool isStarted()  const { return m_started; }
    bool isFinished() const { return m_finished; }

private:
    NetworkHost& m_host;
    MultiplayerConfig m_config;
    std::unique_ptr<tetris::core::IMatchRules> m_rules;

    bool m_started{false};
    bool m_finished{false};
    Tick m_startTick{0};
};

} // namespace tetris::net
