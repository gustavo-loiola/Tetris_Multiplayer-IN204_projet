#pragma once

#include <string>
#include <cstdint>

#include "network/MessageTypes.hpp" // for tetris::net::GameMode

namespace tetris::net {

struct MultiplayerConfig {
    bool isHost{true};               // true = host, false = join
    GameMode mode{GameMode::TimeAttack};

    std::string playerName;

    std::uint32_t timeLimitSeconds{180};  // used for TimeAttack (0 = no limit)
    std::uint32_t piecesPerTurn{1};       // used for SharedTurns (>=1)

    std::string hostAddress{"127.0.0.1"}; // used when joining
    std::uint16_t port{5000};             // TCP/UDP port, host or join
};

} // namespace tetris::net
