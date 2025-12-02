#include <catch2/catch_test_macros.hpp>
#include <algorithm>

#include "network/HostGameSession.hpp"
#include "network/NetworkHost.hpp"
#include "network/MultiplayerConfig.hpp"
#include "core/MatchRules.hpp"
#include "network/MessageTypes.hpp"

#include "FakeNetworkSession.hpp"

using tetris::core::PlayerSnapshot;
using tetris::core::TimeAttackRules;
using tetris::core::SharedTurnRules;
using tetris::net::MultiplayerConfig;
using tetris::net::NetworkHost;
using tetris::net::HostGameSession;
using tetris::net::MatchOutcome;

TEST_CASE("HostGameSession: start initializes rules and notifies clients", "[host][session]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode = tetris::net::GameMode::TimeAttack;
    cfg.timeLimitSeconds = 120;

    NetworkHost host(cfg);

    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    // Build rules with some arbitrary tick limit (1000 just for test)
    auto rules = std::make_unique<TimeAttackRules>(1000);

    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<PlayerSnapshot> players = {
        {1u, 0, true}
    };

    // Before start: no StartGame messages
    REQUIRE(session->sentMessages.empty());

    gameSession.start(/*startTick=*/0, players);

    // NetworkHost should have sent a StartGame to the client.
    REQUIRE(session->sentMessages.size() == 1);
    const auto& msg = session->sentMessages[0];
    CHECK(msg.kind == tetris::net::MessageKind::StartGame);

    const auto* payload = std::get_if<tetris::net::StartGame>(&msg.payload);
    REQUIRE(payload != nullptr);
    CHECK(payload->mode == tetris::net::GameMode::TimeAttack);
    CHECK(payload->timeLimitSeconds == cfg.timeLimitSeconds);
}

TEST_CASE("HostGameSession: consumes input queue via host", "[host][session]")
{
    MultiplayerConfig cfg;
    cfg.mode = tetris::net::GameMode::TimeAttack;
    cfg.timeLimitSeconds = 60;

    NetworkHost host(cfg);
    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    auto rules = std::make_unique<TimeAttackRules>(1000);
    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<PlayerSnapshot> players = {
        {1u, 0, true}
    };
    gameSession.start(0, players);

    // Simulate an InputActionMessage from the client.
    tetris::net::Message inputMsg;
    inputMsg.kind = tetris::net::MessageKind::InputActionMessage;
    inputMsg.payload = tetris::net::InputActionMessage{
        1u,
        42u,
        tetris::controller::InputAction::MoveLeft
    };

    session->injectIncoming(inputMsg);

    auto queue = gameSession.consumePendingInputs();
    REQUIRE(queue.size() == 1);
    CHECK(queue[0].playerId == 1u);
    CHECK(queue[0].clientTick == 42u);
    CHECK(queue[0].action == tetris::controller::InputAction::MoveLeft);
}

TEST_CASE("HostGameSession: SharedTurnRules finish propagates results", "[host][session][sharedturn]")
{
    MultiplayerConfig cfg;
    cfg.mode = tetris::net::GameMode::SharedTurns;
    cfg.piecesPerTurn = 1;

    NetworkHost host(cfg);
    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    auto rules = std::make_unique<SharedTurnRules>(cfg.piecesPerTurn);
    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<PlayerSnapshot> players = {
        {1u, 100, true},
        {2u, 200, true}
    };

    gameSession.start(/*startTick=*/0, players);

    // Simulate some turns: first piece locks for player 1.
    gameSession.onPieceLocked(1u, players);
    CHECK_FALSE(gameSession.isFinished());

    // Now mark player 2 dead, and lock a piece for player 1 again.
    players[1].isAlive = false;
    gameSession.onPieceLocked(1u, players);

    // Now update; SharedTurnRules should decide a winner.
    auto results = gameSession.update(/*currentTick=*/50, players);
    REQUIRE_FALSE(results.empty());
    CHECK(gameSession.isFinished());

    REQUIRE(results.size() == 2);

    auto r1 = std::find_if(results.begin(), results.end(),
                       [](const auto& r){ return r.playerId == 1u; });
    auto r2 = std::find_if(results.begin(), results.end(),
                        [](const auto& r){ return r.playerId == 2u; });

    REQUIRE(r1 != results.end());
    REQUIRE(r2 != results.end());

    CHECK(r1->outcome == MatchOutcome::Win);
    CHECK(r2->outcome == MatchOutcome::Lose);
}

TEST_CASE("HostGameSession::isInputAllowed - TimeAttack allows all players", "[host][session][timeattack]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode   = tetris::net::GameMode::TimeAttack;
    cfg.timeLimitSeconds = 60;

    NetworkHost host(cfg);
    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    auto rules = std::make_unique<TimeAttackRules>(/*timeLimitTicks=*/1000);
    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<PlayerSnapshot> players = {
        {1u, 0, true},
        {2u, 0, true}
    };

    // Before start: inputs should not be allowed.
    CHECK_FALSE(gameSession.isInputAllowed(1u));
    CHECK_FALSE(gameSession.isInputAllowed(2u));

    gameSession.start(/*startTick=*/0, players);

    // For TimeAttack, all players are allowed to act at any time.
    CHECK(gameSession.isInputAllowed(1u));
    CHECK(gameSession.isInputAllowed(2u));
}

TEST_CASE("HostGameSession::isInputAllowed - SharedTurns only current player allowed", "[host][session][sharedturn][input]")
{
    MultiplayerConfig cfg;
    cfg.isHost = true;
    cfg.mode   = tetris::net::GameMode::SharedTurns;
    cfg.piecesPerTurn = 1; // rotate after each locked piece

    NetworkHost host(cfg);
    auto session = std::make_shared<FakeNetworkSession>();
    host.addClient(session);

    auto rules = std::make_unique<SharedTurnRules>(cfg.piecesPerTurn);
    HostGameSession gameSession(host, cfg, std::move(rules));

    std::vector<PlayerSnapshot> players = {
        {1u, 0, true},  // player 1
        {2u, 0, true}   // player 2
    };

    // Before start: inputs should not be allowed.
    CHECK_FALSE(gameSession.isInputAllowed(1u));
    CHECK_FALSE(gameSession.isInputAllowed(2u));

    gameSession.start(/*startTick=*/0, players);

    // SharedTurnRules initializes order: first player in the list should be current.
    // So initially, only player 1 is allowed to act.
    CHECK(gameSession.isInputAllowed(1u));
    CHECK_FALSE(gameSession.isInputAllowed(2u));

    // Simulate a piece lock for player 1 -> SharedTurnRules should rotate to player 2.
    gameSession.onPieceLocked(1u, players);

    // Now only player 2 should be allowed to act.
    CHECK_FALSE(gameSession.isInputAllowed(1u));
    CHECK(gameSession.isInputAllowed(2u));

    // Mark player 2 as dead and lock another piece for player 1:
    // rules may decide the match finishes; once finished, no inputs should be allowed.
    players[1].isAlive = false;
    gameSession.onPieceLocked(1u, players);
    auto results = gameSession.update(/*currentTick=*/100, players);
    REQUIRE_FALSE(results.empty());
    CHECK(gameSession.isFinished());

    CHECK_FALSE(gameSession.isInputAllowed(1u));
    CHECK_FALSE(gameSession.isInputAllowed(2u));
}
