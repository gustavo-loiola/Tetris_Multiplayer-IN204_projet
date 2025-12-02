#include "network/HostLoop.hpp"

#include <algorithm>
#include <cassert>

#include "network/StateUpdateMapper.hpp"

namespace tetris::net {

HostLoop::HostLoop(HostGameSession& session,
                   const GameStateMap& gameStates,
                   const GameControllerMap& controllers,
                   const PlayerNameMap& playerNames)
    : m_session(session)
    , m_gameStates(gameStates)
    , m_controllers(controllers)
    , m_playerNames(playerNames)
{
    // Basic sanity: every GameState should have a controller (but we don't crash if not).
    for (const auto& [pid, gsPtr] : m_gameStates) {
        (void)gsPtr;
        // If there is no controller for a GameState, we log / assert in debug.
        assert(m_controllers.find(pid) != m_controllers.end()
               && "HostLoop: missing GameController for some GameState");
    }

    // Initialize last-locked-piece counters from current GameStates.
    // This assumes GameState exposes lockedPieces().
    for (const auto& [pid, gsPtr] : m_gameStates) {
        if (gsPtr) {
            m_lastLockedPieces[pid] = gsPtr->lockedPieces();
        }
    }
}

std::vector<MatchResult>
HostLoop::step(Duration elapsed, Tick currentTick)
{
    // 1) Consume input messages coming from NetworkHost via HostGameSession
    auto inputMessages = m_session.consumePendingInputs();

    for (const auto& msg : inputMessages) {
        // For SharedTurns, only the current player is allowed to control
        // the shared board. For other modes, all inputs are accepted.
        if (!m_session.isInputAllowed(msg.playerId)) {
            continue;
        }
        
        auto it = m_controllers.find(msg.playerId);
        if (it == m_controllers.end()) {
            // Unknown player id (might be a late message for a disconnected player).
            // Best practice: ignore safely instead of crashing.
            continue;
        }

        auto* controller = it->second;
        if (controller) {
            controller->handleAction(msg.action);
        }
    }

    // 2) Tick all controllers with elapsed time (gravity, auto-drop, etc.)
    for (auto& [pid, controller] : m_controllers) {
        (void)pid;
        if (controller) {
            controller->update(elapsed);
        }
    }

    // 3) Build PlayerSnapshot list from authoritative GameStates
    std::vector<tetris::core::PlayerSnapshot> snapshots;
    snapshots.reserve(m_gameStates.size());

    for (const auto& [pid, gsPtr] : m_gameStates) {
        if (!gsPtr) {
            continue;
        }

        tetris::core::PlayerSnapshot snap;
        snap.id      = pid;
        snap.score   = static_cast<int>(gsPtr->score());
        snap.isAlive = (gsPtr->status() != tetris::core::GameStatus::GameOver);
        snapshots.push_back(snap);
    }

    // 4) Detect newly locked pieces and notify rules via HostGameSession.
    //    SharedTurnRules uses this to rotate turns; TimeAttackRules ignores it.
    handlePieceLocks(snapshots);

    // 5) Let HostGameSession (rules + network) advance the match
    auto results = m_session.update(currentTick, snapshots);

    // 6) Build and broadcast a StateUpdate so all clients can redraw.
    //    We don't want to send at every physics step; instead, we
    //    accumulate elapsed time and send snapshots at a fixed interval
    //    (~20Hz by default).
    constexpr Duration kBroadcastInterval{50}; // 50ms ≈ 20 updates/sec
    m_stateUpdateAccumulator_ += elapsed;
    if (m_stateUpdateAccumulator_ >= kBroadcastInterval) {
        sendStateUpdate(currentTick);
        // keep the remainder to avoid drift
        m_stateUpdateAccumulator_ -= kBroadcastInterval;
    }

    return results;
}

void HostLoop::handlePieceLocks(const std::vector<tetris::core::PlayerSnapshot>& snapshots)
{
    // For each player/game, compare current lockedPieces() to last known value.
    // If it increased, treat that as "one or more pieces have locked since last step".
    // We call onPieceLocked() once per step per player, even if multiple pieces locked.
    // That’s sufficient for SharedTurns, where rotation is piece-count-based.
    for (const auto& [pid, gsPtr] : m_gameStates) {
        if (!gsPtr) {
            continue;
        }

        const auto currentCount = gsPtr->lockedPieces();
        auto& lastCount = m_lastLockedPieces[pid]; // default-initializes to 0 if missing

        if (currentCount > lastCount) {
            // Notify the rules that a piece has locked for this player.
            // We give the current snapshots so rules can base decisions on up-to-date info.
            m_session.onPieceLocked(pid, snapshots);
        }

        lastCount = currentCount;
    }
}

void HostLoop::sendStateUpdate(Tick currentTick)
{
    // 6) Build and broadcast a StateUpdate so all clients can redraw.
    StateUpdate update;
    update.serverTick = currentTick;

    update.players.reserve(m_gameStates.size());
    for (const auto& [pid, gsPtr] : m_gameStates) {
        if (!gsPtr) continue;

        // Look up player name; fallback to something neutral if not found.
        auto itName = m_playerNames.find(pid);
        const std::string& name = (itName != m_playerNames.end())
                                  ? itName->second
                                  : (static_cast<const std::string&>("")); // "" if unknown

        update.players.push_back(
            StateUpdateMapper::toPlayerDTO(pid, name, *gsPtr)
        );
    }

    m_session.broadcastStateUpdate(update);
}

} // namespace tetris::net
