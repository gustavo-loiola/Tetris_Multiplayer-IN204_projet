#pragma once

#include <cstdint>
#include <vector>

#include "network/MessageTypes.hpp"

namespace tetris::core {

// Minimal info the rules need about each player.
struct PlayerSnapshot {
    tetris::net::PlayerId id{};
    int score{};        // current score
    bool isAlive{true}; // true if the player is still playing (not topped out)
};

// Strategy interface for different multiplayer modes.
class IMatchRules {
public:
    virtual ~IMatchRules() = default;

    // Called once before the match starts to provide initial player info
    // and order (typically lobby order).
    virtual void initializePlayers(const std::vector<PlayerSnapshot>& players) = 0;

    // Called when the match actually starts (host decides startTick).
    virtual void onMatchStart(tetris::net::Tick startTick) = 0;

    // Called whenever the current piece is locked on the shared board
    // (for SharedTurns); TimeAttack can ignore it.
    virtual void onPieceLocked(tetris::net::PlayerId currentPlayerId,
                               const std::vector<PlayerSnapshot>& players) = 0;

    // Host calls this regularly (e.g., once per server tick).
    // Returns per-player MatchResult when the match ends, or empty vector if running.
    virtual std::vector<tetris::net::MatchResult>
    update(tetris::net::Tick currentTick,
           const std::vector<PlayerSnapshot>& players) = 0;

    virtual bool isFinished() const = 0;

    virtual tetris::net::GameMode mode() const = 0;
};

// Time-based competitive rules:
// - Match ends after a fixed tick duration.
// - Winner: highest score among alive players.
// - Equal top scores => all are Draw.
class TimeAttackRules : public IMatchRules {
public:
    // timeLimitTicks: how many ticks after start the match ends.
    // (Host decides what a "tick" means in terms of real time.)
    explicit TimeAttackRules(tetris::net::Tick timeLimitTicks);

    void initializePlayers(const std::vector<PlayerSnapshot>& players) override;
    void onMatchStart(tetris::net::Tick startTick) override;
    void onPieceLocked(tetris::net::PlayerId currentPlayerId,
                       const std::vector<PlayerSnapshot>& players) override;
    std::vector<tetris::net::MatchResult>
    update(tetris::net::Tick currentTick,
           const std::vector<PlayerSnapshot>& players) override;

    bool isFinished() const override { return m_finished; }

    tetris::net::GameMode mode() const override {
        return tetris::net::GameMode::TimeAttack;
    }

private:
    tetris::net::Tick m_timeLimitTicks;
    tetris::net::Tick m_startTick{0};
    tetris::net::Tick m_endTick{0};
    bool m_finished{false};
    std::vector<tetris::net::MatchResult> m_cachedResults;
};

// =============================
// SharedTurnRules
// =============================
// Shared board, players alternate controlling pieces.
// - `piecesPerTurn` controls how many locked pieces before rotating.
// - Match ends when only one player remains alive (or zero, in which case
// the highest score wins / draws).
class SharedTurnRules : public IMatchRules {
public:
    explicit SharedTurnRules(std::uint32_t piecesPerTurn);

    void initializePlayers(const std::vector<PlayerSnapshot>& players) override;
    void onMatchStart(tetris::net::Tick startTick) override;
    void onPieceLocked(tetris::net::PlayerId currentPlayerId,
                       const std::vector<PlayerSnapshot>& players) override;
    std::vector<tetris::net::MatchResult>
    update(tetris::net::Tick currentTick,
           const std::vector<PlayerSnapshot>& players) override;

    bool isFinished() const override { return m_finished; }

    tetris::net::GameMode mode() const override {
        return tetris::net::GameMode::SharedTurns;
    }

    // Not part of the IMatchRules interface on purpose
    // for the host to know who should control the next piece.
    tetris::net::PlayerId currentPlayer() const { return m_currentPlayerId; }

private:
    std::uint32_t m_piecesPerTurn;

    std::vector<tetris::net::PlayerId> m_order;
    std::size_t m_currentIndex{0};
    std::uint32_t m_currentCount{0};

    tetris::net::PlayerId m_currentPlayerId{0};

    bool m_finished{false};
    tetris::net::Tick m_startTick{0};
    tetris::net::Tick m_endTick{0};
    std::vector<tetris::net::MatchResult> m_cachedResults;

    static int countAlive(const std::vector<PlayerSnapshot>& players);
    const PlayerSnapshot* findPlayer(const std::vector<PlayerSnapshot>& players,
                                     tetris::net::PlayerId id) const;
    void rotateToNextAlive(const std::vector<PlayerSnapshot>& players);
    void computeResults(const std::vector<PlayerSnapshot>& players);
};

} // namespace tetris::core
