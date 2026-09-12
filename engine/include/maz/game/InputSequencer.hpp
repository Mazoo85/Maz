#pragma once

#include <cstddef>
#include <deque>
#include <vector>

// maz::game input sequencer — the fighting-game "motion input" / special-move detector: register moves as
// ordered sequences of input tokens (the game encodes directions and buttons however it likes — e.g.
// down, down-forward, forward, punch for a quarter-circle fireball), feed the player's inputs as they
// happen, and it fires the move id the instant the recent inputs end-match a registered sequence WITHIN
// that move's timing window. This is the "execute the combo before the window closes" mechanic of Street
// Fighter, Tekken, Smash, and every beat-'em-up.
//
// This is deliberately distinct from `game::ComboMeter`, which counts a STREAK of hits for a score
// multiplier — it never looks at input order or motion. Here the ORDER and TIMING of raw inputs is the
// whole point. Detection prefers the LONGEST matching sequence (so a 4-input super beats the 3-input
// special sharing its prefix-tail), the match is consumed so the same inputs cannot re-trigger, and stale
// inputs age out of the buffer so an old direction cannot complete a motion minutes later. Time is driven
// by `tick(dt)` (no wall clock), so detection is fully deterministic. Godot ships no motion-input matcher.
// Header-only, std-only.
namespace maz::game {

class InputSequencer {
public:
    // Register a move as an ordered token sequence that must complete within `window` seconds (measured
    // from the first to the last token of the match). Returns the move id (>= 0), or -1 for an empty
    // sequence or a non-positive window.
    int registerMove(const std::vector<int>& sequence, float window) {
        if (sequence.empty() || window <= 0.0f) return -1;
        m_moves.push_back(Move{sequence, window});
        if (window > m_maxWindow) m_maxWindow = window;
        return static_cast<int>(m_moves.size()) - 1;
    }

    // Advance the internal clock and expire inputs older than the longest registered window.
    void tick(float dt) {
        if (dt <= 0.0f) return;
        m_now += static_cast<double>(dt);
        prune();
    }

    // Record one input token at the current time and test for a completed move. Returns the id of the
    // matched move (longest wins; lowest id on a length tie), or -1 if nothing fired. On a match the input
    // buffer is consumed so the same inputs will not fire it again.
    int feed(int token) {
        prune();
        m_buf.push_back(Entry{token, m_now});

        int best = -1;
        std::size_t bestLen = 0;
        for (std::size_t i = 0; i < m_moves.size(); ++i) {
            const Move& mv = m_moves[i];
            const std::size_t k = mv.seq.size();
            if (m_buf.size() < k) continue;
            const std::size_t base = m_buf.size() - k;
            bool ok = true;
            for (std::size_t j = 0; j < k; ++j) {
                if (m_buf[base + j].token != mv.seq[j]) { ok = false; break; }
            }
            if (!ok) continue;
            const double span = m_now - m_buf[base].time; // first matched token to now
            if (span > static_cast<double>(mv.window)) continue;
            if (k > bestLen) { best = static_cast<int>(i); bestLen = k; } // strictly-longer wins; ties keep lower id
        }
        if (best >= 0) m_buf.clear(); // consume the inputs that fired the move
        return best;
    }

    void reset() { m_buf.clear(); }

    std::size_t moveCount() const { return m_moves.size(); }
    std::size_t bufferSize() const { return m_buf.size(); }
    double now() const { return m_now; }

private:
    struct Entry {
        int token;
        double time;
    };
    struct Move {
        std::vector<int> seq;
        float window;
    };

    void prune() {
        // Drop inputs older than the longest window (they can no longer complete any move) and cap memory.
        while (!m_buf.empty() && (m_now - m_buf.front().time) > static_cast<double>(m_maxWindow)) {
            m_buf.pop_front();
        }
        while (m_buf.size() > kMaxBuffer) m_buf.pop_front();
    }

    static constexpr std::size_t kMaxBuffer = 64;

    std::deque<Entry> m_buf;
    std::vector<Move> m_moves;
    double m_now = 0.0;
    float m_maxWindow = 0.0f;
};

} // namespace maz::game
