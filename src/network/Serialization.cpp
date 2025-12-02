#include "network/Serialization.hpp"

#include <sstream>
#include <stdexcept>

namespace tetris::net {

// Very small helpers; you can extend or make more robust.
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
}

std::string serialize(const Message& msg)
{
    // NOTE: This is intentionally straightforward and explicit
    // so you can easily adapt it as you refine the protocol.
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
           << static_cast<int>(m.action); // assumes InputAction is enum class
        break;
    }
    case MessageKind::StateUpdate: {
        // For Week 2 you can keep this simple:
        // send only minimal fields, or even leave unimplemented.
        // I'll still put a TODO here intentionally.
        os << "STATE_UPDATE;TODO";
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
    case MessageKind::Error: {
        os << "ERROR;";
        const auto& m = std::get<ErrorMessage>(msg.payload);
        os << escape(m.description);
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
        // Same story: can be fleshed out later.
        // For now, treat as unsupported.
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace tetris::net
