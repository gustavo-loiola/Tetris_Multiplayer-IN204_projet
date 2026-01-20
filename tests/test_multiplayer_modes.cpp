// tests/test_multiplayer_modes.cpp

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "core/MatchRules.hpp"
#include "network/MessageTypes.hpp"

using tetris::core::PlayerSnapshot;
using tetris::core::TimeAttackRules;
using tetris::core::SharedTurnRules;

using tetris::net::Tick;
using tetris::net::PlayerId;
using tetris::net::MatchOutcome;
using tetris::net::MatchResult;

// ----------------------------
// Helpers (shared by both modes)
// ----------------------------

static std::vector<PlayerSnapshot> make2P(int s1, bool a1,
                                         int s2, bool a2)
{
    std::vector<PlayerSnapshot> players;
    players.push_back(PlayerSnapshot{ 1u, s1, a1 });
    players.push_back(PlayerSnapshot{ 2u, s2, a2 });
    return players;
}

static const MatchResult& findResult(const std::vector<MatchResult>& results, PlayerId id)
{
    auto it = std::find_if(results.begin(), results.end(),
                           [id](const MatchResult& r){ return r.playerId == id; });
    REQUIRE(it != results.end());
    return *it;
}

static void requireStableAfterFinish(tetris::core::IMatchRules& rules,
                                     Tick laterTick,
                                     const std::vector<PlayerSnapshot>& players,
                                     const std::vector<MatchResult>& first)
{
    REQUIRE(rules.isFinished());

    auto second = rules.update(laterTick, players);
    REQUIRE(second.size() == first.size());

    // Same content after finish (order may or may not be stable depending on impl,
    // so compare per-player)
    for (const auto& r : first) {
        const auto& rr = findResult(second, r.playerId);
        CHECK(rr.playerId   == r.playerId);
        CHECK(rr.outcome    == r.outcome);
        CHECK(rr.finalScore == r.finalScore);
        CHECK(rr.endTick    == r.endTick);
    }
}

// ======================================================
// TimeAttack mode
// ======================================================

TEST_CASE("Multiplayer modes: TimeAttackRules", "[modes][timeattack]")
{
    SECTION("no result before time limit")
    {
        constexpr Tick timeLimit = 100;
        TimeAttackRules rules(timeLimit);

        // start at tick 50 => ends at tick 150
        rules.onMatchStart(/*startTick=*/50);

        auto players = make2P(/*s1=*/100, /*a1=*/true, /*s2=*/200, /*a2=*/true);

        auto results = rules.update(/*currentTick=*/149, players);
        REQUIRE(results.empty());
        CHECK_FALSE(rules.isFinished());
    }

    SECTION("no result before time limit (with initializePlayers)")
    {
        constexpr Tick timeLimit = 100;
        TimeAttackRules rules(timeLimit);

        auto players = make2P(100, true, 200, true);
        rules.initializePlayers(players);
        rules.onMatchStart(/*startTick=*/50);

        auto results = rules.update(/*currentTick=*/149, players);
        REQUIRE(results.empty());
        CHECK_FALSE(rules.isFinished());
    }

    SECTION("single winner after time limit")
    {
        constexpr Tick timeLimit = 100;
        TimeAttackRules rules(timeLimit);

        rules.onMatchStart(/*startTick=*/0);

        auto players = make2P(/*s1=*/300, true, /*s2=*/500, true);

        auto results = rules.update(/*currentTick=*/100, players);
        REQUIRE_FALSE(results.empty());
        REQUIRE(results.size() == 2);
        CHECK(rules.isFinished());

        const auto& r1 = findResult(results, 1u);
        const auto& r2 = findResult(results, 2u);

        CHECK(r1.finalScore == 300);
        CHECK(r2.finalScore == 500);

        CHECK(r1.outcome == MatchOutcome::Lose);
        CHECK(r2.outcome == MatchOutcome::Win);

        requireStableAfterFinish(rules, /*laterTick=*/200, players, results);
    }

    SECTION("draw when scores are equal")
    {
        constexpr Tick timeLimit = 50;
        TimeAttackRules rules(timeLimit);

        rules.onMatchStart(/*startTick=*/0);

        auto players = make2P(/*s1=*/400, true, /*s2=*/400, true);

        auto results = rules.update(/*currentTick=*/50, players);
        REQUIRE(results.size() == 2);
        CHECK(rules.isFinished());

        const auto& r1 = findResult(results, 1u);
        const auto& r2 = findResult(results, 2u);

        CHECK(r1.outcome == MatchOutcome::Draw);
        CHECK(r2.outcome == MatchOutcome::Draw);
        CHECK(r1.finalScore == 400);
        CHECK(r2.finalScore == 400);

        requireStableAfterFinish(rules, /*laterTick=*/60, players, results);
    }

    SECTION("multiple calls after finish are stable")
    {
        constexpr Tick timeLimit = 10;
        TimeAttackRules rules(timeLimit);

        rules.onMatchStart(0);

        auto players = make2P(/*s1=*/10, true, /*s2=*/20, true);

        auto first = rules.update(/*currentTick=*/10, players);
        REQUIRE_FALSE(first.empty());
        CHECK(rules.isFinished());

        auto second = rules.update(/*currentTick=*/20, players);
        REQUIRE(second.size() == first.size());

        // Compare by player
        const auto& f1 = findResult(first, 1u);
        const auto& f2 = findResult(first, 2u);
        const auto& s1 = findResult(second, 1u);
        const auto& s2 = findResult(second, 2u);

        CHECK(f1.outcome == s1.outcome);
        CHECK(f1.finalScore == s1.finalScore);
        CHECK(f1.endTick == s1.endTick);

        CHECK(f2.outcome == s2.outcome);
        CHECK(f2.finalScore == s2.finalScore);
        CHECK(f2.endTick == s2.endTick);
    }
}

