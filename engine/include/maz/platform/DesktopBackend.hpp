#pragma once

#include "maz/platform/PlatformBackend.hpp"

#include <string>

namespace maz::io {
class VirtualFileSystem;
}

namespace maz::platform {

// The desktop backend (Linux / Windows / macOS): the first REAL PlatformBackend, the worked example the
// future AndroidBackend / IOSBackend mirror. Where HeadlessBackend reports "no gpu/window", DesktopBackend
// fills the seam the running engine actually uses — a GPU-capable window, a native surface handle for the
// Vulkan renderer, per-user writable directories, and OS focus/minimize driving the lifecycle. It stays SDL-
// free at the type level (the SDL window arrives as an opaque void*), so this header pulls in no platform
// headers and the whole thing unit-tests on any host.
//
// Wiring (see apps/_template and docs/MOBILE_BUILD.md): create it (or via defaultRegistry()), attachWindow()
// the SDL window handle, init(), then mountStandard() to route res:// -> assets and user:// -> save dir into
// a VirtualFileSystem. Each frame, syncLifecycle(minimized) turns an OS minimize into a Suspended transition
// and a restore back into Running — the exact hook a mobile backend uses for background/foreground, exercised
// here so the lifecycle path is real before a device is in hand.
class DesktopBackend final : public PlatformBackend {
public:
    // `org`/`app` name the per-user writable directory (SDL_GetPrefPath -> ~/.local/share/<org>/<app>/ etc.).
    // `id` defaults to the host OS's desktop platform id; pass one explicitly only to emulate another desktop.
    explicit DesktopBackend(std::string org = "MazEngine", std::string app = "App",
                            PlatformId id = hostDesktopId());

    // The desktop id for the OS this build was compiled for.
    static PlatformId hostDesktopId();

    PlatformId id() const override { return m_id; }
    const char* name() const override;
    PlatformCaps caps() const override;

    // Attach the SDL window before init(): its handle becomes nativeWindowHandle() for surface creation, and
    // marks the backend windowed/GPU-capable. Pass nullptr for a windowless desktop tool.
    void attachWindow(void* sdlWindow) { m_window = sdlWindow; }
    // Report whether a touchscreen is present (some 2-in-1 laptops / touch monitors). Off by default.
    void setHasTouch(bool has) { m_hasTouch = has; }

    bool init() override;
    void shutdown() override;

    void* nativeWindowHandle() const override { return m_window; }

    std::string directory(DirKind kind) const override;

    // Mount the standard schemes into `vfs`: res:// -> directory(Assets), user:// -> directory(UserData).
    // Games then do all file I/O through schemes, so an APK/bundle remap is a one-line change on mobile.
    void mountStandard(io::VirtualFileSystem& vfs) const;

    // Drive the lifecycle from the window's OS state: a minimize becomes Suspended, a restore returns to
    // Running. Call once per frame with window.isMinimized(). Idempotent (only transitions on change).
    void syncLifecycle(bool minimized);

private:
    std::string m_org;
    std::string m_app;
    PlatformId m_id;
    void* m_window = nullptr;
    bool m_hasTouch = false;
};

} // namespace maz::platform
