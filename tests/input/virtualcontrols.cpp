// tests/input/virtualcontrols.cpp — verifies the on-screen virtual controls (input::VirtualStick /
// VirtualButton / VirtualControls) that make a touch build play like a gamepad. Pure CPU: a platform::Input
// is driven with synthetic touch points (exactly as Window::pumpEvents feeds it from SDL finger events), so
// this proves here: a fixed stick maps thumb offset to a [-1,1] vector with screen-Y inverted; a floating
// stick recenters under first-touch; a button reports held/pressed/released edges; and multi-touch keeps a
// movement finger and an aim/button finger on separate widgets (one claim per contact per frame).
#include "maz/input/VirtualControls.hpp"
#include "maz/platform/Input.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

using namespace maz;
using platform::Input;
using platform::TouchPhase;
using input::VirtualStick;
using input::VirtualButton;
using input::VirtualControls;

int main() {
    // --- 1. Fixed stick: thumb offset -> conditioned vector, screen-Y inverted, knob clamped to ring. ---
    {
        Input in;
        VirtualStick stick(math::vec2(200.0f, 400.0f), 100.0f, /*floating=*/false);
        stick.setDeadzone(0.0f); // isolate the mapping from the deadzone rescale for an exact check

        std::vector<int64_t> claimed;
        in.newFrame();
        // Touch fully to the right edge of the ring: offset (+100, 0) -> value (+1, 0).
        in.onTouch(1, 300.0f, 400.0f, 0.0f, 0.0f, TouchPhase::Down);
        stick.update(in, claimed);
        auto s = stick.state();
        CHECK(s.active, "fixed stick active while touched");
        CHECK(near(s.value.x, 1.0f) && near(s.value.y, 0.0f), "right edge -> (+1, 0)");

        // Drag upward on screen (smaller y) -> +Y in the stick vector (Y inverted).
        claimed.clear();
        in.newFrame();
        in.onTouch(1, 200.0f, 300.0f, 0.0f, -100.0f, TouchPhase::Move);
        stick.update(in, claimed);
        s = stick.state();
        CHECK(near(s.value.x, 0.0f) && near(s.value.y, 1.0f), "screen-up drag -> (0, +1)");

        // Beyond the ring the knob clamps to radius but the value magnitude never exceeds 1.
        claimed.clear();
        in.newFrame();
        in.onTouch(1, 600.0f, 400.0f, 400.0f, 0.0f, TouchPhase::Move); // way past the right edge
        stick.update(in, claimed);
        s = stick.state();
        const float vlen = std::sqrt(s.value.x * s.value.x + s.value.y * s.value.y);
        CHECK(vlen <= 1.0f + 1e-4f, "value clamped to unit circle past the ring");
        CHECK(near(s.knob.x, 300.0f) && near(s.knob.y, 400.0f), "drawn knob clamped to the ring edge");

        // Release -> inactive, value zero.
        claimed.clear();
        in.newFrame();
        in.onTouch(1, 600.0f, 400.0f, 0.0f, 0.0f, TouchPhase::Up);
        stick.update(in, claimed);
        s = stick.state();
        CHECK(!s.active && near(s.value.x, 0.0f) && near(s.value.y, 0.0f), "released stick reads zero");
    }

    // --- 2. Floating stick recenters to first-touch: same drag from a different spot gives the same vector. ---
    {
        Input in;
        VirtualStick stick(math::vec2(150.0f, 500.0f), 100.0f, /*floating=*/true);
        stick.setDeadzone(0.0f);

        std::vector<int64_t> claimed;
        in.newFrame();
        // First touch lands far from the configured center; the base should move under the thumb.
        in.onTouch(5, 400.0f, 300.0f, 0.0f, 0.0f, TouchPhase::Down);
        stick.update(in, claimed);
        auto s = stick.state();
        CHECK(near(s.base.x, 400.0f) && near(s.base.y, 300.0f), "floating base recenters to first touch");
        CHECK(near(s.value.x, 0.0f) && near(s.value.y, 0.0f), "no offset yet -> zero vector at recenter");

        // Now drag right by the full radius from the recentered base.
        claimed.clear();
        in.newFrame();
        in.onTouch(5, 500.0f, 300.0f, 100.0f, 0.0f, TouchPhase::Move);
        stick.update(in, claimed);
        s = stick.state();
        CHECK(near(s.value.x, 1.0f) && near(s.value.y, 0.0f), "drag from recentered base -> (+1, 0)");
    }

    // --- 3. Button held/pressed/released edges. ---
    {
        Input in;
        VirtualButton btn(math::vec2(700.0f, 500.0f), 60.0f);
        std::vector<int64_t> claimed;

        in.newFrame(); // no touch
        btn.update(in, claimed);
        CHECK(!btn.held() && !btn.pressed() && !btn.released(), "idle button all-false");

        claimed.clear();
        in.newFrame();
        in.onTouch(9, 710.0f, 505.0f, 0.0f, 0.0f, TouchPhase::Down); // inside radius
        btn.update(in, claimed);
        CHECK(btn.held() && btn.pressed() && !btn.released(), "press frame: held + pressed edge");

        claimed.clear();
        in.newFrame();
        in.onTouch(9, 710.0f, 505.0f, 0.0f, 0.0f, TouchPhase::Move); // still down
        btn.update(in, claimed);
        CHECK(btn.held() && !btn.pressed(), "hold frame: held, no repeated press edge");

        claimed.clear();
        in.newFrame();
        in.onTouch(9, 710.0f, 505.0f, 0.0f, 0.0f, TouchPhase::Up);
        btn.update(in, claimed);
        CHECK(!btn.held() && btn.released(), "release frame: released edge");

        // A touch outside the radius never activates it.
        claimed.clear();
        in.newFrame();
        in.onTouch(10, 100.0f, 100.0f, 0.0f, 0.0f, TouchPhase::Down);
        btn.update(in, claimed);
        CHECK(!btn.held(), "touch outside radius does not press");
    }

    // --- 4. Multi-touch: move stick + aim stick + button, each claiming a distinct finger. ---
    {
        Input in;
        VirtualControls vc;
        vc.moveStick = VirtualStick(math::vec2(200.0f, 500.0f), 100.0f, /*floating=*/true);
        vc.moveStick.setActivationRadius(300.0f); // left half
        vc.moveStick.setDeadzone(0.0f);
        vc.aimStick = VirtualStick(math::vec2(900.0f, 500.0f), 100.0f, /*floating=*/true);
        vc.aimStick.setActivationRadius(300.0f);   // right half
        vc.aimStick.setDeadzone(0.0f);
        vc.buttons.push_back(VirtualButton(math::vec2(1100.0f, 200.0f), 60.0f));

        in.newFrame();
        in.onTouch(1, 200.0f, 500.0f, 0.0f, 0.0f, TouchPhase::Down); // left thumb on move
        in.onTouch(2, 900.0f, 500.0f, 0.0f, 0.0f, TouchPhase::Down); // right thumb on aim
        in.onTouch(3, 1100.0f, 200.0f, 0.0f, 0.0f, TouchPhase::Down); // finger on the button
        vc.update(in);
        CHECK(vc.moveStick.active(), "move stick grabbed the left finger");
        CHECK(vc.aimStick.active(), "aim stick grabbed the right finger");
        CHECK(vc.buttons[0].held() && vc.buttons[0].pressed(), "button grabbed the third finger");
        CHECK(vc.claimedCount() == 3, "three distinct contacts claimed, one per widget");

        // Drag the two thumbs in opposite directions; each stick reflects only its own finger.
        in.newFrame();
        in.onTouch(1, 300.0f, 500.0f, 100.0f, 0.0f, TouchPhase::Move); // move right
        in.onTouch(2, 800.0f, 500.0f, -100.0f, 0.0f, TouchPhase::Move); // aim left
        vc.update(in);
        CHECK(near(vc.moveStick.state().value.x, 1.0f), "move stick -> +X from its own finger");
        CHECK(near(vc.aimStick.state().value.x, -1.0f), "aim stick -> -X, unaffected by the move finger");
    }

    if (g_fail == 0) {
        std::printf("input_virtualcontrols: OK — fixed/floating sticks, button edges, multi-touch claim.\n");
        return 0;
    }
    std::printf("input_virtualcontrols: %d failure(s).\n", g_fail);
    return 1;
}
