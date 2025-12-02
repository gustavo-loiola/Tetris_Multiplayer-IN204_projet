#include "core/MatchRules.hpp"

#include <limits>
#include <algorithm>

namespace tetris::core {

using tetris::net::MatchOutcome;
using tetris::net::MatchResult;
using tetris::net::Tick;
using tetris::net::PlayerId;

SharedTurnRules::SharedTurnRules(std::uint32_t piecesPerTurn)
    : m_piecesPerTurn(piecesPerTurn == 0 ? 1u : piecesPerTurn)
{
}

void SharedTurnRules::initializePlayers(const std::vector<PlayerSnapshot>& players)
{
    m_order.clear();
    m_order.reserve(players.size());
    for (const auto& p : players) {
        m_order.push_back(p.id);
    }

    m_currentIndex = 0;
    m_currentCount = 0;
    m_finished     = false;

    if (!m_order.empty()) {
        m_currentPlayerId = m_order[0];
    } else {
        m_currentPlayerId = 0;
    }
}

void SharedTurnRules::onMatchStart(Tick startTick)
{
    m_startTick     = startTick;
    m_endTick       = 0;
    m_finished      = false;
    m_cachedResults.clear();
    m_currentCount  = 0;
}

int SharedTurnRules::countAlive(const std::vector<PlayerSnapshot>& players)
{
    int alive = 0;
    for (const auto& p : players) {
        if (p.isAlive) ++alive;
    }
    return alive;
}

const PlayerSnapshot* SharedTurnRules::findPlayer(
    const std::vector<PlayerSnapshot>& players,
    PlayerId id
) const {
    for (const auto& p : players) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void SharedTurnRules::rotateToNextAlive(const std::vector<PlayerSnapshot>& players)
{
    if (m_order.empty()) {
        m_currentPlayerId = 0;
        return;
    }

    const int alive = countAlive(players);
    if (alive <= 1) {
        // Rotation no longer meaningful; match should be ending.
        return;
    }

    const std::size_t n = m_order.size();
    for (std::size_t i = 0; i < n; ++i) {
        m_currentIndex = (m_currentIndex + 1) % n;
        PlayerId candidate = m_order[m_currentIndex];
        const auto* snap = findPlayer(players, candidate);
        if (snap && snap->isAlive) {
            m_currentPlayerId = candidate;
            return;
        }
    }

    // No alive player found; let update() handle winner selection.
}

void SharedTurnRules::onPieceLocked(PlayerId currentPlayerId,
                                    const std::vector<PlayerSnapshot>& players)
{
    if (m_finished) return;

    // Safety: sync with the caller's idea of current player.
    if (currentPlayerId != 0) {
        m_currentPlayerId = currentPlayerId;
    }

    // Check how many players are alive.
    const int alive = countAlive(players);
    if (alive <= 1) {
        m_finished = true;
        return;
    }

    // Count this piece towards the current player's turn.
    ++m_currentCount;
    if (m_currentCount >= m_piecesPerTurn) {
        m_currentCount = 0;
        rotateToNextAlive(players);
    }
}

void SharedTurnRules::computeResults(const std::vector<PlayerSnapshot>& players)
{
    m_cachedResults.clear();
    m_cachedResults.reserve(players.size());

    if (players.empty()) return;

    int alive = countAlive(players);

    // Decide winners:
    // - If at least one alive: only alive players can win/draw.
    // - If none alive: use highest score across all.
    int maxScore = std::numeric_limits<int>::min();

    if (alive > 0) {
        for (const auto& p : players) {
            if (!p.isAlive) continue;
            if (p.score > maxScore) maxScore = p.score;
        }
    } else {
        for (const auto& p : players) {
            if (p.score > maxScore) maxScore = p.score;
        }
    }

    int winnersCount = 0;
    if (alive > 0) {
        for (const auto& p : players) {
            if (p.isAlive && p.score == maxScore) ++winnersCount;
        }
    } else {
        for (const auto& p : players) {
            if (p.score == maxScore) ++winnersCount;
        }
    }

    const bool isDraw = (winnersCount > 1);

    for (const auto& p : players) {
        bool isCandidate =
            (alive > 0 ? (p.isAlive && p.score == maxScore)
                       : (p.score == maxScore));

        MatchOutcome outcome;
        if (isCandidate && isDraw) {
            outcome = MatchOutcome::Draw;
        } else if (isCandidate && !isDraw) {
            outcome = MatchOutcome::Win;
        } else {
            outcome = MatchOutcome::Lose;
        }

        m_cachedResults.push_back(MatchResult{
            m_endTick,
            p.id,
            outcome,
            p.score
        });
    }
}

std::vector<MatchResult>
SharedTurnRules::update(Tick currentTick,
                        const std::vector<PlayerSnapshot>& players)
{
    if (!m_finished) {
        return {};
    }

    if (m_endTick == 0) {
        m_endTick = currentTick;
    }

    if (m_cachedResults.empty()) {
        computeResults(players);
    }

    return m_cachedResults;
}

} // namespace tetris::core
