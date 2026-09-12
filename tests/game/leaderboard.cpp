// tests/game/leaderboard.cpp — verifies the leaderboard (game Leaderboard.hpp).
// Ground truths, deterministic:
//   * top() returns entries best-first;
//   * competition ranking: tied scores share a rank and the next distinct score skips ("1224");
//   * only a player's BEST score is kept (a worse resubmit is ignored, a better one promotes);
//   * around() returns the requested window centred on a player;
//   * a lower-is-better board (race times) ranks the smallest score first.
#include "maz/game/Leaderboard.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::Leaderboard;
using maz::game::LeaderboardEntry;

int main() {
    // --- 1. Ordering + competition ranks with a tie. ---
    {
        Leaderboard lb(true); // higher is better
        lb.submit("alice", 100);
        lb.submit("bob", 100);   // tie with alice
        lb.submit("carol", 90);
        lb.submit("dave", 80);
        const std::vector<LeaderboardEntry> t = lb.top(10);
        CHECK(t.size() == 4, "top lists all four players");
        CHECK(t[0].score == 100 && t[1].score == 100, "the two 100s come first");
        CHECK(t[2].id == "carol" && t[3].id == "dave", "then carol (90) then dave (80)");
        // Competition ranking: alice & bob rank 1; carol rank 3 (skips 2); dave rank 4.
        CHECK(lb.rank("alice") == 1 && lb.rank("bob") == 1, "tied top scores share rank 1");
        CHECK(lb.rank("carol") == 3, "the score after a two-way tie is rank 3 (not 2)");
        CHECK(lb.rank("dave") == 4, "and the next is rank 4");
        CHECK(lb.rank("nobody") == 0, "an absent player has rank 0");
    }

    // --- 2. Best score kept. ---
    {
        Leaderboard lb(true);
        lb.submit("p", 50);
        CHECK(!lb.submit("p", 40), "a worse resubmit is ignored");
        CHECK(lb.scoreOf("p") == 50, "the better score is retained");
        CHECK(lb.submit("p", 70), "a better resubmit updates");
        CHECK(lb.scoreOf("p") == 70, "the new best is stored");
    }

    // --- 3. around() window. ---
    {
        Leaderboard lb(true);
        lb.submit("a", 500);
        lb.submit("b", 400);
        lb.submit("c", 300);
        lb.submit("d", 200);
        lb.submit("e", 100);
        const std::vector<LeaderboardEntry> w = lb.around("c", 1, 1); // c and one either side
        CHECK(w.size() == 3, "around returns the centred window");
        CHECK(w[0].id == "b" && w[1].id == "c" && w[2].id == "d", "window is b, c, d");
        // Clamp at the top.
        const std::vector<LeaderboardEntry> top = lb.around("a", 2, 1);
        CHECK(top.size() == 2 && top[0].id == "a" && top[1].id == "b", "window clamps at the leader");
    }

    // --- 4. Lower-is-better (race times). ---
    {
        Leaderboard lb(false); // smaller time ranks higher
        lb.submit("fast", 12.5);
        lb.submit("mid", 15.0);
        lb.submit("slow", 20.0);
        const std::vector<LeaderboardEntry> t = lb.top(3);
        CHECK(t[0].id == "fast" && t[2].id == "slow", "the smallest time is ranked first");
        CHECK(lb.rank("fast") == 1 && lb.rank("slow") == 3, "ranks follow ascending scores");
        CHECK(!lb.submit("fast", 13.0), "a slower time does not replace the best");
        CHECK(lb.submit("fast", 11.0), "a faster time does");
    }

    if (g_fail == 0) {
        std::printf("leaderboard: OK — ordering, competition ranks, best-kept, around window, "
                    "lower-is-better.\n");
        return 0;
    }
    std::printf("leaderboard: %d failure(s).\n", g_fail);
    return 1;
}
