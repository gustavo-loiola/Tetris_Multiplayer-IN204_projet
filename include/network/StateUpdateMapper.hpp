#pragma once

#include <string>

#include "network/MessageTypes.hpp"
#include "core/GameState.hpp"

namespace tetris::net {

/// Utility to convert core::GameState into PlayerStateDTO
/// for network StateUpdate messages.
class StateUpdateMapper {
public:
    /// Build a PlayerStateDTO snapshot for a given player.
    /// `playerName` comes from NetworkHost / lobby, not from GameState.
    static PlayerStateDTO toPlayerDTO(PlayerId playerId,
                                      const std::string& playerName,
                                      const tetris::core::GameState& gs);
};

} // namespace tetris::net
