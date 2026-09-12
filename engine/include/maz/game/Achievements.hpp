#pragma once

#include <cstddef>
#include <vector>

// maz::game achievement system — the unlock tracker behind "Achievement Unlocked!" toasts and the
// completion percentage on a save file. Each achievement has a target count (a simple one-shot trophy uses
// target 1; a grind like "defeat 100 enemies" uses target 100); the game reports progress and the system
// fires an unlock the moment the target is reached, reporting it back exactly once so a popup shows a
// single time. Progress clamps at the target, unlocking is one-way (until an explicit reset), and the
// registry answers per-achievement progress and an overall completion fraction for a stats screen. Godot
// ships no achievement system (games talk to Steam/console SDKs or hand-roll one) -> beyond-Godot gameplay
// utility. Header-only, std-only, deterministic.
namespace maz::game {

class Achievements {
public:
    Achievements() = default;

    // Register an achievement needing `target` progress to unlock (clamped to >= 1). A duplicate id resets
    // it to locked with the new target.
    void addAchievement(int id, int target = 1) {
        const int t = target < 1 ? 1 : target;
        Achievement* a = find(id);
        if (a != nullptr) {
            a->target = t;
            a->progress = 0;
            a->unlocked = false;
            return;
        }
        m_achievements.push_back(Achievement{id, t, 0, false});
    }

    // Report `amount` progress toward `id`. Returns true only on the call that crosses the target (the
    // moment to show the toast). No effect once unlocked, on a non-positive amount, or an unknown id.
    bool progress(int id, int amount = 1) {
        Achievement* a = find(id);
        if (a == nullptr || a->unlocked || amount <= 0) return false;
        a->progress += amount;
        if (a->progress >= a->target) {
            a->progress = a->target;
            a->unlocked = true;
            return true;
        }
        return false;
    }

    // Unlock `id` outright (fills its progress). Returns true if it was newly unlocked.
    bool unlock(int id) {
        Achievement* a = find(id);
        if (a == nullptr || a->unlocked) return false;
        a->progress = a->target;
        a->unlocked = true;
        return true;
    }

    // Re-lock an achievement (progress back to 0). Returns true if it existed.
    bool reset(int id) {
        Achievement* a = find(id);
        if (a == nullptr) return false;
        a->progress = 0;
        a->unlocked = false;
        return true;
    }

    bool has(int id) const { return find(id) != nullptr; }
    bool isUnlocked(int id) const {
        const Achievement* a = find(id);
        return a != nullptr && a->unlocked;
    }
    int progressOf(int id) const {
        const Achievement* a = find(id);
        return a == nullptr ? 0 : a->progress;
    }
    int targetOf(int id) const {
        const Achievement* a = find(id);
        return a == nullptr ? 0 : a->target;
    }

    // Fraction [0,1] of the way to unlocking (1.0 once unlocked, 0 for an unknown id).
    float fraction(int id) const {
        const Achievement* a = find(id);
        if (a == nullptr) return 0.0f;
        if (a->unlocked || a->target <= 0) return 1.0f;
        return static_cast<float>(a->progress) / static_cast<float>(a->target);
    }

    std::size_t totalCount() const { return m_achievements.size(); }
    std::size_t unlockedCount() const {
        std::size_t n = 0;
        for (const Achievement& a : m_achievements)
            if (a.unlocked) ++n;
        return n;
    }

    // Overall completion [0,1]: unlocked / total (0 when there are no achievements).
    float completion() const {
        if (m_achievements.empty()) return 0.0f;
        return static_cast<float>(unlockedCount()) / static_cast<float>(m_achievements.size());
    }

    std::vector<int> unlockedIds() const {
        std::vector<int> out;
        for (const Achievement& a : m_achievements)
            if (a.unlocked) out.push_back(a.id);
        return out;
    }

    void clear() { m_achievements.clear(); }

private:
    struct Achievement {
        int id = -1;
        int target = 1;
        int progress = 0;
        bool unlocked = false;
    };

    Achievement* find(int id) {
        for (Achievement& a : m_achievements)
            if (a.id == id) return &a;
        return nullptr;
    }
    const Achievement* find(int id) const {
        for (const Achievement& a : m_achievements)
            if (a.id == id) return &a;
        return nullptr;
    }

    std::vector<Achievement> m_achievements;
};

} // namespace maz::game
