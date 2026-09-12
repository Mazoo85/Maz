// tests/platform/powerstate.cpp — verifies platform::PowerState policy: the pure decisions a mobile game
// makes from a battery/thermal snapshot (recommendsPowerSave / recommendedFps / powerBudgetScale) plus the
// PlatformBackend::powerState() seam's neutral default. All deterministic CPU logic; the live values come
// from the mobile backend at runtime.
#include "maz/platform/PowerState.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
    // --- 1. Neutral / desktop default: full power, no saver. ---
    {
        PowerState p; // Unknown source, 1.0 battery, no saver/throttle
        CHECK(!recommendsPowerSave(p), "neutral state does not recommend power save");
        CHECK(approx(static_cast<float>(recommendedFps(p)), 60.0f), "neutral -> full 60fps");
        CHECK(approx(powerBudgetScale(p), 1.0f), "neutral -> full effort budget");
    }

    // --- 2. Low battery ON battery -> save; the same low level while plugged in does NOT. ---
    {
        PowerState onBat;
        onBat.source = PowerSource::Battery;
        onBat.batteryLevel = 0.15f; // below the 0.20 default threshold
        CHECK(recommendsPowerSave(onBat), "low battery on battery recommends power save");
        CHECK(approx(static_cast<float>(recommendedFps(onBat)), 30.0f), "low battery -> saver 30fps");

        PowerState plugged = onBat;
        plugged.source = PowerSource::PluggedIn; // charging: low level is not an emergency
        CHECK(!recommendsPowerSave(plugged), "low level while charging does not recommend power save");
        CHECK(approx(powerBudgetScale(plugged), 1.0f), "charging keeps full effort regardless of level");
    }

    // --- 3. The threshold is respected: just above stays full, at/below saves. ---
    {
        PowerState p;
        p.source = PowerSource::Battery;
        p.batteryLevel = 0.25f;
        CHECK(!recommendsPowerSave(p), "battery above threshold: no save");
        p.batteryLevel = 0.20f; // exactly at threshold counts as low (<=)
        CHECK(recommendsPowerSave(p), "battery at the threshold counts as low");
        p.batteryLevel = 0.5f;
        CHECK(recommendsPowerSave(p, 0.6f), "a higher custom threshold triggers earlier");
    }

    // --- 4. Hard signals (OS low-power mode, thermal throttle) force save even on a full/plugged battery. ---
    {
        PowerState lp;
        lp.source = PowerSource::PluggedIn;
        lp.batteryLevel = 1.0f;
        lp.lowPowerMode = true;
        CHECK(recommendsPowerSave(lp), "OS low-power mode forces power save even while charging");
        CHECK(approx(powerBudgetScale(lp), 0.5f), "low-power mode clamps the budget to the floor");

        PowerState hot;
        hot.source = PowerSource::PluggedIn;
        hot.thermalThrottling = true;
        CHECK(recommendsPowerSave(hot), "thermal throttling forces power save");
        CHECK(approx(powerBudgetScale(hot, 0.3f), 0.3f), "throttling clamps to the custom floor");
    }

    // --- 5. powerBudgetScale eases linearly with charge on battery. ---
    {
        PowerState p;
        p.source = PowerSource::Battery;
        p.batteryLevel = 1.0f;
        CHECK(approx(powerBudgetScale(p, 0.5f), 1.0f), "full battery -> full budget");
        p.batteryLevel = 0.0f;
        CHECK(approx(powerBudgetScale(p, 0.5f), 0.5f), "empty battery -> floor budget");
        p.batteryLevel = 0.5f;
        CHECK(approx(powerBudgetScale(p, 0.5f), 0.75f), "half battery -> midway between floor and full");
        // Result is always in [floor, 1].
        p.batteryLevel = 0.8f;
        const float s = powerBudgetScale(p, 0.5f);
        CHECK(s >= 0.5f && s <= 1.0f, "budget stays within [floor, 1]");
    }

    // --- 6. recommendedFps honors custom active/saver caps. ---
    {
        PowerState save;
        save.thermalThrottling = true;
        CHECK(approx(static_cast<float>(recommendedFps(save, 120.0, 45.0)), 45.0f),
              "custom saver cap used when saving");
        PowerState ok;
        CHECK(approx(static_cast<float>(recommendedFps(ok, 120.0, 45.0)), 120.0f),
              "custom active cap used when healthy");
    }

    // --- 7. Backend seam: DesktopBackend reports the neutral default (no battery API here). ---
    {
        DesktopBackend db("MazEngine", "PowerTest");
        const PowerState p = db.powerState();
        CHECK(p.source == PowerSource::Unknown, "desktop power source is Unknown");
        CHECK(approx(p.batteryLevel, 1.0f) && !p.lowPowerMode && !p.thermalThrottling,
              "desktop reports full battery, no saver/throttle");
        CHECK(!recommendsPowerSave(p), "desktop never recommends power save");
    }

    if (g_fail == 0) {
        std::printf("power_state: OK — save recommendation, fps mapping, budget scale, thresholds, seam.\n");
        return 0;
    }
    std::printf("power_state: %d failure(s).\n", g_fail);
    return 1;
}
