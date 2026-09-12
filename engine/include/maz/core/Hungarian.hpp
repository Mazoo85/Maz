#pragma once

#include <cstddef>
#include <limits>
#include <vector>

// maz::core::hungarian — the Hungarian algorithm (Kuhn-Munkres) for the optimal ASSIGNMENT problem: given a
// cost matrix of `rows` agents against `cols` tasks (rows <= cols), match every agent to a DISTINCT task so
// the total cost is the minimum possible, in O(n^3). This is a genuinely different problem from the engine's
// pathfinding (AStar2D finds one least-cost route; this optimally pairs a WHOLE SET at once) and from a
// greedy nearest-assignment (which is fast but routinely non-optimal). The canonical uses: assign N attack
// units to N targets to minimise total travel, N defenders to N incoming threats, N workers to N jobs, or
// any "who does what" that must be globally best rather than locally greedy. Uses the classic potentials +
// augmenting-path formulation (Dijkstra-like, O(n^3)); deterministic, header-only, std-only. To MAXIMISE a
// score instead, negate the values before calling. Godot ships no assignment solver.
namespace maz::core {

struct Assignment {
    std::vector<int> taskForAgent; // taskForAgent[i] = task assigned to agent i (always valid when rows<=cols)
    double totalCost = 0.0;
};

// Solve the assignment for a row-major cost matrix `cost` of size rows*cols, with rows <= cols.
// Returns, for each agent (row), the task (column) it is assigned, plus the minimum total cost.
// If rows > cols the problem has no perfect agent-matching; the result is empty.
inline Assignment hungarian(const std::vector<double>& cost, std::size_t rows, std::size_t cols) {
    Assignment out;
    if (rows == 0 || cols == 0 || rows > cols || cost.size() != rows * cols) {
        return out;
    }

    const int n = static_cast<int>(rows);
    const int m = static_cast<int>(cols);
    const double INF = std::numeric_limits<double>::max() / 4.0;

    // 1-based potentials and matching arrays (e-maxx formulation).
    std::vector<double> u(static_cast<std::size_t>(n) + 1, 0.0);
    std::vector<double> v(static_cast<std::size_t>(m) + 1, 0.0);
    std::vector<int> p(static_cast<std::size_t>(m) + 1, 0); // p[j] = agent matched to task j (0 = none)
    std::vector<int> way(static_cast<std::size_t>(m) + 1, 0);

    auto at = [&](int i1, int j1) -> double {
        return cost[static_cast<std::size_t>(i1 - 1) * cols + static_cast<std::size_t>(j1 - 1)];
    };

    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0; // current unmatched task column (0 sentinel)
        std::vector<double> minv(static_cast<std::size_t>(m) + 1, INF);
        std::vector<char> used(static_cast<std::size_t>(m) + 1, 0);
        do {
            used[static_cast<std::size_t>(j0)] = 1;
            const int i0 = p[static_cast<std::size_t>(j0)];
            double delta = INF;
            int j1 = -1;
            for (int j = 1; j <= m; ++j) {
                if (used[static_cast<std::size_t>(j)]) {
                    continue;
                }
                const double cur = at(i0, j) - u[static_cast<std::size_t>(i0)] - v[static_cast<std::size_t>(j)];
                if (cur < minv[static_cast<std::size_t>(j)]) {
                    minv[static_cast<std::size_t>(j)] = cur;
                    way[static_cast<std::size_t>(j)] = j0;
                }
                if (minv[static_cast<std::size_t>(j)] < delta) {
                    delta = minv[static_cast<std::size_t>(j)];
                    j1 = j;
                }
            }
            for (int j = 0; j <= m; ++j) {
                if (used[static_cast<std::size_t>(j)]) {
                    u[static_cast<std::size_t>(p[static_cast<std::size_t>(j)])] += delta;
                    v[static_cast<std::size_t>(j)] -= delta;
                } else {
                    minv[static_cast<std::size_t>(j)] -= delta;
                }
            }
            j0 = j1;
        } while (p[static_cast<std::size_t>(j0)] != 0);
        // Augment along the recorded path.
        do {
            const int j1 = way[static_cast<std::size_t>(j0)];
            p[static_cast<std::size_t>(j0)] = p[static_cast<std::size_t>(j1)];
            j0 = j1;
        } while (j0 != 0);
    }

    out.taskForAgent.assign(static_cast<std::size_t>(n), -1);
    for (int j = 1; j <= m; ++j) {
        const int agent = p[static_cast<std::size_t>(j)];
        if (agent >= 1 && agent <= n) {
            out.taskForAgent[static_cast<std::size_t>(agent - 1)] = j - 1;
        }
    }
    double total = 0.0;
    for (int i = 0; i < n; ++i) {
        const int j = out.taskForAgent[static_cast<std::size_t>(i)];
        total += cost[static_cast<std::size_t>(i) * cols + static_cast<std::size_t>(j)];
    }
    out.totalCost = total;
    return out;
}

} // namespace maz::core
