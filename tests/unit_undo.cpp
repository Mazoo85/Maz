// Unit tests for maz::core::UndoStack — the command-pattern undo/redo history.
// Exercises push executing redo immediately, execute=false recording without
// applying, sequential undo/redo restoring captured prior values, pushing after
// an undo truncating the orphaned redo branch, canUndo/canRedo/undoCount/
// redoCount bookkeeping, clear/empty, and a capacity bound dropping the oldest
// undo step. Pure C++, no GPU/display.

#include "maz/core/UndoStack.hpp"

#include <cstdio>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- SINGLE COMMAND: execute + undo + redo --------------------------------
    {
        int x = 0;
        UndoStack s;
        s.push([&]{ x = 1; }, [&]{ x = 0; });  // executes -> x == 1
        check(x == 1, "push executes redo immediately");
        check(s.canUndo() == true, "canUndo true after push");
        check(s.canRedo() == false, "canRedo false after push");
        check(s.undoCount() == 1, "undoCount == 1 after push");
        check(s.redoCount() == 0, "redoCount == 0 after push");

        check(s.undo() == true, "undo returns true");
        check(x == 0, "undo reverts to 0");
        check(s.canUndo() == false, "canUndo false after undo to bottom");
        check(s.canRedo() == true, "canRedo true after undo");

        check(s.redo() == true, "redo returns true");
        check(x == 1, "redo re-applies to 1");

        check(s.undo() == true, "undo again returns true");
        check(x == 0, "x back to 0");
        check(s.undo() == false, "undo past bottom returns false");
        check(x == 0, "x unchanged after failed undo");
    }

    // --- EXECUTE=FALSE RECORDS WITHOUT APPLYING -------------------------------
    {
        int x = 5;
        UndoStack s;
        s.push([&]{ x = 1; }, [&]{ x = 5; }, /*execute*/false);  // does NOT run redo
        check(x == 5, "execute=false does not apply redo");
        check(s.undoCount() == 1, "execute=false still records (undoCount 1)");
        check(s.canUndo() == true, "execute=false records an undoable command");
        check(s.undo() == true, "undo of recorded-not-applied returns true");
        check(x == 5, "undo sets x to 5 (already 5)");
        check(s.redo() == true, "redo returns true");
        check(x == 1, "redo now applies -> x == 1");
    }

    // --- SEQUENTIAL COMMANDS (captured prior values) --------------------------
    {
        int x = 0;
        UndoStack s;
        int prev1 = x; s.push([&]{ x = 1; }, [&, prev1]{ x = prev1; });  // 0 -> 1
        int prev2 = x; s.push([&]{ x = 2; }, [&, prev2]{ x = prev2; });  // 1 -> 2
        int prev3 = x; s.push([&]{ x = 3; }, [&, prev3]{ x = prev3; });  // 2 -> 3
        check(x == 3, "after three pushes x == 3");
        check(s.undoCount() == 3, "undoCount == 3");

        check(s.undo() && x == 2, "undo -> x == 2");
        check(s.undo() && x == 1, "undo -> x == 1");
        check(s.undo() && x == 0, "undo -> x == 0");
        check(s.canUndo() == false, "canUndo false at bottom");

        check(s.redo() && x == 1, "redo -> x == 1");
        check(s.redo() && x == 2, "redo -> x == 2");
        check(s.redo() && x == 3, "redo -> x == 3");
        check(s.canRedo() == false, "canRedo false at top");
    }

    // --- BRANCH TRUNCATES REDO ------------------------------------------------
    {
        int x = 0;
        UndoStack s;
        int prev1 = x; s.push([&]{ x = 1; }, [&, prev1]{ x = prev1; });  // 0 -> 1
        int prev2 = x; s.push([&]{ x = 2; }, [&, prev2]{ x = prev2; });  // 1 -> 2
        check(x == 2, "x == 2 after two pushes");
        check(s.undoCount() == 2, "undoCount == 2");

        check(s.undo() && x == 1, "undo -> x == 1");
        check(s.canRedo() == true, "canRedo true after undo");
        check(s.redoCount() == 1, "redoCount == 1 after undo");

        // Push a new command from the x==1 state — discards the x->2 redo branch.
        int prevB = x; s.push([&]{ x = 9; }, [&, prevB]{ x = prevB; });
        check(x == 9, "new push applies -> x == 9");
        check(s.canRedo() == false, "redo branch discarded: canRedo false");
        check(s.redoCount() == 0, "redoCount == 0 after branch truncation");
        check(s.undoCount() == 2, "undoCount == 2 (x->1 and x->9)");
        check(s.redo() == false, "redo after truncation returns false");

        check(s.undo() && x == 1, "undo reverts x->9 -> x == 1");
        check(s.undo() && x == 0, "undo reverts x->1 -> x == 0");
    }

    // --- COUNTS + canUndo/canRedo TRACKING ------------------------------------
    {
        int x = 0;
        UndoStack s;
        check(s.empty() == true, "fresh stack empty");
        check(s.canUndo() == false && s.canRedo() == false, "fresh stack neither undo nor redo");

        int prev1 = x; s.push([&]{ x = 1; }, [&, prev1]{ x = prev1; });
        int prev2 = x; s.push([&]{ x = 2; }, [&, prev2]{ x = prev2; });
        check(s.undoCount() == 2 && s.redoCount() == 0, "two applied: undo 2 / redo 0");
        s.undo();
        check(s.undoCount() == 1 && s.redoCount() == 1, "after undo: undo 1 / redo 1");
        check(s.canUndo() && s.canRedo(), "after undo: both canUndo and canRedo");
        s.redo();
        check(s.undoCount() == 2 && s.redoCount() == 0, "after redo: undo 2 / redo 0");
    }

    // --- CLEAR / EMPTY --------------------------------------------------------
    {
        int x = 0;
        UndoStack s;
        int prev1 = x; s.push([&]{ x = 1; }, [&, prev1]{ x = prev1; });
        int prev2 = x; s.push([&]{ x = 2; }, [&, prev2]{ x = prev2; });
        s.clear();
        check(s.empty() == true, "clear -> empty");
        check(s.undoCount() == 0, "clear -> undoCount 0");
        check(s.redoCount() == 0, "clear -> redoCount 0");
        check(s.canUndo() == false, "clear -> canUndo false");
        check(s.canRedo() == false, "clear -> canRedo false");
        check(s.undo() == false, "clear -> undo returns false");
    }

    // --- CAPACITY DROPS OLDEST ------------------------------------------------
    {
        UndoStack s(2);
        check(s.capacity() == 2, "capacity() == 2");
        int x = 0;
        int prev1 = x; s.push([&]{ x = 1; }, [&, prev1]{ x = prev1; });  // 0 -> 1
        int prev2 = x; s.push([&]{ x = 2; }, [&, prev2]{ x = prev2; });  // 1 -> 2
        check(s.undoCount() == 2, "at capacity: undoCount == 2");

        int prev3 = x; s.push([&]{ x = 3; }, [&, prev3]{ x = prev3; });  // 2 -> 3
        check(x == 3, "over-cap push applies -> x == 3");
        check(s.undoCount() == 2, "over-cap push keeps undoCount capped at 2");
        check(s.canUndo() == true, "canUndo true after capacity push");

        // Exactly two undos succeed (holding x->2 and x->3), then the third fails
        // because the oldest command (x->1) and its undo step were dropped.
        check(s.undo() == true, "first undo succeeds");
        check(s.undo() == true, "second undo succeeds");
        check(s.undo() == false, "third undo fails (oldest step forgotten)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
