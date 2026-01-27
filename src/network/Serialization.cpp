#include "network/Serialization.hpp"

#include <sstream>
#include <stdexcept>

namespace tetris::net {
namespace {
    std::string escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == ';' || c == '\\') {
                out.push_back('\\');
            }
            out.push_back(c);
        }
        return out;
    }

    std::string unescape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool esc = false;
        for (char c : s) {
            if (!esc && c == '\\') {
                esc = true;
            } else {
                out.push_back(c);
                esc = false;
            }
        }
        return out;
    }

    // Helper to split a string by a single character delimiter (no escaping inside).
    std::vector<std::string> splitSimple(const std::string& s, char delim) {
        std::vector<std::string> parts;
        std::string current;
        std::istringstream is(s);
        while (std::getline(is, current, delim)) {
            parts.push_back(current);
        }
        return parts;
    }
}

std::string serialize(const Message& msg)
{
    std::ostringstream os;

    switch (msg.kind) {
    case MessageKind::JoinRequest: {
        os << "JOIN_REQUEST;";
        const auto& m = std::get<JoinRequest>(msg.payload);
        os << escape(m.playerName);
        break;
    }
    case MessageKind::JoinAccept: {
        os << "JOIN_ACCEPT;";
        const auto& m = std::get<JoinAccept>(msg.payload);
        os << m.assignedId << ';' << escape(m.welcomeMessage);
        break;
    }
    case MessageKind::StartGame: {
        os << "START_GAME;";
        const auto& m = std::get<StartGame>(msg.payload);
        os << static_cast<int>(m.mode) << ';'
           << m.timeLimitSeconds << ';'
           << m.piecesPerTurn << ';'
           << m.startTick;
        break;
    }
    case MessageKind::InputActionMessage: {
        os << "INPUT;";
        const auto& m = std::get<InputActionMessage>(msg.payload);
        os << m.playerId << ';' << m.clientTick << ';'
           << static_cast<int>(m.action);
        break;
    }
        case MessageKind::StateUpdate: {
            os << "STATE_UPDATE;";
            const auto& m = std::get<StateUpdate>(msg.payload);

            // serverTick;playerCount;timeLeftMs;turnPlayerId;piecesLeftThisTurn
            os << m.serverTick << ';'
            << m.players.size() << ';'
            << m.timeLeftMs << ';'
            << m.turnPlayerId << ';'
            << m.piecesLeftThisTurn;

            for (const auto& p : m.players) {
                os << ';'
                << p.id << ';'
                << escape(p.name) << ';'
                << p.score << ';'
                << p.level << ';'
                << (p.isAlive ? 1 : 0) << ';'
                << p.board.width << ';'
                << p.board.height << ';';

                const auto cellCount = static_cast<std::size_t>(
                    std::max(0, p.board.width) * std::max(0, p.board.height)
                );

                for (std::size_t i = 0; i < cellCount; ++i) {
                    if (i > 0) os << ',';
                    const auto& cell = p.board.cells[i];
                    os << (cell.occupied ? 1 : 0) << ':' << cell.colorIndex;
                }
            }
            break;
        }
    case MessageKind::MatchResult: {
        os << "MATCH_RESULT;";
        const auto& m = std::get<MatchResult>(msg.payload);
        os << m.endTick << ';'
           << m.playerId << ';'
           << static_cast<int>(m.outcome) << ';'
           << m.finalScore;
        break;
    }
        case MessageKind::PlayerLeft: {
        os << "PLAYER_LEFT;";
        const auto& m = std::get<PlayerLeft>(msg.payload);
        os << m.playerId << ';'
           << (m.wasHost ? 1 : 0) << ';'
           << escape(m.reason);
        break;
    }
    case MessageKind::Error: {
        os << "ERROR;";
        const auto& m = std::get<ErrorMessage>(msg.payload);
        os << escape(m.description);
        break;
    }
    case MessageKind::RematchDecision: {
        os << "REMATCH_DECISION;";
        const auto& m = std::get<RematchDecision>(msg.payload);
        os << (m.wantsRematch ? 1 : 0);
        break;
    }
    case MessageKind::KeepAlive: {
        os << "KEEPALIVE";
        break;
    }
    }

    return os.str();
}

