#pragma once

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

#include "network/HostGameSession.hpp"
#include "network/MessageTypes.hpp"
#include "core/GameState.hpp"
#include "core/MatchRules.hpp"
#include "controller/GameController.hpp"

namespace tetris::net {

/// Small helper that glues together:
/// - HostGameSession (rules + networking)
/// - GameControllers (input + gravity)
/// It does NOT own GameState; caller is responsible for their lifetime.
class HostLoop {
public:
    using Duration = tetris::controller::GameController::Duration;

    /// maps from PlayerId -> GameState* / GameController* / playerName.
    using GameStateMap      = std::unordered_map<PlayerId, tetris::core::GameState*>;
    using GameControllerMap = std::unordered_map<PlayerId, tetris::controller::GameController*>;
    using PlayerNameMap     = std::unordered_map<PlayerId, std::string>;

    HostLoop(HostGameSession& session,
             const GameStateMap& gameStates,
             const GameControllerMap& controllers,
             const PlayerNameMap& playerNames);

    /// One step of the host loop:
    /// - consumes input messages and forwards them to controllers
    /// - ticks all controllers with `elapsed` time
    /// - builds PlayerSnapshot list and asks HostGameSession::update
    /// - detects new locked pieces and notifies HostGameSession
    /// - periodically builds and broadcasts StateUpdate to all clients
    ///
    /// Returns:
    ///   - empty vector if match still running
    ///   - non-empty vector of MatchResult when match has finished
    std::vector<MatchResult>
    step(Duration elapsed, Tick currentTick);

private:
    HostGameSession& m_session;
    GameStateMap      m_gameStates;
    GameControllerMap m_controllers;
    PlayerNameMap     m_playerNames;

    // Tracks last known lockedPieces() for each player to detect new locks.
    std::unordered_map<PlayerId, std::uint64_t> m_lastLockedPieces;

    // Accumulator used to send StateUpdate at a fixed interval
    // (e.g. ~20 Hz) instead of every physics step.
    Duration m_stateUpdateAccumulator_{Duration{0}};

    // 4) Detect newly locked pieces and notify rules via HostGameSession.
    //    SharedTurnRules uses this to rotate turns; TimeAttackRules ignores it.
    void handlePieceLocks(const std::vector<tetris::core::PlayerSnapshot>& snapshots);

    /// Build and broadcast a StateUpdate so all clients can redraw.
    void sendStateUpdate(Tick currentTick);
};

} // namespace tetris::net
