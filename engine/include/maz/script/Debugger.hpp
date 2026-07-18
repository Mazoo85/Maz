#pragma once

#include "maz/script/Script.hpp"

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// maz::script source-level debugger — the CPU core of a script debugger (Godot's ScriptDebugger /
// remote debug protocol). It drives the tree-walking VM through its per-statement onStep hook and
// adds what a real debugger needs: line breakpoints, the four stepping modes (into / over / out /
// continue) resolved from call-stack depth, a call-stack snapshot at each stop, and variable
// inspection of the paused frame. Execution pauses *synchronously*: when a stop condition is hit the
// VM calls your onPause handler on the same stack, you inspect state and return the next step mode,
// and execution resumes — exactly how an embedded debug hook works. Pure CPU + deterministic, so it
// unit-tests headlessly. Wiring this to a remote IDE over a socket is the [DESK] transport layer on
// top; the decision logic and inspection here are complete and testable.
namespace maz::script {

enum class StepMode {
    Continue,  // run until the next breakpoint
    StepInto,  // stop at the very next statement, descending into calls
    StepOver,  // stop at the next statement in this frame or a shallower one (skip called bodies)
    StepOut,   // stop only once execution returns to a shallower frame
};

struct PauseEvent {
    enum class Reason { Breakpoint, Step };
    Reason reason = Reason::Step;
    int line = 0;
    std::string function;               // innermost function name ("<main>" at top level)
    std::vector<std::string> callStack; // outermost first: ["<main>", ...], innermost last
    int depth = 0;                      // call depth (0 == <main>)
};

class Debugger {
  public:
    explicit Debugger(Vm& vm) : m_vm(&vm) {
        m_vm->onStep = [this](int line, const std::string& fn) { onStatement(line, fn); };
    }

    // --- breakpoints ------------------------------------------------------------------------------
    void addBreakpoint(int line) { m_breakpoints.insert(line); }
    void removeBreakpoint(int line) { m_breakpoints.erase(line); }
    void clearBreakpoints() { m_breakpoints.clear(); }
    bool hasBreakpoint(int line) const { return m_breakpoints.count(line) != 0; }
    std::vector<int> breakpoints() const { return {m_breakpoints.begin(), m_breakpoints.end()}; }

    // Stop at the first statement of the next run (like launching under a debugger with break-on-entry).
    void breakAtEntry() {
        m_mode = StepMode::StepInto;
        m_anchorDepth = 0;
    }
    void setMode(StepMode m) { m_mode = m; }

    // Invoked synchronously at each pause. Return the step mode to resume with. If unset, the debugger
    // simply continues (stops only at breakpoints).
    std::function<StepMode(Debugger&, const PauseEvent&)> onPause;

    // --- inspection (valid only during an onPause callback) ---------------------------------------
    // Variables bound in the paused frame's scope chain (locals; globals excluded).
    std::vector<std::pair<std::string, Value>> locals() const { return m_vm->debugLocals(); }
    // Resolve any name visible at the pause point (locals then globals), or nullptr.
    const Value* resolve(const std::string& name) const { return m_vm->debugResolve(name); }
    // Convenience: the string form of a visible variable, or "<undefined>" if not in scope.
    std::string valueString(const std::string& name) const {
        const Value* v = resolve(name);
        return v ? v->toString() : std::string("<undefined>");
    }
    const std::vector<std::string>& callStack() const { return m_callStack; }
    int depth() const { return m_depth; }

    // --- stats ------------------------------------------------------------------------------------
    int pauseCount() const { return m_pauseCount; }

  private:
    void onStatement(int line, const std::string& fn) {
        const int depth = m_vm->callDepth();

        bool stop = false;
        PauseEvent::Reason reason = PauseEvent::Reason::Step;
        if (m_breakpoints.count(line) != 0) {
            stop = true;
            reason = PauseEvent::Reason::Breakpoint;
        } else {
            switch (m_mode) {
            case StepMode::Continue:
                stop = false;
                break;
            case StepMode::StepInto:
                stop = true;
                break;
            case StepMode::StepOver:
                stop = depth <= m_anchorDepth; // same frame or returned to a shallower one
                break;
            case StepMode::StepOut:
                stop = depth < m_anchorDepth; // only once we've returned to a shallower frame
                break;
            }
        }
        if (!stop) {
            return;
        }

        ++m_pauseCount;
        m_depth = depth;
        m_callStack.clear();
        m_callStack.push_back("<main>");
        for (const std::string& f : m_vm->callStackSnapshot()) {
            m_callStack.push_back(f);
        }

        PauseEvent ev;
        ev.reason = reason;
        ev.line = line;
        ev.function = fn;
        ev.callStack = m_callStack;
        ev.depth = depth;

        const StepMode next = onPause ? onPause(*this, ev) : StepMode::Continue;
        m_mode = next;
        m_anchorDepth = depth; // future steps are relative to where we just paused
    }

    Vm* m_vm;
    std::set<int> m_breakpoints;
    StepMode m_mode = StepMode::Continue;
    int m_anchorDepth = 0;
    int m_depth = 0;
    int m_pauseCount = 0;
    std::vector<std::string> m_callStack;
};

} // namespace maz::script
