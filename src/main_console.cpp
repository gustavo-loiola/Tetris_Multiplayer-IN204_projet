#include <iostream>
#include <string>
#include <vector>

#include "core/GameState.hpp"
#include "core/Board.hpp"
#include "core/Tetromino.hpp"
#include "core/Types.hpp"

using namespace tetris::core;

// Helper: render the current board + active tetromino as ASCII
void printGame(const GameState& game) {
    const Board& board = game.board();
    const int rows = board.rows();
    const int cols = board.cols();

    // Start with base board
    std::vector<std::string> lines(rows, std::string(cols, ' '));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board.cell(r, c) == CellState::Filled) {
                lines[r][c] = '#'; // locked blocks
            } else {
                lines[r][c] = '.'; // empty
            }
        }
    }

    // Overlay active tetromino as 'X'
    if (game.activeTetromino()) {
        auto blocks = game.activeTetromino()->blocks();
        for (const auto& b : blocks) {
            if (b.row >= 0 && b.row < rows && b.col >= 0 && b.col < cols) {
                lines[b.row][b.col] = 'X';
            }
        }
    }

    std::cout << "\n==== TETRIS CONSOLE VIEW ====\n";
    std::cout << "Score: " << game.score()
              << " | Level: " << game.level()
              << " | Status: ";

    switch (game.status()) {
    case GameStatus::NotStarted: std::cout << "NotStarted"; break;
    case GameStatus::Running:    std::cout << "Running";    break;
    case GameStatus::Paused:     std::cout << "Paused";     break;
    case GameStatus::GameOver:   std::cout << "GameOver";   break;
    }
    std::cout << '\n';

    // Print board with borders
    std::cout << '+' << std::string(cols, '-') << "+\n";
    for (int r = 0; r < rows; ++r) {
        std::cout << '|';
        std::cout << lines[r];
        std::cout << "|\n";
    }
    std::cout << '+' << std::string(cols, '-') << "+\n";

    std::cout << "Commands:\n"
              << "  a = left, d = right, s = soft drop, w = rotate CW\n"
              << "  h = hard drop, g = gravity tick\n"
              << "  p = pause/resume, r = reset+start, q = quit\n";
}

int main() {
    GameState game{20, 10, 0}; // rows, cols, startingLevel

    game.start(); // start immediately

    std::string cmd;
    printGame(game);

    while (true) {
        std::cout << "\nEnter command: ";
        if (!std::getline(std::cin, cmd)) {
            break; // EOF
        }
        if (cmd.empty()) {
            continue;
        }

        char c = cmd[0];
        if (c == 'q' || c == 'Q') {
            std::cout << "Quitting.\n";
            break;
        }

        switch (c) {
        case 'a': case 'A':
            game.moveLeft();
            break;
        case 'd': case 'D':
            game.moveRight();
            break;
        case 's': case 'S':
            game.softDrop();
            break;
        case 'w': case 'W':
            game.rotateClockwise();
            break;
        case 'h': case 'H':
            game.hardDrop();
            break;
        case 'g': case 'G':
            game.tick(); // one gravity step
            break;
        case 'p': case 'P':
            if (game.status() == GameStatus::Running)
                game.pause();
            else if (game.status() == GameStatus::Paused)
                game.resume();
            break;
        case 'r': case 'R':
            game.reset();
            game.start();
            break;
        default:
            std::cout << "Unknown command: " << c << '\n';
            break;
        }

        printGame(game);

        if (game.status() == GameStatus::GameOver) {
            std::cout << "GAME OVER. Press 'r' to restart or 'q' to quit.\n";
        }
    }

    return 0;
}
