#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// maz::game leaderboard — a ranked score table with competition ranking and top-N / around-me windows.
//
// Every game with high scores, ranked ladders, speedrun times, or weekly challenges needs the same
// thing: submit a player's score, and answer "what rank am I?", "show the top 10", and "show me and my
// neighbours". This does it with proper COMPETITION ranking (tied scores share a rank; the next distinct
// score skips ahead — the "1224" convention), keeps only each player's BEST score, and supports both
// higher-is-better (points) and lower-is-better (race/lap times) boards. Pure value logic, header-only,
// deterministic — unit-tested for ordering, tie ranks, best-score-kept updates, top-N, around-window,
// and ascending (time) boards.
namespace maz::game {

struct LeaderboardEntry {
    std::string id;
    double score = 0.0;
};

class Leaderboard {
public:
    explicit Leaderboard(bool higherIsBetter = true) : higherIsBetter_(higherIsBetter) {}

    // Submit a score for `id`. Keeps the player's BEST score (higher or lower per the board's mode); a
    // worse score is ignored. Returns true if the stored score changed.
    bool submit(const std::string& id, double score) {
        const auto it = scores_.find(id);
        if (it == scores_.end()) {
            scores_.emplace(id, score);
            dirty_ = true;
            return true;
        }
        if (better(score, it->second)) {
            it->second = score;
            dirty_ = true;
            return true;
        }
        return false;
    }

    bool contains(const std::string& id) const { return scores_.find(id) != scores_.end(); }
    std::size_t size() const { return scores_.size(); }

    // Stored (best) score for `id`, or 0 if absent (check contains() to disambiguate).
    double scoreOf(const std::string& id) const {
        const auto it = scores_.find(id);
        return it == scores_.end() ? 0.0 : it->second;
    }

    // 1-based competition rank of `id` (ties share a rank), or 0 if absent.
    int rank(const std::string& id) const {
        const auto it = scores_.find(id);
        if (it == scores_.end()) return 0;
        const double mine = it->second;
        int better = 0;
        for (const auto& kv : scores_) {
            if (strictlyBetter(kv.second, mine)) ++better;
        }
        return better + 1;
    }

    // The best `n` entries, best-first.
    std::vector<LeaderboardEntry> top(int n) const {
        rebuild();
        std::vector<LeaderboardEntry> out;
        const int count = n < 0 ? 0 : (n > static_cast<int>(sorted_.size()) ? static_cast<int>(sorted_.size()) : n);
        out.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) out.push_back(sorted_[static_cast<std::size_t>(i)]);
        return out;
    }

    // A window around `id`: up to `before` entries above and `after` below (inclusive of `id`).
    // Empty if `id` is absent.
    std::vector<LeaderboardEntry> around(const std::string& id, int before, int after) const {
        rebuild();
        std::vector<LeaderboardEntry> out;
        int pos = -1;
        for (std::size_t i = 0; i < sorted_.size(); ++i) {
            if (sorted_[i].id == id) {
                pos = static_cast<int>(i);
                break;
            }
        }
        if (pos < 0) return out;
        int lo = pos - (before < 0 ? 0 : before);
        int hi = pos + (after < 0 ? 0 : after);
        if (lo < 0) lo = 0;
        if (hi >= static_cast<int>(sorted_.size())) hi = static_cast<int>(sorted_.size()) - 1;
        for (int i = lo; i <= hi; ++i) out.push_back(sorted_[static_cast<std::size_t>(i)]);
        return out;
    }

    void clear() {
        scores_.clear();
        sorted_.clear();
        dirty_ = false;
    }

private:
    bool better(double a, double b) const { return higherIsBetter_ ? a > b : a < b; }
    bool strictlyBetter(double a, double b) const { return higherIsBetter_ ? a > b : a < b; }

    void rebuild() const {
        if (!dirty_) return;
        sorted_.clear();
        sorted_.reserve(scores_.size());
        for (const auto& kv : scores_) sorted_.push_back(LeaderboardEntry{kv.first, kv.second});
        const bool hib = higherIsBetter_;
        std::sort(sorted_.begin(), sorted_.end(), [hib](const LeaderboardEntry& x, const LeaderboardEntry& y) {
            if (x.score != y.score) return hib ? x.score > y.score : x.score < y.score;
            return x.id < y.id; // deterministic tie-break for display order (does not affect rank)
        });
        dirty_ = false;
    }

    bool higherIsBetter_ = true;
    std::unordered_map<std::string, double> scores_;
    mutable std::vector<LeaderboardEntry> sorted_;
    mutable bool dirty_ = false;
};

} // namespace maz::game
