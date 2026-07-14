#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <algorithm>  // std::clamp

#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// An action-mapping layer — bind an abstract action (a maz::core::StringId such as
// "Jump") to one or more physical inputs so gameplay reads intent, not hardware
// (the Godot InputMap analog). An action can OR-combine several digital buttons
// (any held -> the action is "down") and/or accumulate axis contributions, each of
// which is either a digital pair (negative key = -1, positive key = +1) or an
// analog input scaled by a factor; contributions are summed then clamped to
// [-1, 1].
//
// update(Sampler) is templated on a caller-supplied device rather than depending on
// maz::platform::Input directly: the Sampler must provide `bool down(InputId) const`
// and `float axis(InputId) const`. Keeping the layer decoupled from
// maz::platform::Input (whose implementation is off the core path) lets it build and
// test under MAZ_CORE_ONLY; a thin platform::Input -> Sampler adapter is a future
// refinement (not built here).
//
// Semantics: edge detection stores prev = cur BEFORE recomputing cur each frame, so
// isPressed()/isReleased() see one-frame rising/falling edges; because cur is a
// level (any button held), isPressed does NOT re-fire while the action stays active,
// even if additional bound buttons are pressed. Queries on an unknown action return
// false / 0 and never throw. NOT thread-safe. Action contexts / layers, analog
// deadzone / response curves, chord (multi-input) bindings, and binding
// serialization are future refinements (not built here).

namespace maz::input {

// Which device a physical input belongs to.
enum class DeviceType : std::uint8_t { Keyboard, Mouse, Gamepad };

// Device-neutral physical input id. `code` is a plain int (SDL scancode / mouse
// button / gamepad button) so gameplay code need not include SDL — mirroring the
// convention in maz::platform::Input.
struct InputId {
    DeviceType   device = DeviceType::Keyboard;
    std::int32_t code   = 0;
    constexpr bool operator==(const InputId& o) const { return device == o.device && code == o.code; }
};

class ActionMap {
  public:
    // Adds a digital button binding to `action` (created on first bind). Multiple
    // buttons OR-combine: the action is down if ANY bound button is held.
    void bindButton(maz::core::StringId action, InputId input) {
        MAZ_ASSERT(action.valid(), "ActionMap::bindButton: invalid action id");
        m_actions[action].buttons.push_back(input);
    }

    // Adds a digital axis contribution: `positive` held contributes +1, `negative`
    // held contributes -1 (both held cancel to 0).
    void bindAxis(maz::core::StringId action, InputId negative, InputId positive) {
        MAZ_ASSERT(action.valid(), "ActionMap::bindAxis: invalid action id");
        AxisContribution a;
        a.analog   = false;
        a.negative = negative;
        a.positive = positive;
        m_actions[action].axes.push_back(a);
    }

    // Adds an analog axis contribution: `analog` sampled via Sampler::axis, times `scale`.
    void bindAnalogAxis(maz::core::StringId action, InputId analog, float scale = 1.0f) {
        MAZ_ASSERT(action.valid(), "ActionMap::bindAnalogAxis: invalid action id");
        AxisContribution a;
        a.analog      = true;
        a.analogInput = analog;
        a.scale       = scale;
        m_actions[action].axes.push_back(a);
    }

    // Recomputes cur/prev/axisValue for every action from the sampled device.
    // Call once per frame, after device state is updated and before querying.
    template <class Sampler>
    void update(const Sampler& s) {
        for (auto& [id, st] : m_actions) {
            (void)id;
            // MARQUEE CORRECTNESS POINT: snapshot last frame's level BEFORE recomputing
            // this frame's, so isPressed/isReleased detect one-frame rising/falling edges.
            st.prev = st.cur;

            st.cur = false;
            for (const InputId& b : st.buttons) {
                if (s.down(b)) { st.cur = true; break; }  // OR over buttons
            }

            float v = 0.0f;
            for (const AxisContribution& a : st.axes) {
                if (a.analog) {
                    v += s.axis(a.analogInput) * a.scale;
                } else {
                    v += (s.down(a.positive) ? 1.0f : 0.0f) - (s.down(a.negative) ? 1.0f : 0.0f);
                }
            }
            st.axisValue = std::clamp(v, -1.0f, 1.0f);  // summed, then clamped to [-1, 1]
        }
    }

    // True while the action is active this frame (any bound button held).
    bool isDown(maz::core::StringId action) const {
        auto it = m_actions.find(action);
        return it != m_actions.end() ? it->second.cur : false;
    }

    // True on the frame the action becomes active (rising edge); no re-fire while held.
    bool isPressed(maz::core::StringId action) const {
        auto it = m_actions.find(action);
        return it != m_actions.end() ? (it->second.cur && !it->second.prev) : false;
    }

    // True on the frame the action becomes inactive (falling edge).
    bool isReleased(maz::core::StringId action) const {
        auto it = m_actions.find(action);
        return it != m_actions.end() ? (!it->second.cur && it->second.prev) : false;
    }

    // Combined, clamped axis value in [-1, 1] for the action (0 if unknown).
    float axis(maz::core::StringId action) const {
        auto it = m_actions.find(action);
        return it != m_actions.end() ? it->second.axisValue : 0.0f;
    }

    bool hasAction(maz::core::StringId action) const {
        return m_actions.find(action) != m_actions.end();
    }

    // Drops all bindings + state for a single action.
    void clearBindings(maz::core::StringId action) { m_actions.erase(action); }

    // Drops every action.
    void clear() { m_actions.clear(); }

    std::size_t actionCount() const { return m_actions.size(); }

  private:
    struct AxisContribution {
        bool    analog = false;        // false => digital pair (negative/positive); true => analogInput*scale
        InputId negative;              // digital: contributes -1 when held
        InputId positive;              // digital: contributes +1 when held
        InputId analogInput;           // analog: sampled via Sampler::axis
        float   scale = 1.0f;          // analog scale
    };

    struct ActionState {
        std::vector<InputId>          buttons;      // digital OR-bindings
        std::vector<AxisContribution> axes;         // axis contributions (summed, then clamped)
        bool  cur       = false;                    // active this frame (any button held)
        bool  prev      = false;                    // active last frame (for edge detection)
        float axisValue = 0.0f;                     // combined, clamped axis this frame
    };

    struct StringIdHash {
        std::size_t operator()(maz::core::StringId s) const { return static_cast<std::size_t>(s.hash()); }
    };

    std::unordered_map<maz::core::StringId, ActionState, StringIdHash> m_actions;
};

} // namespace maz::input
