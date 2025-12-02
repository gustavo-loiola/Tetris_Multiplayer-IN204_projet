#include <catch2/catch_test_macros.hpp>
#include <optional>

#include "network/NetworkClient.hpp"
#include "network/MessageTypes.hpp"
#include "controller/InputAction.hpp"

#include "FakeNetworkSession.hpp"

using tetris::net::NetworkClient;
using tetris::net::Message;
using tetris::net::MessageKind;
using tetris::net::JoinRequest;
using tetris::net::JoinAccept;
using tetris::net::InputActionMessage;
using tetris::net::StateUpdate;
using tetris::net::PlayerStateDTO;
using tetris::net::BoardCellDTO;
using tetris::net::Tick;

// NOTE: FakeNetworkSession is defined in the global namespace (see other tests),
// so we do NOT prefix it with tetris::net here.
using ::FakeNetworkSession;

TEST_CASE("NetworkClient sends JoinRequest on start", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Alice");

    REQUIRE(session->sentMessages.empty());

    client.start();

    REQUIRE(session->sentMessages.size() == 1);
    const auto& msg = session->sentMessages[0];

    CHECK(msg.kind == MessageKind::JoinRequest);

    const auto* payload = std::get_if<JoinRequest>(&msg.payload);
    REQUIRE(payload != nullptr);
    CHECK(payload->playerName == "Alice");
}

TEST_CASE("NetworkClient stores playerId after JoinAccept", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Bob");

    SECTION("Before JoinAccept, isJoined is false")
    {
        CHECK_FALSE(client.isJoined());
        CHECK_FALSE(client.playerId().has_value());
    }

    // Simulate host sending JoinAccept back.
    Message acceptMsg;
    acceptMsg.kind = MessageKind::JoinAccept;
    acceptMsg.payload = JoinAccept{
        42u,
        "Welcome Bob"
    };

    session->injectIncoming(acceptMsg);

    CHECK(client.isJoined());
    REQUIRE(client.playerId().has_value());
    CHECK(client.playerId().value() == 42u);
}

TEST_CASE("NetworkClient sends InputActionMessage after join", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Carol");

    // Fake server join accept.
    Message acceptMsg;
    acceptMsg.kind = MessageKind::JoinAccept;
    acceptMsg.payload = JoinAccept{ 7u, "Welcome Carol" };
    session->injectIncoming(acceptMsg);

    REQUIRE(client.isJoined());
    REQUIRE(session->sentMessages.empty());

    // Now send an input action.
    client.sendInput(tetris::controller::InputAction::RotateCW, /*clientTick=*/123);

    REQUIRE(session->sentMessages.size() == 1);
    const auto& msg = session->sentMessages[0];

    CHECK(msg.kind == MessageKind::InputActionMessage);

    const auto* payload = std::get_if<InputActionMessage>(&msg.payload);
    REQUIRE(payload != nullptr);

    CHECK(payload->playerId   == 7u);
    CHECK(payload->clientTick == 123u);
    CHECK(payload->action     == tetris::controller::InputAction::RotateCW);
}

TEST_CASE("NetworkClient ignores input before join", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Dave");

    client.sendInput(tetris::controller::InputAction::MoveLeft, 10);

    // No JoinAccept has been received, so client should not send anything.
    CHECK(session->sentMessages.empty());
}

TEST_CASE("NetworkClient receives StateUpdate and stores it", "[net][client][stateupdate]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Alice");

    // Build a simple StateUpdate message with one player and a tiny board.
    PlayerStateDTO player;
    player.id      = 1u;
    player.name    = "Alice";
    player.score   = 123;
    player.level   = 4;
    player.isAlive = true;
    player.board.width  = 2;
    player.board.height = 1;
    player.board.cells = {
        BoardCellDTO{true,  7},
        BoardCellDTO{false, 0}
    };

    StateUpdate update;
    update.serverTick = static_cast<Tick>(42);
    update.players.push_back(player);

    Message msg;
    msg.kind    = MessageKind::StateUpdate;
    msg.payload = update;

    // Before receiving anything, lastStateUpdate() should be empty.
    REQUIRE_FALSE(client.lastStateUpdate().has_value());

    // Inject the message as if it came from the network.
    session->injectIncoming(msg);

    // Now lastStateUpdate() should be populated and match what we sent.
    auto lastOpt = client.lastStateUpdate();
    REQUIRE(lastOpt.has_value());

    const auto& last = *lastOpt;
    CHECK(last.serverTick == update.serverTick);
    REQUIRE(last.players.size() == 1);

    const auto& p = last.players[0];
    CHECK(p.id      == player.id);
    CHECK(p.name    == player.name);
    CHECK(p.score   == player.score);
    CHECK(p.level   == player.level);
    CHECK(p.isAlive == player.isAlive);
    CHECK(p.board.width  == player.board.width);
    CHECK(p.board.height == player.board.height);
    REQUIRE(p.board.cells.size() == player.board.cells.size());
    for (std::size_t i = 0; i < p.board.cells.size(); ++i) {
        CHECK(p.board.cells[i].occupied   == player.board.cells[i].occupied);
        CHECK(p.board.cells[i].colorIndex == player.board.cells[i].colorIndex);
    }
}

TEST_CASE("NetworkClient invokes StateUpdate handler callback", "[net][client][stateupdate][callback]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Bob");

    int callbackCount = 0;
    std::optional<StateUpdate> callbackUpdate;

    client.setStateUpdateHandler(
        [&](const StateUpdate& update) {
            ++callbackCount;
            callbackUpdate = update;
        }
    );

    // Build a simple StateUpdate message with two players.
    PlayerStateDTO p1;
    p1.id      = 1u;
    p1.name    = "Bob";
    p1.score   = 500;
    p1.level   = 2;
    p1.isAlive = true;
    p1.board.width  = 1;
    p1.board.height = 1;
    p1.board.cells = { BoardCellDTO{true, 3} };

    PlayerStateDTO p2;
    p2.id      = 2u;
    p2.name    = "Alice";
    p2.score   = 400;
    p2.level   = 1;
    p2.isAlive = false;
    p2.board.width  = 1;
    p2.board.height = 1;
    p2.board.cells = { BoardCellDTO{false, 0} };

    StateUpdate update;
    update.serverTick = static_cast<Tick>(99);
    update.players.push_back(p1);
    update.players.push_back(p2);

    Message msg;
    msg.kind    = MessageKind::StateUpdate;
    msg.payload = update;

    // No callback yet.
    CHECK(callbackCount == 0);
    CHECK_FALSE(callbackUpdate.has_value());

    // Inject message.
    session->injectIncoming(msg);

    // Callback should have been invoked exactly once.
    CHECK(callbackCount == 1);
    REQUIRE(callbackUpdate.has_value());

    const auto& u = *callbackUpdate;
    CHECK(u.serverTick == update.serverTick);
    REQUIRE(u.players.size() == 2);
    CHECK(u.players[0].id == p1.id);
    CHECK(u.players[1].id == p2.id);
}
