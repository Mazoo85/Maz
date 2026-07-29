#pragma once

#include "maz/platform/PlatformBackend.hpp" // platform::PowerState / PowerSource (the POD the backend reports)

#include <algorithm>

// maz::platform power-aware policy — the pure decisions a mobile game makes from a PowerState snapshot.
// PlatformBackend::powerState() reports WHAT the device's power/thermal situation is; these free functions
// turn that into WHAT TO DO about it (should we enter battery-saver pacing? what frame-rate cap fits?),
// kept out of the backend so the policy is deterministic and unit-testable and the numbers stay tunable by
// the game. The natural pairing is with core::FramePacer: each frame (or on a power-change event) do
// `pacer.setActiveFps(recommendedFps(backend->powerState()))`, and the loop eases off exactly when the
// phone needs it to. On desktop/headless the state is neutral, so every helper returns the full-power
// answer — no behavior change.
namespace maz::platform {

// True when the game should ease off to spare the battery or a throttling SoC: the OS battery-saver is on,
// the device is thermally throttling, or it is running on battery at or below `lowThreshold` (0..1) charge.
// Plugged-in or unknown-source devices never trigger the low-battery case (charging isn't an emergency).
inline bool recommendsPowerSave(const PowerState& p, float lowThreshold = 0.20f) {
    if (p.lowPowerMode || p.thermalThrottling) {
        return true;
    }
    if (p.source == PowerSource::Battery && p.batteryLevel <= lowThreshold) {
        return true;
    }
    return false;
}

// The frame-rate cap that fits the current power state: `saverFps` when recommendsPowerSave(), else
// `activeFps`. Feed the result to core::FramePacer::setActiveFps. Defaults: 60 normally, 30 when saving.
inline double recommendedFps(const PowerState& p, double activeFps = 60.0, double saverFps = 30.0,
                             float lowThreshold = 0.20f) {
    return recommendsPowerSave(p, lowThreshold) ? saverFps : activeFps;
}

// A 0..1 "effort" scalar other subsystems can multiply into their own budgets (particle counts, shadow
// resolution, dynamic-resolution floor): 1.0 at full power, easing toward `floor` as the battery drains on
// battery power, and clamped to `floor` whenever a hard saver/throttle signal is active. Monotonic and
// clamped so it is safe to multiply directly.
inline float powerBudgetScale(const PowerState& p, float floor = 0.5f) {
    floor = std::min(1.0f, std::max(0.0f, floor));
    if (p.lowPowerMode || p.thermalThrottling) {
        return floor;
    }
    if (p.source != PowerSource::Battery) {
        return 1.0f; // plugged in or unknown: full effort
    }
    // On battery: scale linearly from `floor` at empty to 1.0 at full charge.
    const float level = std::min(1.0f, std::max(0.0f, p.batteryLevel));
    return floor + (1.0f - floor) * level;
}

} // namespace maz::platform
