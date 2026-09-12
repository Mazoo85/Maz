#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::game turn-order scheduler — the initiative queue behind tactics and JRPG combat, where each
// combatant acts in order of a speed / initiative score, highest first, and play loops round after round.
// Combatants are added with an initiative value; `start` opens round 1 by sorting them (ties broken by id
// so the order is deterministic), `current` names whose turn it is, and `advance` steps to the next
// combatant, rolling into a fresh round (re-sorting to pick up any initiative changes) once everyone has
// acted. Combatants can be removed mid-battle (a defeated enemy is skipped immediately, passing the turn
// on) and added between rounds (a summon joins the next round). Godot ships no turn/initiative system —
// games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only,
// deterministic.
namespace maz::game {

class TurnOrder {
public:
    TurnOrder() = default;

    // Register a combatant. A duplicate id updates its initiative (takes effect next round). Higher
    // initiative acts earlier; ties break toward the lower id.
    void addCombatant(int id, double initiative) {
        for (Combatant& c : m_combatants) {
            if (c.id == id) {
                c.initiative = initiative;
                return;
            }
        }
        m_combatants.push_back(Combatant{id, initiative});
    }

    // Same as addCombatant for an existing id — set a new initiative (applies at the next round).
    void setInitiative(int id, double initiative) { addCombatant(id, initiative); }

    // Remove a combatant. If it is the one currently acting, the turn passes to the next combatant; if it
    // is later in this round, it is skipped. Returns true if it was present.
    bool removeCombatant(int id) {
        bool removed = false;
        for (std::size_t i = 0; i < m_combatants.size(); ++i) {
            if (m_combatants[i].id == id) {
                m_combatants.erase(m_combatants.begin() + static_cast<long>(i));
                removed = true;
                break;
            }
        }
        if (!removed) return false;
        for (std::size_t i = 0; i < m_order.size(); ++i) {
            if (m_order[i] == id) {
                m_order.erase(m_order.begin() + static_cast<long>(i));
                if (i < m_cursor) --m_cursor; // keep the cursor on the same live combatant
                break;
            }
        }
        return true;
    }

    // Begin combat at round 1 (sorts the initiative order and points at the first combatant).
    void start() {
        m_round = 1;
        rebuildOrder();
        m_cursor = 0;
    }

    int round() const { return m_round; }

    // Id of the combatant whose turn it is, or -1 if none (empty or not started).
    int current() const {
        if (m_cursor >= m_order.size()) return -1;
        return m_order[m_cursor];
    }

    // Advance to the next combatant, rolling into a new round (re-sorted) after the last one. Returns the
    // new current id, or -1 if there are no combatants.
    int advance() {
        if (m_order.empty()) {
            if (m_combatants.empty()) return -1;
            rebuildOrder(); // combatants added before any start()
        }
        ++m_cursor;
        if (m_cursor >= m_order.size()) {
            ++m_round;
            rebuildOrder();
            m_cursor = 0;
        }
        return current();
    }

    std::size_t combatantCount() const { return m_combatants.size(); }
    bool has(int id) const { return find(id) != nullptr; }
    double initiativeOf(int id) const {
        const Combatant* c = find(id);
        return c == nullptr ? 0.0 : c->initiative;
    }

    // The sorted ids for the current round (highest initiative first).
    const std::vector<int>& order() const { return m_order; }

    void clear() {
        m_combatants.clear();
        m_order.clear();
        m_cursor = 0;
        m_round = 0;
    }

private:
    struct Combatant {
        int id = -1;
        double initiative = 0.0;
    };

    const Combatant* find(int id) const {
        for (const Combatant& c : m_combatants)
            if (c.id == id) return &c;
        return nullptr;
    }

    void rebuildOrder() {
        m_order.clear();
        m_order.reserve(m_combatants.size());
        for (const Combatant& c : m_combatants) m_order.push_back(c.id);
        std::sort(m_order.begin(), m_order.end(), [this](int a, int b) {
            const double ia = initiativeOf(a);
            const double ib = initiativeOf(b);
            if (ia != ib) return ia > ib; // higher initiative first
            return a < b;                  // deterministic tie-break
        });
    }

    std::vector<Combatant> m_combatants;
    std::vector<int> m_order;
    std::size_t m_cursor = 0;
    int m_round = 0;
};

} // namespace maz::game
