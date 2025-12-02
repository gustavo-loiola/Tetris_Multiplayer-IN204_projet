// tests/network/SerializationTests.cpp

#include <catch2/catch_test_macros.hpp>

#include "network/Serialization.hpp"
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
