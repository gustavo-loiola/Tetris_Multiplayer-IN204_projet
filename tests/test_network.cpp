#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "network/Serialization.hpp"
#include "network/NetworkClient.hpp"
#include "network/NetworkHost.hpp"
#include "network/HostGameSession.hpp"
#include "network/MessageTypes.hpp"
#include "network/MultiplayerConfig.hpp"

#include "core/MatchRules.hpp"
#include "controller/InputAction.hpp"

#include "FakeNetworkSession.hpp"

using namespace tetris::net;
using ::FakeNetworkSession;

// ----------------------------
// Helpers
// ----------------------------

static StateUpdate makeSmallStateUpdate()
{
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

    StateUpdate up;
    up.serverTick = 42;
    up.players.push_back(player);

    up.timeLeftMs = 1000;
    up.turnPlayerId = 1;
    up.piecesLeftThisTurn = 2;

    return up;
}

static bool hasMatchResultFor(const std::vector<Message>& msgs, PlayerId pid)
{
    for (const auto& m : msgs) {
        if (m.kind != MessageKind::MatchResult) continue;
        const auto* r = std::get_if<MatchResult>(&m.payload);
        if (r && r->playerId == pid) return true;
    }
    return false;
}

static PlayerId extractAssignedIdOrFail(const std::shared_ptr<FakeNetworkSession>& s)
{
    REQUIRE_FALSE(s->sentMessages.empty());
    const auto& last = s->sentMessages.back();
    REQUIRE(last.kind == MessageKind::JoinAccept);
    const auto* ja = std::get_if<JoinAccept>(&last.payload);
    REQUIRE(ja != nullptr);
    return ja->assignedId;
}

// ============================
// Serialization tests
// ============================

TEST_CASE("Network serialization round-trips core message types", "[network][serialization]")
{
    SECTION("JoinRequest")
    {
        Message original;
        original.kind = MessageKind::JoinRequest;
        original.payload = JoinRequest{ "Player;One\\Weird" };

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::JoinRequest);

        const auto* p = std::get_if<JoinRequest>(&parsed->payload);
        REQUIRE(p != nullptr);
        CHECK(p->playerName == "Player;One\\Weird");
    }

    SECTION("JoinAccept")
    {
        Message original;
        original.kind = MessageKind::JoinAccept;
        original.payload = JoinAccept{ 99u, "Welcome;User\\X" };

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::JoinAccept);

        const auto* p = std::get_if<JoinAccept>(&parsed->payload);
        REQUIRE(p != nullptr);
        CHECK(p->assignedId == 99u);
        CHECK(p->welcomeMessage == "Welcome;User\\X");
    }

    SECTION("StartGame")
    {
        Message original;
        original.kind = MessageKind::StartGame;
        original.payload = StartGame{ GameMode::TimeAttack, 123u, 7u, 555u };

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::StartGame);

        const auto* p = std::get_if<StartGame>(&parsed->payload);
        REQUIRE(p != nullptr);
        CHECK(p->mode == GameMode::TimeAttack);
        CHECK(p->timeLimitSeconds == 123u);
        CHECK(p->piecesPerTurn == 7u);
        CHECK(p->startTick == 555u);
    }

    SECTION("InputActionMessage")
    {
        Message original;
        original.kind = MessageKind::InputActionMessage;
        original.payload = InputActionMessage{
            42u, 12345u, tetris::controller::InputAction::RotateCW
        };

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::InputActionMessage);

        const auto* p = std::get_if<InputActionMessage>(&parsed->payload);
        REQUIRE(p != nullptr);
        CHECK(p->playerId == 42u);
        CHECK(p->clientTick == 12345u);
        CHECK(p->action == tetris::controller::InputAction::RotateCW);
    }

    SECTION("MatchResult")
    {
        Message original;
        original.kind = MessageKind::MatchResult;
        original.payload = MatchResult{ 9999u, 7u, MatchOutcome::Win, 123456 };

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::MatchResult);

        const auto* p = std::get_if<MatchResult>(&parsed->payload);
        REQUIRE(p != nullptr);
        CHECK(p->endTick == 9999u);
        CHECK(p->playerId == 7u);
        CHECK(p->outcome == MatchOutcome::Win);
        CHECK(p->finalScore == 123456);
    }

    SECTION("StateUpdate")
    {
        Message original;
        original.kind = MessageKind::StateUpdate;
        original.payload = makeSmallStateUpdate();

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::StateUpdate);

        const auto& incoming = std::get<StateUpdate>(parsed->payload);
        CHECK(incoming.serverTick == 42);
        REQUIRE(incoming.players.size() == 1);
        CHECK(incoming.players[0].name == "Alice");
        CHECK(incoming.timeLeftMs == 1000);
        CHECK(incoming.turnPlayerId == 1);
        CHECK(incoming.piecesLeftThisTurn == 2);
    }

    SECTION("PlayerLeft")
    {
        Message original;
        original.kind = MessageKind::PlayerLeft;
        original.payload = PlayerLeft{ 2u, false, "LEFT_TO_MENU" };

        const auto line = serialize(original);
        const auto parsed = deserialize(line);

        REQUIRE(parsed.has_value());
        CHECK(parsed->kind == MessageKind::PlayerLeft);

        const auto* p = std::get_if<PlayerLeft>(&parsed->payload);
        REQUIRE(p != nullptr);
        CHECK(p->playerId == 2u);
        CHECK(p->wasHost == false);
        CHECK(p->reason == "LEFT_TO_MENU");
    }

    SECTION("Unknown message type fails")
    {
        const auto parsed = deserialize("TOTALLY_UNKNOWN;something;else");
        CHECK_FALSE(parsed.has_value());
    }
}