std::optional<Message> deserialize(const std::string& line)
{
    std::istringstream is(line);
    std::string type;
    if (!std::getline(is, type, ';')) {
        return std::nullopt;
    }

    Message msg{};

    auto read_rest_of_line = [&]() {
        std::string rest, tmp;
        if (std::getline(is, tmp)) {
            rest = tmp;
        }
        return rest;
    };

    if (type == "JOIN_REQUEST") {
        std::string name;
        std::getline(is, name);
        JoinRequest payload{ unescape(name) };
        msg.kind = MessageKind::JoinRequest;
        msg.payload = std::move(payload);
        return msg;
    } else if (type == "JOIN_ACCEPT") {
        std::string idStr, welcome;
        if (!std::getline(is, idStr, ';')) return std::nullopt;
        std::getline(is, welcome);
        JoinAccept payload{
            static_cast<PlayerId>(std::stoul(idStr)),
            unescape(welcome)
        };
        msg.kind = MessageKind::JoinAccept;
        msg.payload = std::move(payload);
        return msg;
    } else if (type == "START_GAME") {
        std::string modeStr, timeStr, piecesStr, tickStr;
        if (!std::getline(is, modeStr, ';')) return std::nullopt;
        if (!std::getline(is, timeStr, ';')) return std::nullopt;
        if (!std::getline(is, piecesStr, ';')) return std::nullopt;
        std::getline(is, tickStr);

        StartGame payload{
            static_cast<GameMode>(std::stoi(modeStr)),
            static_cast<std::uint32_t>(std::stoul(timeStr)),
            static_cast<std::uint32_t>(std::stoul(piecesStr)),
            static_cast<Tick>(std::stoull(tickStr))
        };
        msg.kind = MessageKind::StartGame;
        msg.payload = std::move(payload);
        return msg;
    } else if (type == "INPUT") {
        std::string pidStr, tickStr, actionStr;
        if (!std::getline(is, pidStr, ';')) return std::nullopt;
        if (!std::getline(is, tickStr, ';')) return std::nullopt;
        std::getline(is, actionStr);

        InputActionMessage payload{
            static_cast<PlayerId>(std::stoul(pidStr)),
            static_cast<Tick>(std::stoull(tickStr)),
            static_cast<tetris::controller::InputAction>(std::stoi(actionStr))
        };
        msg.kind = MessageKind::InputActionMessage;
        msg.payload = std::move(payload);
        return msg;
    } else if (type == "MATCH_RESULT") {
        std::string endStr, pidStr, outcomeStr, scoreStr;
        if (!std::getline(is, endStr, ';')) return std::nullopt;
        if (!std::getline(is, pidStr, ';')) return std::nullopt;
        if (!std::getline(is, outcomeStr, ';')) return std::nullopt;
        std::getline(is, scoreStr);

        MatchResult payload{
            static_cast<Tick>(std::stoull(endStr)),
            static_cast<PlayerId>(std::stoul(pidStr)),
            static_cast<MatchOutcome>(std::stoi(outcomeStr)),
            std::stoi(scoreStr)
        };
        msg.kind = MessageKind::MatchResult;
        msg.payload = std::move(payload);
        return msg;
    } else if (type == "ERROR") {
        std::string desc;
        std::getline(is, desc);
        ErrorMessage payload{ unescape(desc) };
        msg.kind = MessageKind::Error;
        msg.payload = std::move(payload);
        return msg;
        } else if (type == "STATE_UPDATE") {
            std::string tickStr, countStr, timeLeftStr, turnPidStr, piecesLeftStr;

            if (!std::getline(is, tickStr, ';')) return std::nullopt;
            if (!std::getline(is, countStr, ';')) return std::nullopt;
            if (!std::getline(is, timeLeftStr, ';')) return std::nullopt;
            if (!std::getline(is, turnPidStr, ';')) return std::nullopt;
            if (!std::getline(is, piecesLeftStr, ';')) return std::nullopt;

            StateUpdate update;
            update.serverTick = static_cast<Tick>(std::stoull(tickStr));
            const auto playerCount = static_cast<std::size_t>(std::stoul(countStr));
            update.timeLeftMs = static_cast<std::uint32_t>(std::stoul(timeLeftStr));
            update.turnPlayerId = static_cast<PlayerId>(std::stoul(turnPidStr));
            update.piecesLeftThisTurn = static_cast<std::uint32_t>(std::stoul(piecesLeftStr));

            update.players.reserve(playerCount);

            for (std::size_t i = 0; i < playerCount; ++i) {
                std::string idStr, nameStr, scoreStr, levelStr, aliveStr, wStr, hStr, cellsStr;

                if (!std::getline(is, idStr, ';'))    return std::nullopt;
                if (!std::getline(is, nameStr, ';'))  return std::nullopt;
                if (!std::getline(is, scoreStr, ';')) return std::nullopt;
                if (!std::getline(is, levelStr, ';')) return std::nullopt;
                if (!std::getline(is, aliveStr, ';')) return std::nullopt;
                if (!std::getline(is, wStr, ';'))     return std::nullopt;
                if (!std::getline(is, hStr, ';'))     return std::nullopt;
                if (!std::getline(is, cellsStr, ';')) return std::nullopt;

                PlayerStateDTO dto;
                dto.id      = static_cast<PlayerId>(std::stoul(idStr));
                dto.name    = unescape(nameStr);
                dto.score   = std::stoi(scoreStr);
                dto.level   = std::stoi(levelStr);
                dto.isAlive = (std::stoi(aliveStr) != 0);
                dto.board.width  = std::stoi(wStr);
                dto.board.height = std::stoi(hStr);

                const int w = dto.board.width;
                const int h = dto.board.height;
                const std::size_t expectedCells =
                    (w > 0 && h > 0) ? static_cast<std::size_t>(w) * static_cast<std::size_t>(h) : 0;

                dto.board.cells.clear();

                if (expectedCells > 0) {
                    auto tokens = splitSimple(cellsStr, ',');
                    if (tokens.size() != expectedCells) return std::nullopt;

                    dto.board.cells.reserve(expectedCells);
                    for (const auto& token : tokens) {
                        const auto pos = token.find(':');
                        if (pos == std::string::npos) return std::nullopt;

                        const auto occStr   = token.substr(0, pos);
                        const auto colorStr = token.substr(pos + 1);

                        BoardCellDTO cell;
                        cell.occupied  = (std::stoi(occStr) != 0);
                        cell.colorIndex = std::stoi(colorStr);
                        dto.board.cells.push_back(cell);
                    }
                }

                update.players.push_back(std::move(dto));
            }

            msg.kind = MessageKind::StateUpdate;
            msg.payload = std::move(update);
            return msg;
        } else if (type == "PLAYER_LEFT") {
            std::string pidStr, wasHostStr, reasonStr;
            if (!std::getline(is, pidStr, ';')) return std::nullopt;
            if (!std::getline(is, wasHostStr, ';')) return std::nullopt;
            std::getline(is, reasonStr);

            PlayerLeft payload;
            payload.playerId = static_cast<PlayerId>(std::stoul(pidStr));
            payload.wasHost = (std::stoi(wasHostStr) != 0);
            payload.reason = unescape(reasonStr);

            msg.kind = MessageKind::PlayerLeft;
            msg.payload = std::move(payload);
            return msg;
        } else if (type == "REMATCH_DECISION") {
            std::string vStr;
            std::getline(is, vStr);

            RematchDecision payload;
            payload.wantsRematch = (std::stoi(vStr) != 0);

            msg.kind = MessageKind::RematchDecision;
            msg.payload = std::move(payload);
            return msg;
        } else if (type == "KEEPALIVE") {
            msg.kind = MessageKind::KeepAlive;
            msg.payload = KeepAlive{};
            return msg;
        }

    return std::nullopt;
}

} // namespace tetris::net