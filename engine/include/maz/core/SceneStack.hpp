#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace maz::core {

// The application-framework layer: a stack of game "scenes" (menu, gameplay, pause, game-over) with
// a proper lifecycle. Pushing a scene pauses the one below and enters the new one; popping exits it
// and resumes the one revealed. Scenes can be transparent overlays (a pause menu drawn over the
// frozen game) via blocksUpdate()/blocksRender(). This is what ties menus and gameplay into a real
// game instead of a single-screen demo. render() is a hook (default no-op) so the core stays
// renderer-agnostic and unit-tests headless; the app overrides it. Stack mutations requested during
// update() are DEFERRED and applied afterwards, so a scene can safely pop or replace itself.
class Scene {
public:
    virtual ~Scene() = default;

    virtual void onEnter() {}   // became active (pushed, or replaced in)
    virtual void onExit() {}    // being destroyed (popped / replaced / cleared)
    virtual void onPause() {}   // another scene was pushed on top of this one
    virtual void onResume() {}  // the scene above this one was popped

    virtual void update(float dt) { (void)dt; }
    virtual void render() {}

    // If true, scenes below this one on the stack do NOT update (a modal scene freezes the game).
    virtual bool blocksUpdate() const { return true; }
    // If true, this scene is opaque and scenes below it are NOT rendered (false = transparent overlay).
    virtual bool blocksRender() const { return true; }
};

using ScenePtr = std::unique_ptr<Scene>;

class SceneStack {
public:
    // Push a new scene on top (pausing the current top). Deferred if called during update().
    void push(ScenePtr scene) {
        if (m_iterating) {
            m_pending.push_back({Op::Push, std::move(scene)});
        } else {
            doPush(std::move(scene));
        }
    }
    // Pop the top scene (resuming the one below). Deferred if called during update().
    void pop() {
        if (m_iterating) {
            m_pending.push_back({Op::Pop, nullptr});
        } else {
            doPop();
        }
    }
    // Replace the top scene with a new one (exits the old, enters the new; no pause/resume).
    void replace(ScenePtr scene) {
        if (m_iterating) {
            m_pending.push_back({Op::Replace, std::move(scene)});
        } else {
            doReplace(std::move(scene));
        }
    }
    // Remove every scene (each gets onExit, top-down). Deferred if called during update().
    void clear() {
        if (m_iterating) {
            m_pending.push_back({Op::Clear, nullptr});
        } else {
            doClear();
        }
    }

    // Update scenes from the top down, stopping after the first that blocksUpdate(). Any push/pop/
    // replace/clear requested by a scene during its update is applied after the pass.
    void update(float dt) {
        m_iterating = true;
        for (size_t i = m_scenes.size(); i-- > 0;) {
            m_scenes[i]->update(dt);
            if (m_scenes[i]->blocksUpdate()) {
                break;
            }
        }
        m_iterating = false;
        applyPending();
    }

    // Render the visible scenes bottom-up: everything from the topmost opaque scene up to the top,
    // so a transparent overlay draws over the scene(s) beneath it.
    void render() {
        if (m_scenes.empty()) {
            return;
        }
        size_t base = m_scenes.size() - 1;
        while (base > 0 && !m_scenes[base]->blocksRender()) {
            --base;
        }
        for (size_t i = base; i < m_scenes.size(); ++i) {
            m_scenes[i]->render();
        }
    }

    Scene* top() const { return m_scenes.empty() ? nullptr : m_scenes.back().get(); }
    size_t size() const { return m_scenes.size(); }
    bool empty() const { return m_scenes.empty(); }

private:
    enum class Op { Push, Pop, Replace, Clear };
    struct Cmd {
        Op op;
        ScenePtr scene;
    };

    void doPush(ScenePtr scene) {
        if (!m_scenes.empty()) {
            m_scenes.back()->onPause();
        }
        m_scenes.push_back(std::move(scene));
        m_scenes.back()->onEnter();
    }
    void doPop() {
        if (m_scenes.empty()) {
            return;
        }
        m_scenes.back()->onExit();
        m_scenes.pop_back();
        if (!m_scenes.empty()) {
            m_scenes.back()->onResume();
        }
    }
    void doReplace(ScenePtr scene) {
        if (!m_scenes.empty()) {
            m_scenes.back()->onExit();
            m_scenes.pop_back();
        }
        m_scenes.push_back(std::move(scene));
        m_scenes.back()->onEnter();
    }
    void doClear() {
        while (!m_scenes.empty()) {
            m_scenes.back()->onExit();
            m_scenes.pop_back();
        }
    }
    void applyPending() {
        // Take the queue (a command may itself queue more; process until drained).
        while (!m_pending.empty()) {
            std::vector<Cmd> batch;
            batch.swap(m_pending);
            for (Cmd& c : batch) {
                switch (c.op) {
                    case Op::Push: doPush(std::move(c.scene)); break;
                    case Op::Pop: doPop(); break;
                    case Op::Replace: doReplace(std::move(c.scene)); break;
                    case Op::Clear: doClear(); break;
                }
            }
        }
    }

    std::vector<ScenePtr> m_scenes;
    std::vector<Cmd> m_pending;
    bool m_iterating = false;
};

} // namespace maz::core