// ======================================================
// SharedTurns mode
// ======================================================

TEST_CASE("Multiplayer modes: SharedTurnRules", "[modes][sharedturns]")
{
    SECTION("rotates after piecesPerTurn")
    {
        SharedTurnRules rules(/*piecesPerTurn=*/2);

        std::vector<PlayerSnapshot> players = {
            {1u, 0, true},
            {2u, 0, true}
        };

        rules.initializePlayers(players);
        rules.onMatchStart(/*startTick=*/0);

        CHECK(rules.currentPlayer() == 1u);

        rules.onPieceLocked(1u, players);
        CHECK(rules.currentPlayer() == 1u); // still P1 (1 piece used)

        rules.onPieceLocked(1u, players);
        CHECK(rules.currentPlayer() == 2u); // rotated
    }

    SECTION("skips dead players on rotation")
    {
        SharedTurnRules rules(/*piecesPerTurn=*/1);

        std::vector<PlayerSnapshot> players = {
            {1u, 0, true},
            {2u, 0, true},
            {3u, 0, true}
        };

        rules.initializePlayers(players);
        rules.onMatchStart(0);

        CHECK(rules.currentPlayer() == 1u);

        // P1 -> P2
        rules.onPieceLocked(1u, players);
        CHECK(rules.currentPlayer() == 2u);

        // Kill P2
        players[1].isAlive = false;

        // Next lock on P2 should skip to P3
        rules.onPieceLocked(2u, players);
        CHECK(rules.currentPlayer() == 3u);

        // P3 -> back to P1
        rules.onPieceLocked(3u, players);
        CHECK(rules.currentPlayer() == 1u);
    }

    SECTION("finishes when only one alive")
    {
        SharedTurnRules rules(/*piecesPerTurn=*/1);

        auto players = make2P(/*s1=*/100, true, /*s2=*/200, true);

        rules.initializePlayers(players);
        rules.onMatchStart(0);

        rules.onPieceLocked(1u, players);
        CHECK_FALSE(rules.isFinished());

        // Kill player 2
        players[1].isAlive = false;

        // Next lock should end the match
        rules.onPieceLocked(1u, players);
        CHECK(rules.isFinished());

        auto results = rules.update(/*currentTick=*/42, players);
        REQUIRE(results.size() == 2);

        const auto& r1 = findResult(results, 1u);
        const auto& r2 = findResult(results, 2u);

        CHECK(r1.outcome == MatchOutcome::Win);
        CHECK(r2.outcome == MatchOutcome::Lose);

        requireStableAfterFinish(rules, /*laterTick=*/100, players, results);
    }

    SECTION("draw when survivors tie")
    {
        SharedTurnRules rules(/*piecesPerTurn=*/1);

        auto players = make2P(/*s1=*/300, true, /*s2=*/300, true);

        rules.initializePlayers(players);
        rules.onMatchStart(0);

        // Force "alive <= 1" condition by killing both (your old test did this)
        players[0].isAlive = false;
        players[1].isAlive = false;

        rules.onPieceLocked(1u, players);
        CHECK(rules.isFinished());

        auto results = rules.update(/*currentTick=*/100, players);
        REQUIRE(results.size() == 2);

        for (const auto& r : results) {
            CHECK(r.outcome == MatchOutcome::Draw);
            CHECK(r.finalScore == 300);
        }

        requireStableAfterFinish(rules, /*laterTick=*/200, players, results);
    }
}