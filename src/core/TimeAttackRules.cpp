#include "core/MatchRules.hpp"

#include <limits>
#include <algorithm>

namespace tetris::core {

using tetris::net::MatchOutcome;
using tetris::net::MatchResult;
using tetris::net::Tick;

TimeAttackRules::TimeAttackRules(Tick timeLimitTicks)
    : m_timeLimitTicks(timeLimitTicks)
{
}

void TimeAttackRules::initializePlayers(const std::vector<PlayerSnapshot>& /*players*/)
{
    // TimeAttack rules don't depend on players list up front.
}

void TimeAttackRules::onMatchStart(Tick startTick)
{
    m_startTick     = startTick;
    m_endTick       = 0;
    m_finished      = false;
    m_cachedResults.clear();
}

void TimeAttackRules::onPieceLocked(tetris::net::PlayerId /*currentPlayerId*/,
                                    const std::vector<PlayerSnapshot>& /*players*/)
{
    // TimeAttack is purely time-based; ignore piece events.
}

std::vector<MatchResult>
TimeAttackRules::update(Tick currentTick,
                        const std::vector<PlayerSnapshot>& players)
{
    if (m_finished) {
        return m_cachedResults; // idempotent after finish
    }

    // Not reached the time limit yet?
    if (currentTick < m_startTick + m_timeLimitTicks) {
        return {}; // match still running
    }

    m_finished = true;
    m_endTick  = currentTick;

    m_cachedResults.clear();
    m_cachedResults.reserve(players.size());

    if (players.empty()) {
        // No players: edge case, nothing to do.
        return m_cachedResults;
    }

    // Find highest score among *alive* players.
    int maxScore = std::numeric_limits<int>::min();
    for (const auto& p : players) {
        if (!p.isAlive) continue;
        if (p.score > maxScore) {
            maxScore = p.score;
        }
    }

    // It’s possible all players are dead; then we still look at scores.
    if (maxScore == std::numeric_limits<int>::min()) {
        maxScore = 0;
    }

    // Count how many players have the top score.
    int winnersCount = 0;
    for (const auto& p : players) {
        if (p.score == maxScore) {
            ++winnersCount;
        }
    }

    const bool isDraw = (winnersCount > 1);

    for (const auto& p : players) {
        MatchOutcome outcome;
        if (isDraw && p.score == maxScore) {
            outcome = MatchOutcome::Draw;
        } else if (!isDraw && p.score == maxScore) {
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

    return m_cachedResults;
}

} // namespace tetris::core
