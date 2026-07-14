// Unit tests for maz::input::ActionMap — the device-neutral action-mapping layer.
// Exercises button binding + isDown, edge detection (press/hold/release with no
// re-fire), multi-button OR semantics, digital axes, analog axes with scale and
// clamping, clearBindings/clear, and unknown-action safety. Driven by a MockDevice
// sampler; pure C++, no GPU/display/SDL.

#include "maz/input/ActionMap.hpp"

#include <cstdio>
#include <vector>
#include <utility>

using namespace maz::input;
using maz::core::StringId;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// A minimal Sampler: a set of currently-down digital inputs and per-input analog values.
struct MockDevice {
    std::vector<InputId>                   held;     // currently-down digital inputs
    std::vector<std::pair<InputId, float>> analogs;  // analog values per input

    bool down(InputId id) const {
        for (const InputId& h : held) { if (h == id) { return true; } }
        return false;
    }
    float axis(InputId id) const {
        for (const auto& a : analogs) { if (a.first == id) { return a.second; } }
        return 0.0f;
    }
};

} // namespace

int main() {
    const InputId kSpace  { DeviceType::Keyboard, 44 };
    const InputId kPadA   { DeviceType::Gamepad,  0 };
    const InputId kLeft   { DeviceType::Keyboard, 4 };
    const InputId kRight  { DeviceType::Keyboard, 7 };

    // --- BIND + ISDOWN -------------------------------------------------------
    {
        ActionMap map;
        map.bindButton("Jump", kSpace);
        MockDevice dev;
        dev.held = { kSpace };
        map.update(dev);
        check(map.isDown("Jump"), "bound + held action is down");
        check(!map.isDown("Fire"), "unbound action is not down");
        check(map.hasAction("Jump"), "hasAction true for bound action");
        check(!map.hasAction("Fire"), "hasAction false for unbound action");
        check(map.actionCount() == 1, "actionCount reflects single bound action");
    }

    // --- EDGE DETECTION ------------------------------------------------------
    {
        ActionMap map;
        map.bindButton("Jump", kSpace);
        MockDevice dev;

        map.update(dev);  // empty
        check(!map.isDown("Jump"), "edge: not down while unheld");

        dev.held = { kSpace };
        map.update(dev);  // press
        check(map.isDown("Jump"), "edge: down after press");
        check(map.isPressed("Jump"), "edge: pressed on rising edge");
        check(!map.isReleased("Jump"), "edge: not released on press");

        map.update(dev);  // still held
        check(map.isDown("Jump"), "edge: still down while held");
        check(!map.isPressed("Jump"), "edge: no re-fire of pressed while held");
        check(!map.isReleased("Jump"), "edge: not released while held");

        dev.held = {};
        map.update(dev);  // release
        check(!map.isDown("Jump"), "edge: not down after release");
        check(map.isReleased("Jump"), "edge: released on falling edge");
        check(!map.isPressed("Jump"), "edge: not pressed on release");
    }

    // --- MULTI-BUTTON OR + SINGLE PRESSED ------------------------------------
    {
        ActionMap map;
        map.bindButton("Jump", kSpace);
        map.bindButton("Jump", kPadA);
        MockDevice dev;

        dev.held = { kPadA };
        map.update(dev);
        check(map.isDown("Jump"), "OR: down via gamepad button");
        check(map.isPressed("Jump"), "OR: pressed on first activation");

        dev.held = { kPadA, kSpace };
        map.update(dev);
        check(map.isDown("Jump"), "OR: still down with both held");
        check(!map.isPressed("Jump"), "OR: no re-fire when already active");
    }

    // --- DIGITAL AXIS --------------------------------------------------------
    {
        ActionMap map;
        map.bindAxis("MoveX", kLeft, kRight);  // negative=Left, positive=Right
        MockDevice dev;

        dev.held = { kRight };
        map.update(dev);
        check(map.axis("MoveX") == 1.0f, "digital axis: positive held -> +1");

        dev.held = { kLeft };
        map.update(dev);
        check(map.axis("MoveX") == -1.0f, "digital axis: negative held -> -1");

        dev.held = { kLeft, kRight };
        map.update(dev);
        check(map.axis("MoveX") == 0.0f, "digital axis: both held -> 0");

        dev.held = {};
        map.update(dev);
        check(map.axis("MoveX") == 0.0f, "digital axis: none held -> 0");
    }

    // --- ANALOG AXIS + CLAMP -------------------------------------------------
    {
        ActionMap map;
        map.bindAnalogAxis("MoveX2", kPadA, 1.0f);
        MockDevice dev;

        dev.analogs = { { kPadA, 1.0f } };
        map.update(dev);
        check(map.axis("MoveX2") == 1.0f, "analog axis: +1 passes through");

        dev.analogs = { { kPadA, -1.0f } };
        map.update(dev);
        check(map.axis("MoveX2") == -1.0f, "analog axis: -1 passes through");

        dev.analogs = { { kPadA, 0.0f } };
        map.update(dev);
        check(map.axis("MoveX2") == 0.0f, "analog axis: 0 passes through");

        dev.analogs = { { kPadA, 5.0f } };
        map.update(dev);
        check(map.axis("MoveX2") == 1.0f, "analog axis: out-of-range clamps to +1");
    }

    // --- ANALOG AXIS ACCUMULATION (SUM THEN CLAMP) ---------------------------
    {
        // Axis contributions are SUMMED then clamped — bind two analog contributions to
        // one action on distinct inputs. This guards the accumulator: with last-wins
        // (v = instead of v +=) the sum cases below would report the second value alone
        // and these assertions would fail.
        ActionMap map;
        const InputId axA{ DeviceType::Gamepad, 10 };
        const InputId axB{ DeviceType::Gamepad, 11 };
        map.bindAnalogAxis("SumAxis", axA, 1.0f);
        map.bindAnalogAxis("SumAxis", axB, 1.0f);

        MockDevice dev;
        // 0.5 + 0.5 = 1.0 (in range). Last-wins would give 0.5 -> this fails under the bug.
        dev.analogs = { { axA, 0.5f }, { axB, 0.5f } };
        map.update(dev);
        check(map.axis("SumAxis") == 1.0f, "two analog contributions sum (0.5+0.5==1.0)");

        // 0.5 + (-0.5) = 0.0 — proves SIGNED accumulation (last-wins would give -0.5).
        dev.analogs = { { axA, 0.5f }, { axB, -0.5f } };
        map.update(dev);
        check(map.axis("SumAxis") == 0.0f, "signed analog contributions cancel (0.5 + -0.5 == 0.0)");

        // 1.0 + 1.0 = 2.0 -> clamps to 1.0 (proves accumulate-then-clamp, not clamp-per-contribution masking).
        dev.analogs = { { axA, 1.0f }, { axB, 1.0f } };
        map.update(dev);
        check(map.axis("SumAxis") == 1.0f, "summed contributions clamp (1.0+1.0 -> 1.0)");
    }

    // --- ANALOG AXIS SCALE (fresh action) ------------------------------------
    {
        ActionMap map;
        map.bindAnalogAxis("Scaled", kPadA, 5.0f);
        MockDevice dev;
        dev.analogs = { { kPadA, 1.0f } };
        map.update(dev);
        check(map.axis("Scaled") == 1.0f, "analog axis: scale 5 * 1 clamps to +1");
    }

    // --- CLEAR BINDINGS ------------------------------------------------------
    {
        ActionMap map;
        map.bindButton("Jump", kSpace);
        map.bindButton("Fire", kPadA);
        map.clearBindings("Jump");
        MockDevice dev;
        dev.held = { kSpace, kPadA };
        map.update(dev);
        check(!map.hasAction("Jump"), "clearBindings: action removed");
        check(!map.isDown("Jump"), "clearBindings: cleared action not down");
        check(map.hasAction("Fire"), "clearBindings: other action retained");
    }

    // --- CLEAR ---------------------------------------------------------------
    {
        ActionMap map;
        map.bindButton("Jump", kSpace);
        map.bindButton("Fire", kPadA);
        map.bindAxis("MoveX", kLeft, kRight);
        map.clear();
        check(map.actionCount() == 0, "clear: no actions remain");
        check(!map.isDown("Jump"), "clear: isDown false after clear");
        check(map.axis("MoveX") == 0.0f, "clear: axis 0 after clear");
    }

    // --- UNKNOWN-ACTION SAFETY -----------------------------------------------
    {
        ActionMap map;
        check(!map.isDown("Nope"), "unknown: isDown false");
        check(!map.isPressed("Nope"), "unknown: isPressed false");
        check(!map.isReleased("Nope"), "unknown: isReleased false");
        check(map.axis("Nope") == 0.0f, "unknown: axis 0");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
