#include "maz/platform/DesktopBackend.hpp"

#include "maz/io/VirtualFileSystem.hpp"
#include "maz/platform/Paths.hpp"

#include <utility>

namespace maz::platform {

PlatformId DesktopBackend::hostDesktopId() {
#if defined(_WIN32)
    return PlatformId::DesktopWindows;
#elif defined(__APPLE__)
    return PlatformId::DesktopMac;
#else
    return PlatformId::DesktopLinux;
#endif
}

DesktopBackend::DesktopBackend(std::string org, std::string app, PlatformId id)
    : m_org(std::move(org)), m_app(std::move(app)), m_id(id) {}

const char* DesktopBackend::name() const {
    switch (m_id) {
        case PlatformId::DesktopWindows: return "desktop-windows";
        case PlatformId::DesktopMac: return "desktop-mac";
        case PlatformId::DesktopLinux: return "desktop-linux";
        default: return "desktop";
    }
}

PlatformCaps DesktopBackend::caps() const {
    PlatformCaps c;
    c.hasGpu = m_window != nullptr;    // a real presentable surface exists once a window is attached
    c.hasWindow = m_window != nullptr;
    c.hasKeyboard = true;
    c.hasGamepad = true;
    c.hasTouch = m_hasTouch;           // some desktops (touch monitors, 2-in-1s) have it
    c.canSuspend = false;              // desktops run uninterrupted; minimize is tracked but not an OS suspend
    c.immersiveVr = false;
    return c;
}

bool DesktopBackend::init() {
    transition(LifecycleState::Running);
    return true;
}

void DesktopBackend::shutdown() { transition(LifecycleState::Stopped); }

std::string DesktopBackend::directory(DirKind kind) const {
    // Assets are read-only, shipped next to the executable. The rest live under the per-user pref dir, so
    // saves/caches survive reinstalls and don't need write access to the install location.
    switch (kind) {
        case DirKind::Assets:
            return "./assets";
        case DirKind::UserData:
            return prefPath(m_org.c_str(), m_app.c_str(), "");
        case DirKind::Cache:
            return prefPath(m_org.c_str(), m_app.c_str(), "cache");
        case DirKind::Temp:
            return prefPath(m_org.c_str(), m_app.c_str(), "tmp");
    }
    return ".";
}

void DesktopBackend::mountStandard(io::VirtualFileSystem& vfs) const {
    vfs.mount("res", directory(DirKind::Assets));
    vfs.mount("user", directory(DirKind::UserData));
}

void DesktopBackend::syncLifecycle(bool minimized) {
    if (minimized && lifecycle() == LifecycleState::Running) {
        transition(LifecycleState::Suspended);
    } else if (!minimized && lifecycle() == LifecycleState::Suspended) {
        transition(LifecycleState::Running);
    }
}

// The backends this build actually ships: the reference headless backend (everywhere) plus the desktop
// backend for the host OS. Android/iOS/console/VR backends are registered by their own build variants when
// their toolchains are present (see docs/PLATFORMS.md + docs/MOBILE_BUILD.md); create(pid) returns null here
// for any target not registered, which the engine treats as "not built for this platform".
PlatformRegistry defaultRegistry() {
    PlatformRegistry reg;
    reg.registerBackend(PlatformId::Headless, [] {
        return std::unique_ptr<PlatformBackend>(std::make_unique<HeadlessBackend>());
    });
    reg.registerBackend(DesktopBackend::hostDesktopId(), [] {
        return std::unique_ptr<PlatformBackend>(std::make_unique<DesktopBackend>());
    });
    return reg;
}

} // namespace maz::platform
