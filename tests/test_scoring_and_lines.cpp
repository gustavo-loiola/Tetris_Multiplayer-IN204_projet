#include <catch2/catch_test_macros.hpp>

#include "core/Board.hpp"
#include "core/ScoreManager.hpp"
#include "core/Types.hpp"

using namespace tetris::core;

TEST_CASE("ScoringAndLines: Board basic cell operations", "[scoring][board]")
{
    Board b{4, 4};

    REQUIRE(b.rows() == 4);
    REQUIRE(b.cols() == 4);

    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            REQUIRE(b.cell(r, c) == CellState::Empty);
        }
    }

    b.setCell(1, 1, CellState::Filled);
    REQUIRE(b.cell(1, 1) == CellState::Filled);
}

TEST_CASE("ScoringAndLines: Board clears a single full line and shifts down", "[scoring][board][lines]")
{
    Board b{4, 4};

    // Fill bottom row (row 3)
    for (int c = 0; c < 4; ++c) {
        b.setCell(3, c, CellState::Filled);
    }

    // Place a block above to check it falls down after line clear
    b.setCell(2, 0, CellState::Filled);

    const int cleared = b.clearFullLines();
    REQUIRE(cleared == 1);

    // Row 3 should now have what row 2 had before
    REQUIRE(b.cell(3, 0) == CellState::Filled);
    for (int c = 1; c < 4; ++c) {
        REQUIRE(b.cell(3, c) == CellState::Empty);
    }

    // Row 2 should now be empty
    for (int c = 0; c < 4; ++c) {
        REQUIRE(b.cell(2, c) == CellState::Empty);
    }
}

TEST_CASE("ScoringAndLines: Board clears multiple lines", "[scoring][board][lines]")
{
    Board b{4, 4};

    // Fill rows 2 and 3 fully
    for (int c = 0; c < 4; ++c) {
        b.setCell(2, c, CellState::Filled);
        b.setCell(3, c, CellState::Filled);
    }

    // Put a block in row 1 to verify shifting
    b.setCell(1, 1, CellState::Filled);

    const int cleared = b.clearFullLines();
    REQUIRE(cleared == 2);

    // After clearing, row 3 should contain what row 1 had
    REQUIRE(b.cell(3, 1) == CellState::Filled);
    for (int c = 0; c < 4; ++c) {
        if (c != 1) REQUIRE(b.cell(3, c) == CellState::Empty);
    }

    // Rows 0,1,2 should be empty
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
            REQUIRE(b.cell(r, c) == CellState::Empty);
        }
    }
}

TEST_CASE("ScoringAndLines: ScoreManager follows classic Tetris scoring", "[scoring][scoremanager]")
{
    ScoreManager s;

    SECTION("No lines gives no score")
    {
        s.addLinesCleared(0, 0);
        REQUIRE(s.score() == 0);
    }

    SECTION("Single line clear")
    {
        s.addLinesCleared(1, 0);
        REQUIRE(s.score() == 40);

        s.reset();
        s.addLinesCleared(1, 4); // (4+1)*40
        REQUIRE(s.score() == 200);
    }

    SECTION("Double line clear")
    {
        s.addLinesCleared(2, 0);
        REQUIRE(s.score() == 100);

        s.reset();
        s.addLinesCleared(2, 2); // (2+1)*100
        REQUIRE(s.score() == 300);
    }

    SECTION("Triple line clear")
    {
        s.addLinesCleared(3, 0);
        REQUIRE(s.score() == 300);

        s.reset();
        s.addLinesCleared(3, 1); // (1+1)*300
        REQUIRE(s.score() == 600);
    }

    SECTION("Tetris (4 lines)")
    {
        s.addLinesCleared(4, 0);
        REQUIRE(s.score() == 1200);

        s.reset();
        s.addLinesCleared(4, 5); // (5+1)*1200
        REQUIRE(s.score() == 7200);
    }

    SECTION("Ignore weird values like >4 lines")
    {
        s.addLinesCleared(5, 0);
        REQUIRE(s.score() == 0);
    }
}
