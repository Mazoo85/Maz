# Building Maz games for mobile (iOS + Android)

This is the device hand-off. The **engine-side foundation is done and desktop-verified** (see
[PLATFORMS.md → Mobile foundation](PLATFORMS.md#mobile-foundation-delivered--desktop-verified)); what
remains is the per-platform **backend `.cpp` + native project**, which needs a Mac/Xcode (iOS) and the
Android SDK/NDK — hardware and toolchains a cloud Linux box cannot have. Everything below is written so
that on the right machine it is a mechanical build, not a design problem.

Maz is SDL3 + Vulkan + C++20. SDL3 already supports iOS and Android natively, and its shaders are
SPIR-V (portable to Android Vulkan directly and to iOS via MoltenVK, no shader rewrite). The renderer is
already MoltenVK/portability-safe and has a `--mobile` render tier. So the remaining work is small and
well-scoped.

---

## What's already in place (don't redo these)

| Concern | Where it lives | Status |
|---|---|---|
| Touch input | `platform::Input` (multi-touch) fed by `Window::pumpEvents` SDL finger events | ✅ done |
| On-screen controls | `input::VirtualControls` (virtual sticks + buttons) | ✅ done |
| OS-owned loop | `platform::runMainLoop(step, user)` (apps use it; see `apps/_template/`) | ✅ done |
| Backend seam | `platform::PlatformBackend` + `DesktopBackend` as the worked example | ✅ done |
| Asset/save routing | `io::VirtualFileSystem` `res://`/`user://`, mounted by the backend at boot | ✅ done |
| Autosave on background | `PlatformBackend::setOnSuspend/​setOnResume` — fired on the lifecycle edge (see `apps/_template/`) | ✅ done |
| Notch / safe area | `platform::SafeArea` (`safeAreaRect`/`clampPointToSafeArea`/`fitRectInSafeArea`) + `PlatformBackend::safeAreaInsets()`; `apps/_template` anchors controls inside it | ✅ done |
| Vulkan portability | `VK_KHR_portability_enumeration` + `_subset` opt-in in `VulkanContext` | ✅ done |
| Push-constant limit | sky push constant is 128 bytes (mobile floor) | ✅ done |
| Lighter frame graph | `RenderTier::Mobile` (`--mobile`): MSAA off, bloom blur passes skipped, SSAO off | ✅ done |
| Dynamic resolution | `render::DynamicResolution` — adaptive render-scale policy (frametime→scale, hysteresis); GPU target-resize is the on-device wire-up | ✅ policy done |
| Frame-rate cap / battery saver | `core::FramePacer` — target-fps sleep budget + lower idle cap (menu/pause), drift-corrected; the caller does the sleep | ✅ done |

The one thing every mobile backend must provide is `nativeWindowHandle()`; the rest of `PlatformBackend`
(caps, directories, lifecycle, and `safeAreaInsets()` from the OS) is filled exactly like `DesktopBackend`
(`engine/src/platform/DesktopBackend.cpp`).

---

## Android

**You need:** Android Studio + the **Android SDK & NDK** (`ANDROID_NDK_HOME` set), a device or emulator
with Vulkan, and (to publish) a Google Play developer account.

1. **Project skeleton.** Start from SDL3's Android project template (`SDL/android-project`) or a Gradle
   project with the NDK. Build Maz + your game as a shared library loaded by `SDLActivity`. SDL provides
   the `android_app`/`ANativeWindow` and pumps touch/lifecycle events into the same `SDL_Event` path
   `Window::pumpEvents` already handles — so touch "just works" once the window exists.
2. **`AndroidBackend : PlatformBackend`** (`engine/src/platform/AndroidBackend.cpp`, new):
   - `id()` → `PlatformId::Android`; `caps()` = `hasGpu/hasWindow/hasTouch/canSuspend = true`,
     `hasKeyboard/hasGamepad` as detected.
   - `nativeWindowHandle()` → the `ANativeWindow*` (from `SDL_GetWindowProperties` /
     `SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER`). This is what `SDL_Vulkan_CreateSurface` binds to — the
     renderer already creates the surface through SDL, so wiring the SDL window is usually enough.
   - `directory(DirKind::Assets)` → the APK **asset manager** path (bundled, read-only);
     `UserData/Cache/Temp` → the app's internal storage (`SDL_GetPrefPath` works on Android).
   - Lifecycle: forward `onPause`/`onResume` (or SDL's `SDL_EVENT_WILL_ENTER_BACKGROUND` /
     `SDL_EVENT_DID_ENTER_FOREGROUND`) to `transition(Suspended/Running)` — mirror
     `DesktopBackend::syncLifecycle`. On suspend, stop rendering and release the surface; recreate on resume.
   - Register it: `reg.registerBackend(PlatformId::Android, [] { return std::make_unique<AndroidBackend>(); });`
     in the Android build's registry (guard the desktop registration out on Android).
3. **Assets.** Package `assets/` (shaders staged into `bin/`, fonts, `.gltf`, `.pck`) into the APK's
   assets. Mount `res://` → the asset-manager root in the backend's `mountStandard`-equivalent. Because
   games do I/O through `res://`/`user://`, no game-code paths change.
4. **Render tier.** Set `RendererConfig::tier = RenderTier::Mobile` on Android (or pass `--mobile`) — MSAA
   off + no bloom/SSAO is the right default for mobile GPUs.
5. **Build + deploy.** `./gradlew assembleDebug`, then `adb install` to a device/emulator. Publishing:
   `assembleRelease`, sign, upload to Play.

**Verify:** the app launches, the virtual joystick/buttons drive the survivor (ZOMBOID), and
background/foreground pauses/resumes cleanly.

---

## iOS

**You need:** a **Mac with Xcode**, an **Apple Developer account** (to sign and deploy to a device), and
**MoltenVK** (Vulkan-on-Metal).

1. **Project skeleton.** Start from SDL3's iOS Xcode template. Link Maz + your game, and link **MoltenVK**
   (the `libMoltenVK` static/dynamic library or the Vulkan SDK's iOS package). MoltenVK provides a Vulkan
   1.1 ICD over Metal; the renderer's portability opt-in (already in `VulkanContext`) is what makes
   `vkEnumeratePhysicalDevices` return the Metal GPU and `vkCreateDevice` succeed.
2. **`IOSBackend : PlatformBackend`** (`engine/src/platform/IOSBackend.cpp`, new):
   - `id()` → `PlatformId::iOS`; `caps()` = `hasGpu/hasWindow/hasTouch/canSuspend = true`.
   - `nativeWindowHandle()` → the `CAMetalLayer` backing the `UIView` (via
     `SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER` → the view's layer). `SDL_Vulkan_CreateSurface` +
     MoltenVK turn that into a `VkSurfaceKHR`; again, wiring the SDL window generally suffices.
   - `directory(DirKind::Assets)` → the app bundle's resource path (read-only); `UserData/Cache/Temp` →
     the app sandbox (`SDL_GetPrefPath`).
   - Lifecycle: wire `applicationDidEnterBackground` / `applicationWillEnterForeground` (or SDL's
     background/foreground events) to `transition(Suspended/Running)`.
   - Register it under `PlatformId::iOS`.
3. **Assets.** Add `assets/` to the Xcode target as bundle resources; mount `res://` → the bundle path.
4. **Render tier.** Use `RenderTier::Mobile` on iOS as well.
5. **Build + deploy.** Select your team/signing in Xcode, build to a **physical device** (the Simulator's
   Metal/MoltenVK support is limited — prefer a real device), and run. Distribution: Archive → App Store
   Connect / TestFlight.

**Verify:** launches on device, MoltenVK reports a valid GPU (the log line
`Vulkan portability enumeration enabled` should appear), touch controls drive the game, and
lock/unlock suspends/resumes cleanly.

---

## Still out of scope (next phase, on your toolchain)

These are intentionally **not** done here because they require the Mac/Android toolchains:

- The concrete `AndroidBackend.cpp` / `IOSBackend.cpp` (skeletons specified above).
- Gradle / Xcode project generation and signing config.
- A mobile `io::BundlePlan` target and `.pck` packaging into the APK/app bundle.
- Any on-device or emulator run.

Everything the engine can do without those toolchains — touch, virtual controls, the callback loop, the
backend seam, the MoltenVK-safe renderer, and the mobile render tier — is done, tested, and on `main`.
