#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <variant>

#include "controller/InputAction.hpp"

namespace tetris::net {

using PlayerId = std::uint32_t;
using Tick     = std::uint64_t;

enum class GameMode {
    TimeAttack,
    SharedTurns
};

enum class MessageKind : std::uint8_t {
    JoinRequest,
    JoinAccept,
    StartGame,
    InputActionMessage,
    StateUpdate,
    MatchResult,
    PlayerLeft,     // NEW
    Error
};

// ---------- Individual message payloads ----------

struct JoinRequest {
    std::string playerName;
};

struct JoinAccept {
    PlayerId assignedId;
    std::string welcomeMessage;
};

struct StartGame {
    GameMode mode;
    std::uint32_t timeLimitSeconds; // TimeAttack
    std::uint32_t piecesPerTurn;    // SharedTurns
    Tick startTick;
};

struct InputActionMessage {
    PlayerId playerId;
    Tick clientTick;
    tetris::controller::InputAction action;
};

struct BoardCellDTO {
    bool occupied{};
    int colorIndex{};
};

struct BoardDTO {
    int width{};
    int height{};
    std::vector<BoardCellDTO> cells;
};

struct PlayerStateDTO {
    PlayerId id{};
    std::string name;

    BoardDTO board;
    int score{};
    int level{};
    bool isAlive{true};
};

// ✅ NEW: enriquecemos StateUpdate para HUD/qualidade de rede
struct StateUpdate {
    Tick serverTick{};
    std::vector<PlayerStateDTO> players;

    // TimeAttack: host manda tempo restante (evita drift no client)
    std::uint32_t timeLeftMs{0}; // 0 se não aplicável

    // SharedTurns: host manda de quem é o turno e quantas peças restam
    PlayerId turnPlayerId{0};          // 0 se não aplicável
    std::uint32_t piecesLeftThisTurn{0};
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

// ✅ NEW: mensagem explícita de saída/desconexão
struct PlayerLeft {
    PlayerId playerId{};
    bool wasHost{false};
    std::string reason; // "DISCONNECTED", "LEFT_TO_MENU", etc.
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
    PlayerLeft,     // NEW
    ErrorMessage
>;

struct Message {
    MessageKind kind;
    MessagePayload payload;
};

} // namespace tetris::net
