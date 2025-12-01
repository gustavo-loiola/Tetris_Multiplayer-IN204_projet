#include <iostream>
#include <string>
#include <vector>

#include "core/GameState.hpp"
#include "core/Board.hpp"
#include "core/Tetromino.hpp"
#include "core/Types.hpp"
#include "controller/GameController.hpp"

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
    tetris::controller::GameController controller{game};

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

        using tetris::controller::InputAction;

        switch (c) {
        case 'a': case 'A':
            controller.handleAction(InputAction::MoveLeft);
            break;
        case 'd': case 'D':
            controller.handleAction(InputAction::MoveRight);
            break;
        case 's': case 'S':
            controller.handleAction(InputAction::SoftDrop);
            break;
        case 'w': case 'W':
            controller.handleAction(InputAction::RotateCW);
            break;
        case 'h': case 'H':
            controller.handleAction(InputAction::HardDrop);
            break;
        case 'g': case 'G': {
            const int intervalMs = game.gravityIntervalMs();
            if (intervalMs > 0) {
                controller.update(
                    tetris::controller::GameController::Duration{intervalMs});
            } else {
                std::cout << "Gravity interval is non-positive; no tick.\n";
            }
            break;
        }
        case 'p': case 'P':
            controller.handleAction(InputAction::PauseResume);
            break;
        case 'r': case 'R':
            game.reset();
            game.start();
            controller.resetTiming();
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