// ============================
// NetworkClient tests
// ============================

TEST_CASE("NetworkClient sends JoinRequest on start()", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Alice");

    REQUIRE(session->sentMessages.empty());
    client.start();

    REQUIRE(session->sentMessages.size() == 1);
    CHECK(session->sentMessages[0].kind == MessageKind::JoinRequest);

    const auto* p = std::get_if<JoinRequest>(&session->sentMessages[0].payload);
    REQUIRE(p != nullptr);
    CHECK(p->playerName == "Alice");
}

TEST_CASE("NetworkClient stores playerId after JoinAccept", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Bob");

    CHECK_FALSE(client.isJoined());
    CHECK_FALSE(client.playerId().has_value());

    Message accept;
    accept.kind = MessageKind::JoinAccept;
    accept.payload = JoinAccept{ 2u, "Welcome Bob" };

    session->injectIncoming(accept);

    CHECK(client.isJoined());
    REQUIRE(client.playerId().has_value());
    CHECK(*client.playerId() == 2u);
}

TEST_CASE("NetworkClient sends InputActionMessage only after join", "[network][client]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Carol");

    client.sendInput(tetris::controller::InputAction::MoveLeft, 10);
    CHECK(session->sentMessages.empty());

    Message accept;
    accept.kind = MessageKind::JoinAccept;
    accept.payload = JoinAccept{ 2u, "Welcome Carol" };
    session->injectIncoming(accept);

    REQUIRE(client.isJoined());

    client.sendInput(tetris::controller::InputAction::RotateCW, 123);

    REQUIRE(session->sentMessages.size() == 1);
    const auto& msg = session->sentMessages[0];
    CHECK(msg.kind == MessageKind::InputActionMessage);

    const auto* p = std::get_if<InputActionMessage>(&msg.payload);
    REQUIRE(p != nullptr);
    CHECK(p->playerId == 2u);
    CHECK(p->clientTick == 123u);
    CHECK(p->action == tetris::controller::InputAction::RotateCW);
}

TEST_CASE("NetworkClient receives StateUpdate and stores it", "[network][client][stateupdate]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Alice");

    REQUIRE_FALSE(client.lastStateUpdate().has_value());

    Message msg;
    msg.kind = MessageKind::StateUpdate;
    msg.payload = makeSmallStateUpdate();

    session->injectIncoming(msg);

    auto last = client.lastStateUpdate();
    REQUIRE(last.has_value());
    CHECK(last->serverTick == 42);
    REQUIRE(last->players.size() == 1);
    CHECK(last->players[0].name == "Alice");
}

TEST_CASE("NetworkClient receives MatchResult and stores it", "[network][client][matchresult]")
{
    auto session = std::make_shared<FakeNetworkSession>();
    NetworkClient client(session, "Alice");

    REQUIRE_FALSE(client.lastMatchResult().has_value());

    MatchResult result;
    result.endTick = 99;
    result.playerId = 2u;
    result.outcome = MatchOutcome::Win;
    result.finalScore = 999;

    Message msg;
    msg.kind = MessageKind::MatchResult;
    msg.payload = result;

    session->injectIncoming(msg);

    auto last = client.lastMatchResult();
    REQUIRE(last.has_value());
    CHECK(last->playerId == 2u);
    CHECK(last->outcome == MatchOutcome::Win);
    CHECK(last->finalScore == 999);
}

// ============================
// NetworkHost tests
// ============================

TEST_CASE("NetworkHost assigns client ids starting at 2 and sends JoinAccept", "[network][host]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode = GameMode::TimeAttack;

    NetworkHost host(cfg);

    auto s = std::make_shared<FakeNetworkSession>();
    host.addClient(s);

    Message req;
    req.kind = MessageKind::JoinRequest;
    req.payload = JoinRequest{ "Alice" };
    s->injectIncoming(req);

    REQUIRE_FALSE(s->sentMessages.empty());
    CHECK(s->sentMessages[0].kind == MessageKind::JoinAccept);

    const auto* p = std::get_if<JoinAccept>(&s->sentMessages[0].payload);
    REQUIRE(p != nullptr);
    CHECK(p->assignedId >= 2u);
}

