#pragma once

#include <functional>
#include <vector>
#include <cstddef>
#include <utility>  // std::move

#include "maz/core/Assert.hpp"

namespace maz::core {

// A command-pattern undo/redo history (the Godot UndoRedo analog) — push(redo,
// undo) records a reversible command and, by default, applies its redo action
// immediately; undo() reverts the most-recently-applied command and redo()
// re-applies the next undone one. Pushing a new command after undoing discards
// the now-orphaned redo branch (standard undo-stack behavior). An optional
// capacity bounds the history, forgetting the OLDEST undo step when exceeded.
// canUndo/canRedo/undoCount/redoCount/clear round it out. Composes std::function,
// which holds a capturing lambda, a free function, or a maz::core::Delegate.
// Do NOT push/undo/redo from inside a command's action
// (re-entrancy is unsupported). NOT thread-safe. Action merging/coalescing and
// named transactions are future refinements (not built here).
class UndoStack {
  public:
    using Action = std::function<void()>;

    // Constructs an undo stack with an optional history bound. A capacity of 0
    // means unbounded.
    explicit UndoStack(std::size_t capacity = 0) : m_capacity(capacity) {}

    // Records a reversible command. redo/undo must both be valid. If execute is
    // true (the default), the redo action is applied NOW; pass false when the
    // caller already performed the change and only wants it recorded. Pushing
    // after some undos truncates the redoable branch. When the history exceeds
    // capacity the oldest command is dropped (its undo step is forgotten).
    void push(Action redo, Action undo, bool execute = true) {
        MAZ_ASSERT(redo != nullptr && undo != nullptr, "UndoStack::push: null action");
        // A new command supersedes any undone (redoable) tail — discard it.
        if (m_cursor < m_commands.size()) {
            m_commands.resize(m_cursor);
        }
        m_commands.push_back(Command{ std::move(redo), std::move(undo) });
        if (execute) {
            m_commands.back().redo();
        }
        m_cursor = m_commands.size();
        // Exceeding capacity forgets the oldest undo step (over by at most 1).
        if (m_capacity > 0 && m_commands.size() > m_capacity) {
            m_commands.erase(m_commands.begin());
            --m_cursor;
        }
    }

    // Reverts the most-recently-applied command. Returns false if nothing is
    // applied.
    bool undo() {
        if (m_cursor == 0) {
            return false;
        }
        m_commands[m_cursor - 1].undo();
        --m_cursor;
        return true;
    }

    // Re-applies the next undone command. Returns false if nothing is redoable.
    bool redo() {
        if (m_cursor >= m_commands.size()) {
            return false;
        }
        m_commands[m_cursor].redo();
        ++m_cursor;
        return true;
    }

    bool canUndo() const { return m_cursor > 0; }
    bool canRedo() const { return m_cursor < m_commands.size(); }

    std::size_t undoCount() const { return m_cursor; }
    std::size_t redoCount() const { return m_commands.size() - m_cursor; }
    std::size_t capacity() const { return m_capacity; }

    void clear() {
        m_commands.clear();
        m_cursor = 0;
    }

    bool empty() const { return m_commands.empty(); }

  private:
    struct Command {
        Action redo;
        Action undo;
    };

    std::vector<Command> m_commands;
    std::size_t m_cursor = 0;   // commands[0..cursor) applied, [cursor..size) undone
    std::size_t m_capacity;     // 0 == unbounded
};

} // namespace maz::core
