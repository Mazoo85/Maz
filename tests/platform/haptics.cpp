// tests/platform/haptics.cpp — verifies the haptic-feedback seam: PlatformBackend::triggerHaptic dispatching
// to onHaptic (recorded by HeadlessBackend, no-op on DesktopBackend) plus the pure Haptics.hpp helpers
// (hapticName strings, the monotonic impact-intensity hints, isImpact classification). All deterministic CPU;
// real vibration is the mobile backend's job.
#include "maz/platform/Haptics.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

int main() {
    // --- 1. HeadlessBackend records triggerHaptic dispatches (the seam works headlessly). ---
    {
        HeadlessBackend hb;
        CHECK(hb.hapticCount() == 0, "no haptics yet");
        hb.triggerHaptic(HapticFeedback::Selection);
        CHECK(hb.hapticCount() == 1 && hb.lastHaptic() == HapticFeedback::Selection,
              "triggerHaptic dispatches to onHaptic and records the kind");
        hb.triggerHaptic(HapticFeedback::ImpactHeavy);
        CHECK(hb.hapticCount() == 2 && hb.lastHaptic() == HapticFeedback::ImpactHeavy,
              "a second trigger records the latest kind");
    }

    // --- 2. DesktopBackend: triggerHaptic is a safe no-op (no motor, must not crash). ---
    {
        DesktopBackend db("MazEngine", "HapticTest");
        db.triggerHaptic(HapticFeedback::Warning); // default onHaptic: does nothing
        db.triggerHaptic(HapticFeedback::Error);
        CHECK(true, "desktop triggerHaptic is a harmless no-op");
    }

    // --- 3. hapticName strings. ---
    {
        CHECK(std::strcmp(hapticName(HapticFeedback::Selection), "selection") == 0, "selection name");
        CHECK(std::strcmp(hapticName(HapticFeedback::ImpactMedium), "impact-medium") == 0, "impact-medium name");
        CHECK(std::strcmp(hapticName(HapticFeedback::Error), "error") == 0, "error name");
    }

    // --- 4. Intensity hints: in [0,1], impacts strictly increasing, notifications escalate. ---
    {
        for (HapticFeedback fb : {HapticFeedback::Selection, HapticFeedback::ImpactLight,
                                  HapticFeedback::ImpactMedium, HapticFeedback::ImpactHeavy,
                                  HapticFeedback::Success, HapticFeedback::Warning, HapticFeedback::Error}) {
            const float v = hapticIntensity(fb);
            CHECK(v >= 0.0f && v <= 1.0f, "intensity is within [0,1]");
        }
        CHECK(hapticIntensity(HapticFeedback::ImpactLight) < hapticIntensity(HapticFeedback::ImpactMedium) &&
                  hapticIntensity(HapticFeedback::ImpactMedium) < hapticIntensity(HapticFeedback::ImpactHeavy),
              "impact intensities are strictly increasing light < medium < heavy");
        CHECK(hapticIntensity(HapticFeedback::Success) < hapticIntensity(HapticFeedback::Warning) &&
                  hapticIntensity(HapticFeedback::Warning) < hapticIntensity(HapticFeedback::Error),
              "notification intensities escalate success < warning < error");
    }

    // --- 5. isImpact classifies only the three collision kinds. ---
    {
        CHECK(isImpact(HapticFeedback::ImpactLight) && isImpact(HapticFeedback::ImpactMedium) &&
                  isImpact(HapticFeedback::ImpactHeavy),
              "the three impact kinds are impacts");
        CHECK(!isImpact(HapticFeedback::Selection) && !isImpact(HapticFeedback::Success) &&
                  !isImpact(HapticFeedback::Warning) && !isImpact(HapticFeedback::Error),
              "selection and notification kinds are not impacts");
    }

    if (g_fail == 0) {
        std::printf("haptics: OK — backend dispatch/record, desktop no-op, names, intensity ordering, isImpact.\n");
        return 0;
    }
    std::printf("haptics: %d failure(s).\n", g_fail);
    return 1;
}