TEST_CASE("NetworkHost stores incoming InputActionMessage (queue)", "[network][host][input]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode = GameMode::TimeAttack;

    NetworkHost host(cfg);
    auto s = std::make_shared<FakeNetworkSession>();
    host.addClient(s);

    Message req;
    req.kind = MessageKind::JoinRequest;
    req.payload = JoinRequest{ "Bob" };
    s->injectIncoming(req);

    const PlayerId pid = extractAssignedIdOrFail(s);
    s->sentMessages.clear();

    Message in;
    in.kind = MessageKind::InputActionMessage;
    in.payload = InputActionMessage{ pid, 5u, tetris::controller::InputAction::SoftDrop };
    s->injectIncoming(in);

    auto q = host.consumeInputQueue();
    REQUIRE(q.size() == 1);
    CHECK(q[0].playerId == pid);
    CHECK(q[0].clientTick == 5u);
    CHECK(q[0].action == tetris::controller::InputAction::SoftDrop);
}

// ============================
// HostGameSession tests (using SharedTurnRules only, no TimeAttackRules header)
// ============================

TEST_CASE("HostGameSession::isInputAllowed - SharedTurns only current player allowed", "[network][hostgamesession][inputallowed][sharedturns]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode = GameMode::SharedTurns;
    cfg.piecesPerTurn = 1;

    NetworkHost host(cfg);
    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    Message jr;
    jr.kind = MessageKind::JoinRequest;
    jr.payload = JoinRequest{ "Client" };
    session->injectIncoming(jr);
    const PlayerId cid = extractAssignedIdOrFail(session);
    session->sentMessages.clear();

    auto rules = std::make_unique<tetris::core::SharedTurnRules>(cfg.piecesPerTurn);
    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<tetris::core::PlayerSnapshot> players = {
        { NetworkHost::HostPlayerId, 0, true },
        { cid, 0, true }
    };

    CHECK_FALSE(gameSession.isInputAllowed(NetworkHost::HostPlayerId));
    CHECK_FALSE(gameSession.isInputAllowed(cid));

    gameSession.start(0, players);

    CHECK(gameSession.isInputAllowed(NetworkHost::HostPlayerId));
    CHECK_FALSE(gameSession.isInputAllowed(cid));

    gameSession.onPieceLocked(NetworkHost::HostPlayerId, players);

    CHECK_FALSE(gameSession.isInputAllowed(NetworkHost::HostPlayerId));
    CHECK(gameSession.isInputAllowed(cid));
}

TEST_CASE("HostGameSession finishes and sends MatchResult to each client by assignedId", "[network][hostgamesession][matchresult]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode = GameMode::SharedTurns;
    cfg.piecesPerTurn = 1;

    NetworkHost host(cfg);

    auto session1 = std::make_shared<FakeNetworkSession>();
    auto session2 = std::make_shared<FakeNetworkSession>();
    host.addClient(session1);
    host.addClient(session2);

    // join both -> learn assigned ids (2,3,...)
    Message jr;
    jr.kind = MessageKind::JoinRequest;

    jr.payload = JoinRequest{ "P1" };
    session1->injectIncoming(jr);
    const PlayerId pid1 = extractAssignedIdOrFail(session1);

    jr.payload = JoinRequest{ "P2" };
    session2->injectIncoming(jr);
    const PlayerId pid2 = extractAssignedIdOrFail(session2);

    session1->sentMessages.clear();
    session2->sentMessages.clear();

    auto rules = std::make_unique<tetris::core::SharedTurnRules>(cfg.piecesPerTurn);
    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<tetris::core::PlayerSnapshot> initial = {
        { pid1, 100, true },
        { pid2,  50, true }
    };

    gameSession.start(/*startTick=*/0, initial);

    // SharedTurns: emulate actual match flow:
    // - at least one piece lock happens
    // - then one player dies
    // - another lock happens (or simply the next rules step) and update() returns results
    std::vector<tetris::core::PlayerSnapshot> players = initial;

    // First lock while both alive (rotates / advances internal state)
    gameSession.onPieceLocked(pid1, players);

    // Now kill player 2
    players[1].isAlive = false;

    // Second lock after the death (rules can now determine winner/loser)
    gameSession.onPieceLocked(pid1, players);

    auto results = gameSession.update(/*currentTick=*/100, players);
    REQUIRE_FALSE(results.empty());
    CHECK(gameSession.isFinished());
    CHECK(gameSession.isFinished());

    REQUIRE(hasMatchResultFor(session1->sentMessages, pid1));
    REQUIRE(hasMatchResultFor(session2->sentMessages, pid2));
}
