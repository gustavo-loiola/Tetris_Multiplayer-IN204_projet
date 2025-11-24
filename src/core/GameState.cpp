#include "core/GameState.hpp"

namespace tetris::core {

GameState::GameState(int rows, int cols, int startingLevel)
    : board_{rows, cols}
    , factory_{}
    , scoreManager_{}
    , levelManager_{startingLevel}
    , activeTetromino_{}
    , nextTetromino_{}
    , status_{GameStatus::NotStarted}
{
}

void GameState::start() {
    if (status_ == GameStatus::Running) return;

    scoreManager_.reset();
    levelManager_.reset(levelManager_.level()); // keep current starting level
    board_ = Board(board_.rows(), board_.cols()); // reset grid

    activeTetromino_.reset();
    nextTetromino_.reset();

    status_ = GameStatus::Running;

    if (!spawnNewTetromino()) {
        status_ = GameStatus::GameOver;
    }
}

void GameState::pause() {
    if (status_ == GameStatus::Running) {
        status_ = GameStatus::Paused;
    }
}

void GameState::resume() {
    if (status_ == GameStatus::Paused) {
        status_ = GameStatus::Running;
    }
}

void GameState::reset() {
    scoreManager_.reset();
    levelManager_.reset(0);
    board_ = Board(board_.rows(), board_.cols());
    activeTetromino_.reset();
    nextTetromino_.reset();
    status_ = GameStatus::NotStarted;
}

bool GameState::tick() {
    if (status_ != GameStatus::Running) {
        return false;
    }

    if (!activeTetromino_) {
        if (!spawnNewTetromino()) {
            status_ = GameStatus::GameOver;
            return false;
        }
    }

    // Try move down by 1
    if (tryMove(1, 0)) {
        return true; // moved down
    }

    // Cannot move down => lock piece and spawn a new one
    lockActiveTetrominoAndProcessLines();

    if (board_.isGameOver()) {
        status_ = GameStatus::GameOver;
        return false;
    }

    if (!spawnNewTetromino()) {
        status_ = GameStatus::GameOver;
        return false;
    }

    return false; // piece didn't move this tick (it locked)
}

void GameState::moveLeft() {
    if (status_ != GameStatus::Running || !activeTetromino_) return;
    tryMove(0, -1);
}

void GameState::moveRight() {
    if (status_ != GameStatus::Running || !activeTetromino_) return;
    tryMove(0, 1);
}

void GameState::softDrop() {
    if (status_ != GameStatus::Running || !activeTetromino_) return;
    tryMove(1, 0);
}

void GameState::hardDrop() {
    if (status_ != GameStatus::Running || !activeTetromino_) return;
    if (!activeTetromino_) return;

    // Drop until we can’t move further
    while (tryMove(1, 0)) {
        // keep dropping
    }

    lockActiveTetrominoAndProcessLines();

    if (board_.isGameOver() || !spawnNewTetromino()) {
        status_ = GameStatus::GameOver;
    }
}

void GameState::rotateClockwise() {
    if (status_ != GameStatus::Running || !activeTetromino_) return;
    tryRotate(true);
}

void GameState::rotateCounterClockwise() {
    if (status_ != GameStatus::Running || !activeTetromino_) return;
    tryRotate(false);
}

bool GameState::spawnNewTetromino() {
    const int rows = board_.rows();
    const int cols = board_.cols();

    // Spawn origin roughly in the middle at row 0 or 1
    Position spawn{0, cols / 2};

    if (!nextTetromino_) {
        nextTetromino_ = factory_.createRandom(spawn);
    }

    activeTetromino_ = nextTetromino_;
    activeTetromino_->setOrigin(spawn);

    nextTetromino_ = factory_.createRandom(spawn); // origin will be reset on spawn

    if (!board_.canPlace(*activeTetromino_)) {
        // Cannot spawn -> game over
        activeTetromino_.reset();
        return false;
    }
    return true;
}

void GameState::lockActiveTetrominoAndProcessLines() {
    if (!activeTetromino_) return;

    board_.lockTetromino(*activeTetromino_);
    activeTetromino_.reset();

    int lines = board_.clearFullLines();
    if (lines > 0) {
        scoreManager_.addLinesCleared(lines, levelManager_.level());
        levelManager_.onLinesCleared(lines);
    }
}

bool GameState::tryMove(int dRow, int dCol) {
    if (!activeTetromino_) return false;

    Tetromino moved = *activeTetromino_;
    Position origin = moved.origin();
    origin.row += dRow;
    origin.col += dCol;
    moved.setOrigin(origin);

    if (board_.canPlace(moved)) {
        activeTetromino_ = moved;
        return true;
    }
    return false;
}

bool GameState::tryRotate(bool clockwise) {
    if (!activeTetromino_) return false;

    Tetromino rotated = *activeTetromino_;
    if (clockwise) {
        rotated.rotateClockwise();
    } else {
        rotated.rotateCounterClockwise();
    }

    if (board_.canPlace(rotated)) {
        activeTetromino_ = rotated;
        return true;
    }

    // Here we could implement wall kicks; for now, simple "no rotate if collides"
    return false;
}

} // namespace tetris::core
