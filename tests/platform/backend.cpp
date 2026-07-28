// tests/platform/backend.cpp — verifies the per-platform backend seam (platform::PlatformBackend +
// HeadlessBackend + PlatformRegistry). The headless backend and the registry are pure CPU, so they prove
// here: capability reporting, init/shutdown lifecycle, directory resolution, OS suspend/resume transitions,
// and backend selection by id (including "not registered" for console/VR/mobile SDKs absent from this build).
#include "maz/platform/PlatformBackend.hpp"

#include "maz/io/VirtualFileSystem.hpp"
#include "maz/platform/DesktopBackend.hpp"

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
        CHECK(reg.registered().size() == 2, "headless + host desktop registered by default");
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

    // --- 5. DesktopBackend: the first real backend — caps, native handle, directories, VFS mounts. ---
    {
        DesktopBackend db("MazEngine", "BackendTest");
        CHECK(db.id() == DesktopBackend::hostDesktopId(), "desktop id matches the host OS");
        CHECK(std::string(db.name()).rfind("desktop", 0) == 0, "desktop name is 'desktop-*'");

        // Before a window is attached: no GPU/window surface, but keyboard/gamepad exist.
        PlatformCaps c0 = db.caps();
        CHECK(!c0.hasGpu && !c0.hasWindow, "no gpu/window until a window is attached");
        CHECK(c0.hasKeyboard && c0.hasGamepad, "desktop always has keyboard + gamepad");
        CHECK(!c0.canSuspend, "desktop does not report OS suspend capability");
        CHECK(db.nativeWindowHandle() == nullptr, "no native handle before attach");

        // Attach a stand-in window handle: now the renderer-facing surface is available.
        int fakeWindow = 0;
        db.attachWindow(&fakeWindow);
        PlatformCaps c1 = db.caps();
        CHECK(c1.hasGpu && c1.hasWindow, "attaching a window enables gpu/window caps");
        CHECK(db.nativeWindowHandle() == &fakeWindow, "native handle is the attached window");
        db.setHasTouch(true);
        CHECK(db.caps().hasTouch, "touch capability is reportable (touch monitors / 2-in-1s)");

        CHECK(db.init(), "desktop init succeeds");
        CHECK(db.lifecycle() == LifecycleState::Running, "desktop init -> Running");

        // Directories: Assets is the read-only bundle dir; the writable dirs are distinct and non-empty.
        CHECK(db.directory(DirKind::Assets) == "./assets", "assets dir is ./assets");
        CHECK(!db.directory(DirKind::UserData).empty(), "user data dir resolved");
        CHECK(db.directory(DirKind::UserData) != db.directory(DirKind::Cache),
              "user data and cache dirs differ");

        // Lifecycle from OS minimize/restore — the exact hook a mobile backend uses for background/foreground.
        db.syncLifecycle(true); // minimized
        CHECK(db.lifecycle() == LifecycleState::Suspended, "minimize -> Suspended");
        db.syncLifecycle(true); // still minimized: no spurious re-transition
        CHECK(db.lifecycle() == LifecycleState::Suspended, "staying minimized keeps Suspended");
        db.syncLifecycle(false); // restored
        CHECK(db.lifecycle() == LifecycleState::Running, "restore -> Running");

        db.shutdown();
        CHECK(db.lifecycle() == LifecycleState::Stopped, "desktop shutdown -> Stopped");
    }

    // --- 6. VFS mounts: the backend routes res:// -> assets and user:// -> save dir through schemes. ---
    {
        DesktopBackend db("MazEngine", "BackendTest");
        maz::io::VirtualFileSystem vfs;
        db.mountStandard(vfs);
        CHECK(vfs.isMounted("res"), "res:// mounted");
        CHECK(vfs.isMounted("user"), "user:// mounted");
        // A res:// path resolves under the assets dir; the traversal guard still refuses escapes.
        const std::string hero = vfs.resolve("res://textures/hero.png");
        CHECK(hero.find("assets") != std::string::npos, "res:// resolves under the assets dir");
        CHECK(hero.find("hero.png") != std::string::npos, "res:// keeps the sub-path");
        CHECK(vfs.resolve("res://../../etc/passwd").empty(), "res:// escape is refused");
        CHECK(!vfs.resolve("user://save1.dat").empty(), "user:// resolves for saves");
    }

    // --- 7. defaultRegistry now ships the host desktop backend alongside headless. ---
    {
        PlatformRegistry reg = defaultRegistry();
        CHECK(reg.has(PlatformId::Headless), "headless still registered");
        CHECK(reg.has(DesktopBackend::hostDesktopId()), "host desktop backend registered by default");
        auto db = reg.create(DesktopBackend::hostDesktopId());
        CHECK(db != nullptr && db->id() == DesktopBackend::hostDesktopId(),
              "registry creates the desktop backend for this OS");
        CHECK(reg.registered().size() == 2, "exactly headless + desktop registered here");
    }

    if (g_fail == 0) {
        std::printf("platform_backend: OK — headless + desktop caps, lifecycle, dirs, suspend/resume, "
                    "VFS mounts, registry selection.\n");
        return 0;
    }
    std::printf("platform_backend: %d failure(s).\n", g_fail);
    return 1;
}
