// tests/network/SerializationTests.cpp

#include <catch2/catch_test_macros.hpp>

#include "network/Serialization.hpp"
#include "network/MessageTypes.hpp"
#include "controller/InputAction.hpp"

using namespace tetris::net;

TEST_CASE("NetworkSerialization: JoinRequest round-trip", "[network][serialization]")
{
    Message original;
    original.kind = MessageKind::JoinRequest;
    original.payload = JoinRequest{ "Player;One\\Weird" };

    const auto line   = serialize(original);
    const auto parsed = deserialize(line);

    REQUIRE(parsed.has_value());
    const auto& msg = *parsed;

    CHECK(msg.kind == MessageKind::JoinRequest);

    const auto* payload = std::get_if<JoinRequest>(&msg.payload);
    REQUIRE(payload != nullptr);
    CHECK(payload->playerName == "Player;One\\Weird");
}

TEST_CASE("NetworkSerialization: InputActionMessage round-trip", "[network][serialization]")
{
    Message original;
    original.kind = MessageKind::InputActionMessage;
    original.payload = InputActionMessage{
        42u,
        12345u,
        tetris::controller::InputAction::RotateCW
    };

    const auto line   = serialize(original);
    const auto parsed = deserialize(line);

    REQUIRE(parsed.has_value());
    const auto& msg = *parsed;

    CHECK(msg.kind == MessageKind::InputActionMessage);

    const auto* payload = std::get_if<InputActionMessage>(&msg.payload);
    REQUIRE(payload != nullptr);

    CHECK(payload->playerId    == 42u);
    CHECK(payload->clientTick  == 12345u);
    CHECK(payload->action      == tetris::controller::InputAction::RotateCW);
}

TEST_CASE("NetworkSerialization: MatchResult round-trip", "[network][serialization]")
{
    Message original;
    original.kind = MessageKind::MatchResult;
    original.payload = MatchResult{
        9999u,          // endTick
        7u,             // playerId
        MatchOutcome::Win,
        123456          // finalScore
    };

    const auto line   = serialize(original);
    const auto parsed = deserialize(line);

    REQUIRE(parsed.has_value());
    const auto& msg = *parsed;

    CHECK(msg.kind == MessageKind::MatchResult);

    const auto* payload = std::get_if<MatchResult>(&msg.payload);
    REQUIRE(payload != nullptr);

    CHECK(payload->endTick    == 9999u);
    CHECK(payload->playerId   == 7u);
    CHECK(payload->outcome    == MatchOutcome::Win);
    CHECK(payload->finalScore == 123456);
}

TEST_CASE("NetworkSerialization: unknown message type fails to parse", "[network][serialization]")
{
    const std::string line = "TOTALLY_UNKNOWN;something;else";

    const auto parsed = deserialize(line);

    CHECK_FALSE(parsed.has_value());
}

TEST_CASE("StateUpdate serialization roundtrip - single player")
{
    // Build a simple 2x2 board with a known pattern
    PlayerStateDTO player;
    player.id       = 1;
    player.name     = "Alice";
    player.score    = 123;
    player.level    = 5;
    player.isAlive  = true;
    player.board.width  = 2;
    player.board.height = 2;
    player.board.cells = {
        BoardCellDTO{true,  1},  // (0,0)
        BoardCellDTO{false, 0},  // (0,1)
        BoardCellDTO{true,  2},  // (1,0)
        BoardCellDTO{false, 0}   // (1,1)
    };

    StateUpdate outgoing;
    outgoing.serverTick = 42;
    outgoing.players.push_back(player);

    Message msg;
    msg.kind    = MessageKind::StateUpdate;
    msg.payload = outgoing;

    // Serialize to text
    const std::string line = serialize(msg);

    // Deserialize back
    auto parsedOpt = deserialize(line);
    REQUIRE(parsedOpt.has_value());

    const Message& parsedMsg = *parsedOpt;
    REQUIRE(parsedMsg.kind == MessageKind::StateUpdate);

    const auto& incoming = std::get<StateUpdate>(parsedMsg.payload);
    REQUIRE(incoming.serverTick == outgoing.serverTick);
    REQUIRE(incoming.players.size() == 1);

    const auto& p = incoming.players[0];
    REQUIRE(p.id      == player.id);
    REQUIRE(p.name    == player.name);
    REQUIRE(p.score   == player.score);
    REQUIRE(p.level   == player.level);
    REQUIRE(p.isAlive == player.isAlive);

    REQUIRE(p.board.width  == player.board.width);
    REQUIRE(p.board.height == player.board.height);
    REQUIRE(p.board.cells.size() == player.board.cells.size());

    for (std::size_t i = 0; i < p.board.cells.size(); ++i) {
        CHECK(p.board.cells[i].occupied   == player.board.cells[i].occupied);
        CHECK(p.board.cells[i].colorIndex == player.board.cells[i].colorIndex);
    }
}

