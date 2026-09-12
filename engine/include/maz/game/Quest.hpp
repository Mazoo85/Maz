#pragma once

#include <cstddef>
#include <vector>

// maz::game quest / objective tracker — the journal behind "kill 5 goblins", "collect 3 keys", and the
// "Quest Complete!" banner. A quest is a set of counted objectives; each objective needs a target count and
// tracks progress toward it, and a quest auto-completes when every objective is met. The canonical driver
// is `advance(objectiveId, amount)`: the game reports an event once ("a goblin died -> objective 5 +1") and
// every active quest with a matching objective moves forward, with quests that finish reported back so the
// UI can celebrate. Quests carry a state (Inactive -> Active -> Completed / Failed) and the log answers
// which are active or completed for a journal screen. Godot ships no quest system — games hand-roll it
// every time — so this is a beyond-Godot gameplay utility extending the RPG suite. Header-only, std-only,
// deterministic.
namespace maz::game {

enum class QuestState { Inactive, Active, Completed, Failed };

struct Objective {
    int id = -1;      // game-defined objective id (e.g. "kill goblins")
    int required = 1; // target count to satisfy this objective
    int current = 0;  // progress so far

    bool complete() const { return current >= required; }
};

struct Quest {
    int id = -1;
    QuestState state = QuestState::Inactive;
    std::vector<Objective> objectives;

    // True when every objective is met (vacuously true for a quest with no objectives).
    bool objectivesMet() const {
        for (const Objective& o : objectives)
            if (!o.complete()) return false;
        return true;
    }
};

class QuestLog {
public:
    QuestLog() = default;

    // Register a quest (Inactive). A duplicate id is ignored (returns false).
    bool addQuest(int questId, std::vector<Objective> objectives) {
        if (find(questId) != nullptr) return false;
        Quest q;
        q.id = questId;
        q.state = QuestState::Inactive;
        q.objectives = std::move(objectives);
        m_quests.push_back(std::move(q));
        return true;
    }

    // Move an Inactive quest to Active. Returns true if it started (auto-completing at once if its
    // objectives are already satisfied). False for an unknown or non-Inactive quest.
    bool startQuest(int questId) {
        Quest* q = find(questId);
        if (q == nullptr || q->state != QuestState::Inactive) return false;
        q->state = QuestState::Active;
        if (q->objectivesMet()) q->state = QuestState::Completed;
        return true;
    }

    // Report progress toward objective `objectiveId` across every ACTIVE quest. Matching objectives advance
    // by `amount` (clamped at their target). Returns how many quests transitioned to Completed on this call.
    int advance(int objectiveId, int amount = 1) {
        if (amount <= 0) return 0;
        int completed = 0;
        for (Quest& q : m_quests) {
            if (q.state != QuestState::Active) continue;
            bool touched = false;
            for (Objective& o : q.objectives) {
                if (o.id == objectiveId && o.current < o.required) {
                    o.current += amount;
                    if (o.current > o.required) o.current = o.required;
                    touched = true;
                }
            }
            if (touched && q.objectivesMet()) {
                q.state = QuestState::Completed;
                ++completed;
            }
        }
        return completed;
    }

    // Advance one objective within a single Active quest. Returns true if THIS quest became Completed.
    bool addProgress(int questId, int objectiveId, int amount = 1) {
        Quest* q = find(questId);
        if (q == nullptr || q->state != QuestState::Active || amount <= 0) return false;
        bool touched = false;
        for (Objective& o : q->objectives) {
            if (o.id == objectiveId && o.current < o.required) {
                o.current += amount;
                if (o.current > o.required) o.current = o.required;
                touched = true;
            }
        }
        if (touched && q->objectivesMet()) {
            q->state = QuestState::Completed;
            return true;
        }
        return false;
    }

    // Mark an Active quest Failed. Returns true if it was Active.
    bool failQuest(int questId) {
        Quest* q = find(questId);
        if (q == nullptr || q->state != QuestState::Active) return false;
        q->state = QuestState::Failed;
        return true;
    }

    QuestState state(int questId) const {
        const Quest* q = find(questId);
        return q == nullptr ? QuestState::Inactive : q->state;
    }
    bool isActive(int questId) const { return state(questId) == QuestState::Active; }
    bool isComplete(int questId) const { return state(questId) == QuestState::Completed; }
    bool isFailed(int questId) const { return state(questId) == QuestState::Failed; }

    // current/required for one objective, or {0,0} if the quest or objective is unknown.
    Objective objective(int questId, int objectiveId) const {
        const Quest* q = find(questId);
        if (q != nullptr)
            for (const Objective& o : q->objectives)
                if (o.id == objectiveId) return o;
        return Objective{-1, 0, 0};
    }

    // Fraction [0,1] of the quest's objectives that are complete (1.0 for a Completed or objective-less
    // quest, 0 for an unknown quest).
    float progress(int questId) const {
        const Quest* q = find(questId);
        if (q == nullptr) return 0.0f;
        if (q->objectives.empty() || q->state == QuestState::Completed) return 1.0f;
        int done = 0;
        for (const Objective& o : q->objectives)
            if (o.complete()) ++done;
        return static_cast<float>(done) / static_cast<float>(q->objectives.size());
    }

    std::size_t questCount() const { return m_quests.size(); }
    const std::vector<Quest>& quests() const { return m_quests; }

    std::vector<int> activeQuests() const { return idsInState(QuestState::Active); }
    std::vector<int> completedQuests() const { return idsInState(QuestState::Completed); }

    void clear() { m_quests.clear(); }

private:
    Quest* find(int id) {
        for (Quest& q : m_quests)
            if (q.id == id) return &q;
        return nullptr;
    }
    const Quest* find(int id) const {
        for (const Quest& q : m_quests)
            if (q.id == id) return &q;
        return nullptr;
    }
    std::vector<int> idsInState(QuestState s) const {
        std::vector<int> out;
        for (const Quest& q : m_quests)
            if (q.state == s) out.push_back(q.id);
        return out;
    }

    std::vector<Quest> m_quests;
};

} // namespace maz::game
