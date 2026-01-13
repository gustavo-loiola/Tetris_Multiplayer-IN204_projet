#include <catch2/catch_test_macros.hpp>

#include "core/MatchRules.hpp"
#include "network/MessageTypes.hpp"

#include <algorithm>

using tetris::core::PlayerSnapshot;
using tetris::core::SharedTurnRules;
using tetris::net::MatchOutcome;

TEST_CASE("SharedTurnRules: rotates after piecesPerTurn", "[rules][sharedturn]")
{
    SharedTurnRules rules(/*piecesPerTurn=*/2);

    std::vector<PlayerSnapshot> players = {
        {1u, 0, true},
        {2u, 0, true}
    };

    rules.initializePlayers(players);
    rules.onMatchStart(/*startTick=*/0);

    // Initially first player should be 1
    CHECK(rules.currentPlayer() == 1u);

    // First piece by player 1
    rules.onPieceLocked(1u, players);
    CHECK(rules.currentPlayer() == 1u); // still player 1 (1 piece used)

    // Second piece by player 1 => rotation
    rules.onPieceLocked(1u, players);
    CHECK(rules.currentPlayer() == 2u);
}

TEST_CASE("SharedTurnRules: skips dead players on rotation", "[rules][sharedturn]")
{
    SharedTurnRules rules(1); // rotate every piece

    std::vector<PlayerSnapshot> players = {
        {1u, 0, true},
        {2u, 0, true},
        {3u, 0, true}
    };

    rules.initializePlayers(players);
    rules.onMatchStart(0);

    CHECK(rules.currentPlayer() == 1u);

    // P1 turn -> P2
    rules.onPieceLocked(1u, players);
    CHECK(rules.currentPlayer() == 2u);

    // Now mark player 2 dead
    players[1].isAlive = false;

    // P2's piece locks (even if isAlive changed after)
    rules.onPieceLocked(2u, players);
    // Next alive should be player 3
    CHECK(rules.currentPlayer() == 3u);

    // Next piece from P3 should rotate back to P1
    rules.onPieceLocked(3u, players);
    CHECK(rules.currentPlayer() == 1u);
}

TEST_CASE("SharedTurnRules: finishes when only one alive", "[rules][sharedturn]")
{
    SharedTurnRules rules(1);

    std::vector<PlayerSnapshot> players = {
        {1u, 100, true},
        {2u, 200, true}
    };

    rules.initializePlayers(players);
    rules.onMatchStart(0);

    // Initially both alive; piece by P1
    rules.onPieceLocked(1u, players);
    CHECK_FALSE(rules.isFinished());

    // Kill player 2
    players[1].isAlive = false;

    // Next lock by P1 should detect only one alive and finish
    rules.onPieceLocked(1u, players);
    CHECK(rules.isFinished());

    auto results = rules.update(/*currentTick=*/42, players);
    REQUIRE(results.size() == 2);

    auto it1 = std::find_if(results.begin(), results.end(),
                        [](const auto& r){ return r.playerId == 1u; });
    auto it2 = std::find_if(results.begin(), results.end(),
                            [](const auto& r){ return r.playerId == 2u; });

    REQUIRE(it1 != results.end());
    REQUIRE(it2 != results.end());

    CHECK(it1->outcome == MatchOutcome::Win); // or Draw, Lose depending on test case
    CHECK(it2->outcome == MatchOutcome::Lose);
}

TEST_CASE("SharedTurnRules: draw when survivors tie", "[rules][sharedturn]")
{
    SharedTurnRules rules(1);

    std::vector<PlayerSnapshot> players = {
        {1u, 300, true},
        {2u, 300, true}
    };

    rules.initializePlayers(players);
    rules.onMatchStart(0);

    // Kill neither; then force finish manually by marking both dead.
    players[0].isAlive = false;
    players[1].isAlive = false;

    // Any piece lock will mark finished since alive <= 1
    rules.onPieceLocked(1u, players);
    CHECK(rules.isFinished());

    auto results = rules.update(100, players);
    REQUIRE(results.size() == 2);

    for (const auto& r : results) {
        CHECK(r.outcome == MatchOutcome::Draw);
        CHECK(r.finalScore == 300);
    }
}