TEST_CASE("StateUpdate serialization roundtrip - multiple players")
{
    PlayerStateDTO p1;
    p1.id       = 1;
    p1.name     = "Alice";
    p1.score    = 1000;
    p1.level    = 3;
    p1.isAlive  = true;
    p1.board.width  = 1;
    p1.board.height = 2;
    p1.board.cells = {
        BoardCellDTO{true,  7},
        BoardCellDTO{false, 0}
    };

    PlayerStateDTO p2;
    p2.id       = 2;
    p2.name     = "Bob";
    p2.score    = 800;
    p2.level    = 2;
    p2.isAlive  = false;
    p2.board.width  = 2;
    p2.board.height = 1;
    p2.board.cells = {
        BoardCellDTO{false, 0},
        BoardCellDTO{true,  4}
    };

    StateUpdate outgoing;
    outgoing.serverTick = 999;
    outgoing.players.push_back(p1);
    outgoing.players.push_back(p2);

    Message msg;
    msg.kind    = MessageKind::StateUpdate;
    msg.payload = outgoing;

    const std::string line = serialize(msg);

    auto parsedOpt = deserialize(line);
    REQUIRE(parsedOpt.has_value());

    const Message& parsedMsg = *parsedOpt;
    REQUIRE(parsedMsg.kind == MessageKind::StateUpdate);

    const auto& incoming = std::get<StateUpdate>(parsedMsg.payload);
    REQUIRE(incoming.serverTick == outgoing.serverTick);
    REQUIRE(incoming.players.size() == 2);

    const auto& q1 = incoming.players[0];
    const auto& q2 = incoming.players[1];

    // Player 1 checks
    CHECK(q1.id      == p1.id);
    CHECK(q1.name    == p1.name);
    CHECK(q1.score   == p1.score);
    CHECK(q1.level   == p1.level);
    CHECK(q1.isAlive == p1.isAlive);
    CHECK(q1.board.width  == p1.board.width);
    CHECK(q1.board.height == p1.board.height);
    REQUIRE(q1.board.cells.size() == p1.board.cells.size());
    for (std::size_t i = 0; i < q1.board.cells.size(); ++i) {
        CHECK(q1.board.cells[i].occupied   == p1.board.cells[i].occupied);
        CHECK(q1.board.cells[i].colorIndex == p1.board.cells[i].colorIndex);
    }

    // Player 2 checks
    CHECK(q2.id      == p2.id);
    CHECK(q2.name    == p2.name);
    CHECK(q2.score   == p2.score);
    CHECK(q2.level   == p2.level);
    CHECK(q2.isAlive == p2.isAlive);
    CHECK(q2.board.width  == p2.board.width);
    CHECK(q2.board.height == p2.board.height);
    REQUIRE(q2.board.cells.size() == p2.board.cells.size());
    for (std::size_t i = 0; i < q2.board.cells.size(); ++i) {
        CHECK(q2.board.cells[i].occupied   == p2.board.cells[i].occupied);
        CHECK(q2.board.cells[i].colorIndex == p2.board.cells[i].colorIndex);
    }
}
