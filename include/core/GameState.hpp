#pragma once

#include "Board.hpp"
#include "Tetromino.hpp"
#include "TetrominoFactory.hpp"
#include "ScoreManager.hpp"
#include "LevelManager.hpp"
#include <optional>

namespace tetris::core {

enum class GameStatus {
    NotStarted,
    Running,
    Paused,
    GameOver
};

class GameState {
public:
    GameState(int rows = 20, int cols = 10, int startingLevel = 0);

    const Board& board() const noexcept { return board_; }
    const std::optional<Tetromino>& activeTetromino() const noexcept { return activeTetromino_; }
    const std::optional<Tetromino>& nextTetromino() const noexcept { return nextTetromino_; }

    std::uint64_t score() const noexcept { return scoreManager_.score(); }
    int level() const noexcept { return levelManager_.level(); }
    GameStatus status() const noexcept { return status_; }

    // Control API for the controller / input layer
    void start();
    void pause();
    void resume();
    void reset();

    // One gravity step (called periodically according to LevelManager)
    // Returns true if piece moved down; false if it locked or could not move
    bool tick();

    // Player actions
    void moveLeft();
    void moveRight();
    void softDrop();  // move down one, if possible
    void hardDrop();  // drop to the bottom and lock

    void rotateClockwise();
    void rotateCounterClockwise();

    int gravityIntervalMs() const noexcept {
    return levelManager_.gravityIntervalMs();
    }

private:
    Board board_;
    TetrominoFactory factory_;
    ScoreManager scoreManager_;
    LevelManager levelManager_;

    std::optional<Tetromino> activeTetromino_;
    std::optional<Tetromino> nextTetromino_;

    GameStatus status_{GameStatus::NotStarted};

    bool spawnNewTetromino();
    void lockActiveTetrominoAndProcessLines();

    // Helper to try move active tetromino by dx, dy
    bool tryMove(int dRow, int dCol);
    bool tryRotate(bool clockwise);
};

} // namespace tetris::core
