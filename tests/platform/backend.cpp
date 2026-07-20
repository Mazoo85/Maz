// tests/platform/backend.cpp — verifies the per-platform backend seam (platform::PlatformBackend +
// HeadlessBackend + PlatformRegistry). The headless backend and the registry are pure CPU, so they prove
// here: capability reporting, init/shutdown lifecycle, directory resolution, OS suspend/resume transitions,
// and backend selection by id (including "not registered" for console/VR/mobile SDKs absent from this build).
#include "maz/platform/PlatformBackend.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

int main() {
    // --- 1. Headless backend: capabilities, lifecycle, directories. ---
    {
        HeadlessBackend hb;
        CHECK(hb.id() == PlatformId::Headless, "id is Headless");
        CHECK(std::string(hb.name()) == "headless", "name is headless");

        const PlatformCaps c = hb.caps();
        CHECK(!c.hasGpu && !c.hasWindow && !c.hasTouch, "headless reports no gpu/window/touch");
        CHECK(hb.nativeWindowHandle() == nullptr, "headless has no native window handle");

        CHECK(hb.lifecycle() == LifecycleState::Created, "starts in Created");
        CHECK(hb.init(), "init succeeds");
        CHECK(hb.lifecycle() == LifecycleState::Running, "init moves to Running");

        CHECK(!hb.directory(DirKind::Assets).empty(), "assets dir resolved");
        CHECK(hb.directory(DirKind::UserData) != hb.directory(DirKind::Assets),
              "user data dir differs from assets");

        hb.shutdown();
        CHECK(hb.lifecycle() == LifecycleState::Stopped, "shutdown moves to Stopped");
    }

    // --- 2. Lifecycle transitions (mobile/console suspend/resume) reach the backend hook. ---
    {
        HeadlessBackend hb;
        hb.init();
        CHECK(hb.suspendCount() == 0, "no suspends yet");
        hb.transition(LifecycleState::Suspended);
        CHECK(hb.lifecycle() == LifecycleState::Suspended && hb.suspendCount() == 1,
              "suspend transition observed by the backend");
        hb.transition(LifecycleState::Running); // resume
        CHECK(hb.lifecycle() == LifecycleState::Running, "resume returns to Running");
        hb.transition(LifecycleState::Suspended);
        CHECK(hb.suspendCount() == 2, "second suspend counted");
    }

    // --- 3. Registry selects a backend by id; absent backends (console/VR/mobile SDKs) return null. ---
    {
        PlatformRegistry reg = defaultRegistry();
        CHECK(reg.has(PlatformId::Headless), "headless registered by default");
        CHECK(!reg.has(PlatformId::VrOpenXR), "VR backend not present in this build");
        CHECK(!reg.has(PlatformId::ConsoleA), "console backend not present (NDA SDK absent)");

        std::unique_ptr<PlatformBackend> hb = reg.create(PlatformId::Headless);
        CHECK(hb != nullptr && hb->id() == PlatformId::Headless, "created the headless backend");
        CHECK(reg.create(PlatformId::Android) == nullptr, "unregistered platform yields null");
        CHECK(reg.registered().size() == 1, "exactly one backend registered here");
    }

    // --- 4. A custom backend registers and drives through the same interface (extension point works). ---
    {
        PlatformRegistry reg;
        reg.registerBackend(PlatformId::DesktopLinux, [] {
            return std::unique_ptr<PlatformBackend>(std::make_unique<HeadlessBackend>());
        });
        CHECK(reg.has(PlatformId::DesktopLinux), "custom backend registered");
        auto b = reg.create(PlatformId::DesktopLinux);
        CHECK(b != nullptr && b->init(), "custom backend creates + inits via the interface");
    }

    if (g_fail == 0) {
        std::printf("platform_backend: OK — caps, lifecycle, dirs, suspend/resume, registry selection.\n");
        return 0;
    }
    std::printf("platform_backend: %d failure(s).\n", g_fail);
    return 1;
}
