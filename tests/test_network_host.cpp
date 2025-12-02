#include <catch2/catch_test_macros.hpp>

#include "network/NetworkHost.hpp"
#include "FakeNetworkSession.hpp"
#include "controller/InputAction.hpp"

using namespace tetris::net;

TEST_CASE("Host echoes JoinAccept correctly", "[network][host]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode = GameMode::TimeAttack;

    NetworkHost host(cfg);

    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    // Simulate join request sent from client side:
    Message req;
    req.kind = MessageKind::JoinRequest;
    req.payload = JoinRequest{"Alice"};
    session->injectIncoming(req);

    REQUIRE(session->sentMessages.size() == 1);
    const auto& reply = session->sentMessages[0];
    CHECK(reply.kind == MessageKind::JoinAccept);

    const auto* payload = std::get_if<JoinAccept>(&reply.payload);
    REQUIRE(payload);
    CHECK(payload->assignedId == 1u);
}

TEST_CASE("Host stores incoming input actions", "[network][host]")
{
    MultiplayerConfig cfg;
    cfg.mode = GameMode::TimeAttack;

    NetworkHost host(cfg);
    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    Message req;
    req.kind = MessageKind::JoinRequest;
    req.payload = JoinRequest{"Bob"};
    session->injectIncoming(req);
    session->sentMessages.clear();

    // Simulate input from Bob
    Message inputMsg;
    inputMsg.kind = MessageKind::InputActionMessage;
    inputMsg.payload = InputActionMessage{1u, 5u, tetris::controller::InputAction::SoftDrop};
    session->injectIncoming(inputMsg);

    auto queue = host.consumeInputQueue();
    REQUIRE(queue.size() == 1);
    CHECK(queue[0].playerId == 1u);
    CHECK(queue[0].clientTick == 5u);
    CHECK(queue[0].action == tetris::controller::InputAction::SoftDrop);
}

TEST_CASE("Host sends StartGame to all players", "[network][host]")
{
    MultiplayerConfig cfg;
    cfg.mode = GameMode::TimeAttack;
    cfg.timeLimitSeconds = 123;

    NetworkHost host(cfg);

    auto s1 = std::make_shared<FakeNetworkSession>();
    auto s2 = std::make_shared<FakeNetworkSession>();
    host.addClient(s1);
    host.addClient(s2);

    // Both must send JoinRequest first to be named
    Message req;
    req.kind = MessageKind::JoinRequest;
    req.payload = JoinRequest{"P1"};
    s1->injectIncoming(req);

    req.payload = JoinRequest{"P2"};
    s2->injectIncoming(req);

    s1->sentMessages.clear();
    s2->sentMessages.clear();

    host.startMatch();

    REQUIRE(s1->sentMessages.size() == 1);
    REQUIRE(s2->sentMessages.size() == 1);

    const auto* p1 = std::get_if<StartGame>(&s1->sentMessages[0].payload);
    const auto* p2 = std::get_if<StartGame>(&s2->sentMessages[0].payload);

    REQUIRE(p1);
    REQUIRE(p2);
    CHECK(p1->timeLimitSeconds == 123);
    CHECK(p2->timeLimitSeconds == 123);
}
