#pragma once

#include "maz/platform/PlatformBackend.hpp" // platform::HapticFeedback

// maz::platform haptics helpers — small pure utilities around the HapticFeedback kinds a game requests via
// PlatformBackend::triggerHaptic(). The trigger itself lives on the backend (no-op on desktop, the OS haptic
// API on mobile); these are the tunable/loggable bits: a stable name for each kind, and a 0..1 intensity hint
// a backend can scale its vibration amplitude by (iOS's generators are categorical, but Android's
// VibrationEffect takes an amplitude, so a numeric hint is useful). Deterministic and header-only.
namespace maz::platform {

inline const char* hapticName(HapticFeedback fb) {
    switch (fb) {
        case HapticFeedback::Selection: return "selection";
        case HapticFeedback::ImpactLight: return "impact-light";
        case HapticFeedback::ImpactMedium: return "impact-medium";
        case HapticFeedback::ImpactHeavy: return "impact-heavy";
        case HapticFeedback::Success: return "success";
        case HapticFeedback::Warning: return "warning";
        case HapticFeedback::Error: return "error";
    }
    return "unknown";
}

// A relative strength in [0,1] a backend can use as the vibration amplitude for this feedback kind. The three
// Impact levels are strictly increasing; Selection is the lightest; the notification kinds escalate
// success < warning < error. These are hints, not a spec — a backend may map them however its OS prefers.
inline float hapticIntensity(HapticFeedback fb) {
    switch (fb) {
        case HapticFeedback::Selection: return 0.25f;
        case HapticFeedback::ImpactLight: return 0.35f;
        case HapticFeedback::ImpactMedium: return 0.60f;
        case HapticFeedback::ImpactHeavy: return 1.00f;
        case HapticFeedback::Success: return 0.45f;
        case HapticFeedback::Warning: return 0.65f;
        case HapticFeedback::Error: return 0.85f;
    }
    return 0.0f;
}

// True for the three collision/impact kinds (as opposed to selection ticks and notification patterns).
inline bool isImpact(HapticFeedback fb) {
    return fb == HapticFeedback::ImpactLight || fb == HapticFeedback::ImpactMedium ||
           fb == HapticFeedback::ImpactHeavy;
}

} // namespace maz::platform
