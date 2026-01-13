#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <variant>

#include "controller/InputAction.hpp"   // for tetris::controller::InputAction

namespace tetris::net {

using PlayerId = std::uint32_t;
using Tick     = std::uint64_t;

/// High-level game mode identifiers used by the host & clients.
enum class GameMode {
    TimeAttack,
    SharedTurns
};

/// Message kind tag for on-the-wire messages.
enum class MessageKind : std::uint8_t {
    JoinRequest,
    JoinAccept,
    StartGame,
    InputActionMessage,
    StateUpdate,
    MatchResult,
    Error
};

// ---------- Individual message payloads ----------

struct JoinRequest {
    std::string playerName;
};

struct JoinAccept {
    PlayerId assignedId;
    std::string welcomeMessage; // e.g. "Joined lobby xyz"
};

struct StartGame {
    GameMode mode;
    std::uint32_t timeLimitSeconds;      // For TimeAttack (0 if not used)
    std::uint32_t piecesPerTurn;        // For SharedTurns (0 if not used)
    Tick startTick;                      // Host's tick at which the game starts
};

struct InputActionMessage {
    PlayerId playerId;
    Tick clientTick;                     // client-local tick when action happened
    tetris::controller::InputAction action;
};

/// A minimal DTO for game state snapshot.
/// You will map this from/to your core::GameState / Board.
struct BoardCellDTO {
    bool occupied{};
    int colorIndex{};                    // map from your Tetromino color enum or similar
};

struct BoardDTO {
    int width{};
    int height{};
    std::vector<BoardCellDTO> cells;     // row-major, size = width * height;
};

struct PlayerStateDTO {
    PlayerId id{};
    std::string name;

    BoardDTO board;
    int score{};
    int level{};
    bool isAlive{true};
};

struct StateUpdate {
    Tick serverTick{};
    std::vector<PlayerStateDTO> players;
    // Later you can add flags like "isDelta" vs full snapshot.

    // TimeAttack HUD (0 if not used)
    std::uint32_t timeLeftSeconds{0};
};

enum class MatchOutcome {
    Win,
    Lose,
    Draw
};

struct MatchResult {
    Tick endTick{};
    PlayerId playerId{};
    MatchOutcome outcome{};
    int finalScore{};
};

struct ErrorMessage {
    std::string description;
};

// ---------- Message envelope ----------

using MessagePayload = std::variant<
    JoinRequest,
    JoinAccept,
    StartGame,
    InputActionMessage,
    StateUpdate,
    MatchResult,
    ErrorMessage
>;

struct Message {
    MessageKind kind;
    MessagePayload payload;
};

} // namespace tetris::net
