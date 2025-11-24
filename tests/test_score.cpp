#include <catch2/catch_test_macros.hpp>

#include "core/ScoreManager.hpp"

using namespace tetris::core;

TEST_CASE("ScoreManager scoring rules follow Tetris spec", "[score]") {
    ScoreManager s;

    SECTION("No lines gives no score") {
        s.addLinesCleared(0, 0);
        REQUIRE(s.score() == 0);
    }

    SECTION("Single line clear") {
        s.addLinesCleared(1, 0); // level 0
        REQUIRE(s.score() == 40);

        s.reset();
        s.addLinesCleared(1, 4); // level 4 -> (4+1)*40 = 200
        REQUIRE(s.score() == 200);
    }

    SECTION("Double line clear") {
        s.addLinesCleared(2, 0); // level 0
        REQUIRE(s.score() == 100);

        s.reset();
        s.addLinesCleared(2, 2); // level 2 -> (2+1)*100 = 300
        REQUIRE(s.score() == 300);
    }

    SECTION("Triple line clear") {
        s.addLinesCleared(3, 0);
        REQUIRE(s.score() == 300);

        s.reset();
        s.addLinesCleared(3, 1); // (1+1)*300 = 600
        REQUIRE(s.score() == 600);
    }

    SECTION("Tetris (4 lines)") {
        s.addLinesCleared(4, 0);
        REQUIRE(s.score() == 1200);

        s.reset();
        s.addLinesCleared(4, 5); // (5+1)*1200 = 7200
        REQUIRE(s.score() == 7200);
    }

    SECTION("We ignore weird values like >4 lines") {
        s.addLinesCleared(5, 0); // should not crash, should not add score
        REQUIRE(s.score() == 0);
    }
}