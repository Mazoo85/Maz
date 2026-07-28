#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// maz::platform per-target backend seam — the single abstraction an engine crosses to reach a NEW platform
// (a console, a VR headset, a phone) without touching game or renderer code. Every target differs in exactly
// the same handful of ways: how it boots and tears down, what native surface handle the GPU renderer binds
// to, where its readable (bundled assets) and writable (save data) directories live, which input sources
// exist, and whether the OS can suspend/resume the app under you (mobile/console) versus running
// uninterrupted (desktop). `PlatformBackend` names that seam; a concrete backend implements it for one
// target. The engine talks only to the interface, so porting to a new platform is "write one backend",
// which is how Godot/Unity keep one codebase across a dozen devices.
//
// This box can implement + unit-test the HEADLESS backend (pure CPU, no device) and the registry that
// selects a backend by id — that is verified here. The console/VR/mobile backends are stubs behind the same
// interface plus the exact human/hardware step to finish each (NDA SDK, physical headset, device + paid dev
// account), documented in docs/PLATFORMS.md — those can never be marked 100% from this environment.
namespace maz::platform {

enum class PlatformId {
    Headless,          // no window/GPU — CI, servers, tests (implemented + tested here)
    DesktopLinux,      // SDL3 + Vulkan (the engine's native desktop path)
    DesktopWindows,
    DesktopMac,
    Web,               // Emscripten / WebGL2 (see WebLoop.hpp + WEB_BUILD.md)
    Android,           // NEEDS: Android SDK/NDK + device
    iOS,               // NEEDS: Xcode + Apple developer account + device
    VrOpenXR,          // NEEDS: an OpenXR runtime + a physical headset
    ConsoleA,          // NEEDS: platform holder's NDA SDK (names withheld)
    ConsoleB,
    ConsoleC,
};

// What a target can actually do. The engine queries these to enable/disable subsystems per platform.
struct PlatformCaps {
    bool hasGpu = false;       // a real GPU surface the renderer can present to
    bool hasWindow = false;    // a desktop-style resizable window
    bool hasTouch = false;     // touchscreen input
    bool hasGamepad = false;
    bool hasKeyboard = false;
    bool canSuspend = false;   // the OS may pause/resume the app (mobile/console lifecycle)
    bool immersiveVr = false;  // stereo HMD rendering + head/hand tracking
};

// Directory categories a platform must resolve. Assets is read-only bundled content; the rest are writable.
enum class DirKind { Assets, UserData, Cache, Temp };

// Display safe-area insets in PHYSICAL pixels — the thickness the OS reports as unusable on each edge of
// the drawable because of a notch/cutout, rounded corners, the status bar, or the home-indicator/gesture
// bar. Desktop/headless report all-zero. The geometry that turns these into a usable rectangle lives in
// SafeArea.hpp (kept out of this header so PlatformBackend stays dependency-light); mobile backends fill
// them from UIView.safeAreaInsets (iOS) / WindowInsets (Android).
struct SafeAreaInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

// Lifecycle states the OS can drive. Desktop stays Running; mobile/console push Suspended/Resumed as the
// user backgrounds the app, which the engine must honor (pause audio, release the GPU surface, save state).
enum class LifecycleState { Created, Running, Suspended, Stopped };

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;

    virtual PlatformId id() const = 0;
    virtual const char* name() const = 0;
    virtual PlatformCaps caps() const = 0;

    // Bring the platform up (create the surface/context, mount filesystems). Returns false on failure.
    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Opaque native handles the renderer needs for surface creation. Null when not applicable (headless).
    virtual void* nativeWindowHandle() const { return nullptr; }
    virtual void* nativeDisplayHandle() const { return nullptr; }

    // Display safe-area insets (physical pixels). Zero by default — the honest answer for desktop/headless,
    // which have no notch or home bar. Mobile backends override this from the OS so games can lay virtual
    // controls and HUD inside the safe rectangle (see SafeArea.hpp for the geometry helpers).
    virtual SafeAreaInsets safeAreaInsets() const { return {}; }

    // Absolute root directory for a file category on this platform.
    virtual std::string directory(DirKind kind) const = 0;

