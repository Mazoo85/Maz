#pragma once

#include <string>
#include <utility>
#include <vector>

namespace maz::input {

// Action mapping: the layer that turns raw device state into named gameplay intents. Gameplay code
// asks "is Jump pressed?" or "what's MoveX?" instead of "is Space down / is pad A down / is the left
// stick past the deadzone?" — so one action can bind several physical sources (keyboard OR gamepad),
// bindings can be rebound at runtime, and the game logic never mentions a scancode.
//
// Two action kinds:
//   * Button actions are down if ANY bound source is down; each frame yields held / pressed (edge
//     down this frame) / released (edge up this frame).
//   * Axis actions combine negative/positive button pairs (each contributing -1 / +1) with any analog
//     pad axes (scaled), clamped to [-1, 1] — so WASD and a thumbstick drive the same MoveX.
//
// The map is deliberately SDL-free: update() takes sampler callbacks (down(device, code) and
// analog(axis)), so it is unit-testable with synthetic input and works over any backend. An app wires
// the samplers to platform::Input. Header-only, std only.

enum class Device { Key, MouseButton, PadButton };

class ActionMap {
public:
    // --- binding ----------------------------------------------------------------------------------
    // Bind another physical source to a button action (creates the action if new).
    void bindButton(const std::string& action, Device device, int code) {
        buttonAction(action).sources.push_back(Source{device, code});
    }
    // Bind a negative/positive button pair to an axis action (e.g., A/D -> MoveX).
    void bindAxisPair(const std::string& action, Device negDevice, int negCode, Device posDevice,
                      int posCode) {
        axisAction(action).pairs.push_back(Pair{{negDevice, negCode}, {posDevice, posCode}});
    }
    // Bind an analog pad axis to an axis action; scale can invert (e.g., -1 for an inverted Y).
    void bindAxisAnalog(const std::string& action, int padAxis, float scale = 1.0f) {
        axisAction(action).analogs.push_back(Analog{padAxis, scale});
    }

    // --- per-frame update -------------------------------------------------------------------------
    // down(device, code) -> bool ; analog(padAxis) -> float. Call once per frame before querying.
    template <class DownFn, class AnalogFn>
    void update(DownFn&& down, AnalogFn&& analog) {
        for (Button& b : m_buttons) {
            b.prevHeld = b.held;
            bool any = false;
            for (const Source& s : b.sources) {
                if (down(s.device, s.code)) {
                    any = true;
                    break;
                }
            }
            b.held = any;
        }
        for (Axis& a : m_axes) {
            float v = 0.0f;
            for (const Pair& p : a.pairs) {
                if (down(p.pos.device, p.pos.code)) v += 1.0f;
                if (down(p.neg.device, p.neg.code)) v -= 1.0f;
            }
            for (const Analog& an : a.analogs) v += analog(an.padAxis) * an.scale;
            a.value = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
        }
    }

    // Button-only convenience (no analog axes).
    template <class DownFn>
    void update(DownFn&& down) {
        update(std::forward<DownFn>(down), [](int) { return 0.0f; });
    }

    // --- queries ----------------------------------------------------------------------------------
    bool held(const std::string& action) const {
        const Button* b = findButton(action);
        return b && b->held;
    }
    bool pressed(const std::string& action) const {
        const Button* b = findButton(action);
        return b && b->held && !b->prevHeld;
    }
    bool released(const std::string& action) const {
        const Button* b = findButton(action);
        return b && !b->held && b->prevHeld;
    }
    float axis(const std::string& action) const {
        const Axis* a = findAxis(action);
        return a ? a->value : 0.0f;
    }
    bool hasButton(const std::string& action) const { return findButton(action) != nullptr; }
    bool hasAxis(const std::string& action) const { return findAxis(action) != nullptr; }

private:
    struct Source {
        Device device;
        int code;
    };
    struct Pair {
        Source neg, pos;
    };
    struct Analog {
        int padAxis;
        float scale;
    };
    struct Button {
        std::string name;
        std::vector<Source> sources;
        bool held = false;
        bool prevHeld = false;
    };
    struct Axis {
        std::string name;
        std::vector<Pair> pairs;
        std::vector<Analog> analogs;
        float value = 0.0f;
    };

    std::vector<Button> m_buttons;
    std::vector<Axis> m_axes;

    Button& buttonAction(const std::string& name) {
        for (Button& b : m_buttons)
            if (b.name == name) return b;
        m_buttons.push_back(Button{name, {}, false, false});
        return m_buttons.back();
    }
    Axis& axisAction(const std::string& name) {
        for (Axis& a : m_axes)
            if (a.name == name) return a;
        m_axes.push_back(Axis{name, {}, {}, 0.0f});
        return m_axes.back();
    }
    const Button* findButton(const std::string& name) const {
        for (const Button& b : m_buttons)
            if (b.name == name) return &b;
        return nullptr;
    }
    const Axis* findAxis(const std::string& name) const {
        for (const Axis& a : m_axes)
            if (a.name == name) return &a;
        return nullptr;
    }
};

} // namespace maz::input
