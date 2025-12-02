#include <catch2/catch_test_macros.hpp>

#include "core/MatchRules.hpp"
#include "network/MessageTypes.hpp"

using tetris::core::PlayerSnapshot;
using tetris::core::TimeAttackRules;
using tetris::net::MatchOutcome;

TEST_CASE("TimeAttackRules: no result before time limit", "[rules][timeattack]")
{
    constexpr tetris::net::Tick timeLimit = 100;
    TimeAttackRules rules(timeLimit);

    rules.onMatchStart(/*startTick=*/50);

    std::vector<PlayerSnapshot> players = {
        {1u, 100, true},
        {2u, 200, true}
    };

    auto results = rules.update(/*currentTick=*/120 - 1, players);
    REQUIRE(results.empty());
    CHECK_FALSE(rules.isFinished());
}

TEST_CASE("TimeAttackRules: no result before time limit. V2", "[rules][timeattack]")
{
    constexpr tetris::net::Tick timeLimit = 100;
    TimeAttackRules rules(timeLimit);

    std::vector<PlayerSnapshot> players = {
        {1u, 100, true},
        {2u, 200, true}
    };

    rules.initializePlayers(players);
    rules.onMatchStart(/*startTick=*/50);

    auto results = rules.update(/*currentTick=*/120 - 1, players);
    REQUIRE(results.empty());
    CHECK_FALSE(rules.isFinished());
}

TEST_CASE("TimeAttackRules: single winner after time limit", "[rules][timeattack]")
{
    constexpr tetris::net::Tick timeLimit = 100;
    TimeAttackRules rules(timeLimit);

    rules.onMatchStart(0);

    std::vector<PlayerSnapshot> players = {
        {1u, 300, true},
        {2u, 500, true}
    };

    auto results = rules.update(/*currentTick=*/100, players);
    REQUIRE_FALSE(results.empty());
    CHECK(rules.isFinished());

    // Expect 2 results (one per player)
    REQUIRE(results.size() == 2);

    const auto& p1 = results[0];
    const auto& p2 = results[1];

    if (p1.playerId == 1u) {
        CHECK(p1.outcome == MatchOutcome::Lose);
        CHECK(p2.playerId == 2u);
        CHECK(p2.outcome == MatchOutcome::Win);
    } else {
        CHECK(p1.playerId == 2u);
        CHECK(p1.outcome == MatchOutcome::Win);
        CHECK(p2.playerId == 1u);
        CHECK(p2.outcome == MatchOutcome::Lose);
    }
}

TEST_CASE("TimeAttackRules: draw when scores are equal", "[rules][timeattack]")
{
    constexpr tetris::net::Tick timeLimit = 50;
    TimeAttackRules rules(timeLimit);

    rules.onMatchStart(0);

    std::vector<PlayerSnapshot> players = {
        {1u, 400, true},
        {2u, 400, true}
    };

    auto results = rules.update(/*currentTick=*/50, players);
    REQUIRE(results.size() == 2);
    CHECK(rules.isFinished());

    for (const auto& r : results) {
        CHECK(r.outcome == MatchOutcome::Draw);
        CHECK((r.playerId == 1u || r.playerId == 2u));
        CHECK(r.finalScore == 400);
    }
}

TEST_CASE("TimeAttackRules: multiple calls after finish are stable", "[rules][timeattack]")
{
    constexpr tetris::net::Tick timeLimit = 10;
    TimeAttackRules rules(timeLimit);

    rules.onMatchStart(0);

    std::vector<PlayerSnapshot> players = {
        {1u, 10, true},
        {2u, 20, true}
    };

    auto first = rules.update(/*currentTick=*/10, players);
    REQUIRE_FALSE(first.empty());
    CHECK(rules.isFinished());

    auto second = rules.update(/*currentTick=*/20, players);
    REQUIRE(second.size() == first.size());

    // Make sure they don't change after finishing
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].playerId   == second[i].playerId);
        CHECK(first[i].outcome    == second[i].outcome);
        CHECK(first[i].finalScore == second[i].finalScore);
        CHECK(first[i].endTick    == second[i].endTick);
    }
}
