#pragma once

#include <cstdint>
#include <optional>

#include "core/GameState.hpp"
#include "network/MessageTypes.hpp" // for GameMode, PlayerId, MatchResult, etc.

namespace tetris::core {

class IMatchRules {
public:
    virtual ~IMatchRules() = default;

    /// Called once when the match starts.
    virtual void onMatchStart(std::uint64_t startTick) = 0;

    /// Called every host tick, with current tick and access to player states.
    /// Return a MatchResult when the match is over; std::nullopt otherwise.
    virtual std::optional<tetris::net::MatchResult>
    update(std::uint64_t currentTick, const std::vector<GameState>& playerStates) = 0;

    /// Returns true when the match is finished (even if result not yet broadcast).
    virtual bool isFinished() const = 0;

    /// Convenience: each ruleset knows its mode (TimeAttack or SharedTurns).
    virtual tetris::net::GameMode mode() const = 0;
};

} // namespace tetris::core