    // Lifecycle: the platform event pump calls transition(); the engine reads lifecycle(). onLifecycle lets a
    // backend react (e.g. drop the GPU surface on Suspended).
    LifecycleState lifecycle() const { return m_lifecycle; }
    void transition(LifecycleState s) {
        const LifecycleState prev = m_lifecycle;
        m_lifecycle = s;
        onLifecycle(s);
        // Game-facing lifecycle edges. The suspend hook fires the moment the OS backgrounds the app (edge INTO
        // Suspended) — the place to persist progress, since a backgrounded mobile app may be killed without
        // further notice; the resume hook fires when it returns to the foreground. Edge-guarded so a repeated
        // Suspended (e.g. DesktopBackend::syncLifecycle(true) while already minimized) never double-saves.
        if (s == LifecycleState::Suspended && prev != LifecycleState::Suspended) {
            if (m_onSuspend) m_onSuspend();
        } else if (s == LifecycleState::Running && prev == LifecycleState::Suspended) {
            if (m_onResume) m_onResume();
        }
    }

    // Register a game callback fired once on each lifecycle edge. onSuspend is where a mobile game persists
    // its state (autosave); onResume is where it reacquires anything it released. Backend-agnostic — every
    // backend routes lifecycle changes through transition(), so this works on desktop (window minimize/restore
    // via DesktopBackend::syncLifecycle) and on the future Android/iOS backends (onPause / didEnterBackground).
    void setOnSuspend(std::function<void()> fn) { m_onSuspend = std::move(fn); }
    void setOnResume(std::function<void()> fn) { m_onResume = std::move(fn); }

protected:
    virtual void onLifecycle(LifecycleState /*s*/) {}
    LifecycleState m_lifecycle = LifecycleState::Created;
    std::function<void()> m_onSuspend;
    std::function<void()> m_onResume;
};

// The reference backend that runs anywhere: no window, no GPU, deterministic directories. It exists both for
// CI/headless servers and as the worked example every real backend mirrors.
class HeadlessBackend final : public PlatformBackend {
public:
    PlatformId id() const override { return PlatformId::Headless; }
    const char* name() const override { return "headless"; }
    PlatformCaps caps() const override {
        PlatformCaps c;
        c.hasKeyboard = true; // stdin-style input is still possible
        return c;             // everything else false: no gpu/window/touch/suspend
    }

    bool init() override {
        transition(LifecycleState::Running);
        return true;
    }
    void shutdown() override { transition(LifecycleState::Stopped); }

    std::string directory(DirKind kind) const override {
        switch (kind) {
            case DirKind::Assets: return "./assets";
            case DirKind::UserData: return "./user";
            case DirKind::Cache: return "./cache";
            case DirKind::Temp: return "./tmp";
        }
        return ".";
    }

    // Test/inspection hook: how many suspend transitions this backend has seen.
    int suspendCount() const { return m_suspends; }

protected:
    void onLifecycle(LifecycleState s) override {
        if (s == LifecycleState::Suspended) ++m_suspends;
    }

private:
    int m_suspends = 0;
};

// Selects a backend by PlatformId. Real backends register a factory (a shared library, a console SDK module,
// etc.); the engine asks the registry for the current target and gets an interface it can drive uniformly.
class PlatformRegistry {
public:
    using Factory = std::function<std::unique_ptr<PlatformBackend>()>;

    void registerBackend(PlatformId pid, Factory factory) { m_factories[pid] = std::move(factory); }
    bool has(PlatformId pid) const { return m_factories.find(pid) != m_factories.end(); }

    // Create the backend for `pid`, or nullptr if none is registered (e.g. a console SDK not linked in).
    std::unique_ptr<PlatformBackend> create(PlatformId pid) const {
        const auto it = m_factories.find(pid);
        return it == m_factories.end() ? nullptr : it->second();
    }

    std::vector<PlatformId> registered() const {
        std::vector<PlatformId> ids;
        ids.reserve(m_factories.size());
        for (const auto& kv : m_factories) ids.push_back(kv.first);
        return ids;
    }

private:
    std::unordered_map<PlatformId, Factory> m_factories;
};

// Convenience: a registry preloaded with the backends this build actually has (always includes headless, and
// the desktop backend for the host OS). Defined in DesktopBackend.cpp so this header stays dependency-light;
// declared here as the single entry point the engine calls to pick a backend.
PlatformRegistry defaultRegistry();

} // namespace maz::platform
