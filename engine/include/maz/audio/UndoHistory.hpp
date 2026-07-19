#pragma once

#include <string>
#include <vector>

namespace maz::audio {

// A bounded undo/redo history of project snapshots. Snapshots are opaque strings — in practice the
// output of saveProjectToString() — so this class stays decoupled from the project model. The current
// state lives outside the history; the caller supplies it on undo/redo.
//
// Usage: call push(current) BEFORE applying an edit (recording the pre-edit state). To undo, pass the
// now-current state and restore whatever undo() returns; redo() is the mirror. A new push() clears the
// redo stack, so editing after an undo forks the history (standard behaviour).
class UndoHistory {
public:
    explicit UndoHistory(size_t maxDepth = 64) : maxDepth_(maxDepth == 0 ? 1 : maxDepth) {}

    // Record the state as it was before an edit. Drops the oldest entry past the depth cap, and
    // clears the redo stack (a fresh edit forks history).
    void push(const std::string& snapshot) {
        undo_.push_back(snapshot);
        if (undo_.size() > maxDepth_) {
            undo_.erase(undo_.begin());
        }
        redo_.clear();
    }

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    // Undo: stash the current state for redo and return the previous state to restore. Returns false
    // (leaving `out` untouched) when there is nothing to undo.
    bool undo(const std::string& current, std::string& out) {
        if (undo_.empty()) {
            return false;
        }
        redo_.push_back(current);
        out = undo_.back();
        undo_.pop_back();
        return true;
    }

    // Redo: stash the current state for undo and return the next state to restore. Returns false
    // (leaving `out` untouched) when there is nothing to redo.
    bool redo(const std::string& current, std::string& out) {
        if (redo_.empty()) {
            return false;
        }
        undo_.push_back(current);
        out = redo_.back();
        redo_.pop_back();
        return true;
    }

    void clear() {
        undo_.clear();
        redo_.clear();
    }
    size_t undoDepth() const { return undo_.size(); }
    size_t redoDepth() const { return redo_.size(); }

private:
    size_t maxDepth_;
    std::vector<std::string> undo_;
    std::vector<std::string> redo_;
};

} // namespace maz::audio
