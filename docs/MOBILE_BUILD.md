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
| Touch-target sizing | `platform::TouchTarget` (`recommendedTouchTargetPx`/`meetsTouchTarget`/`expandToTouchTarget`/`dpiFromDiagonal`) sizes tappable controls to the ~9mm/48dp accessibility floor across any DPI | ✅ done |
| Control placement | `platform::ThumbZone` (`thumbControlLayout`/`bottomLeftZone`/`bottomRightZone`/`thumbReachRadius`) puts the move stick + action cluster in the natural bottom thumb zones inside the safe area (right/left-handed) | ✅ done |
| Screen orientation | `PlatformBackend::orientation()` + `platform::Orientation` (`isPortrait`/`quarterTurnsFromPortrait`/`orientedInsets`) re-derives safe-area insets per device rotation | ✅ done |
| Haptic feedback | `PlatformBackend::triggerHaptic(HapticFeedback)` (selection/impact/notification kinds) + `platform::Haptics` helpers; no-op on desktop, OS motor on mobile | ✅ seam done |
| Soft keyboard / text entry | `PlatformBackend::show/hideSoftKeyboard(SoftKeyboardType)` + `isSoftKeyboardVisible()`; tracks state on desktop, raises the OS IME on mobile | ✅ seam done |
| Network reachability | `PlatformBackend::reachability()` (Wifi/Cellular/Ethernet/Offline) + `platform::Network` (`isOnline`/`isMetered`/`isUnmeteredOnline`) to gate downloads off cellular data | ✅ seam done |
| Motion sensors | `PlatformBackend::motionState()` (accelerometer + gyroscope) + `platform::Motion` (`tiltVector`/`isShaking`/`deviceIsFlat`/`lowPassFilter`) for tilt steering + shake gestures | ✅ seam done |
| Vulkan portability | `VK_KHR_portability_enumeration` + `_subset` opt-in in `VulkanContext` | ✅ done |
| Push-constant limit | sky push constant is 128 bytes (mobile floor) | ✅ done |
| Lighter frame graph | `RenderTier::Mobile` (`--mobile`): MSAA off, bloom blur passes skipped, SSAO off | ✅ done |
| Dynamic resolution | `render::DynamicResolution` — adaptive render-scale policy (frametime→scale, hysteresis); GPU target-resize is the on-device wire-up | ✅ policy done |
| Frame-rate cap / battery saver | `core::FramePacer` — target-fps sleep budget + lower idle cap (menu/pause), drift-corrected; the caller does the sleep | ✅ done |
| Power/thermal awareness | `PlatformBackend::powerState()` + `platform::PowerState` helpers (`recommendsPowerSave`/`recommendedFps`/`powerBudgetScale`) drive FramePacer on low battery / low-power mode / thermal throttle | ✅ policy done |
| Bundle staging | `tools/package_mobile.sh` + the `mobilepack` CLI drive `io::planMobileBundle` to stage `dist/<app>-<ver>-<os>[-<abi>]/` (game `.so`/`.app`, assets, manifest) and verify it | ✅ done |
| Bundle preflight | `io::preflightMobileBundle` validates a staged tree against its plan (missing binary/manifest/files = errors; no shaders/`.pck` or stale files = warnings); runs as `mobilepack`'s final gate and standalone via `--preflight` | ✅ done |
| Multi-ABI (fat) Android | `mobilepack --abi arm64-v8a,armeabi-v7a,x86_64` stages a `lib/<abi>/` tree per ABI with assets + manifest written once | ✅ done |
| Packed assets (.pck) | `mobilepack --pack` bundles all shaders+assets into one `game.pck` (`io::ResourcePack`) instead of loose files, verified by round-trip | ✅ done |

The one thing every mobile backend must provide is `nativeWindowHandle()`; the rest of `PlatformBackend`
(caps, directories, lifecycle, and `safeAreaInsets()` from the OS) is filled exactly like `DesktopBackend`
(`engine/src/platform/DesktopBackend.cpp`).

---

## Using the mobile features in your game (quickstart)

Every API below is desktop-safe — the seams return neutral values and the helpers no-op on desktop, so you
write the code once and it comes alive on device. `apps/_template/main.cpp` is the full worked example; this
is the tour, in the order a frame touches them.

**1 — Boot: backend + file routing.** Pick the host backend, mount the schemes, and do all I/O through
`res://` (bundled, read-only) and `user://` (writable save dir) so packaging never changes a game path:

```cpp
auto backend = platform::defaultRegistry().create(platform::DesktopBackend::hostDesktopId());
io::VirtualFileSystem vfs;
if (auto* d = dynamic_cast<platform::DesktopBackend*>(backend.get())) { d->attachWindow(win.sdl()); d->init(); d->mountStandard(vfs); }
```

**2 — Layout inside the safe area, per orientation.** Anchor HUD/controls to the usable rectangle so nothing
lands under the notch or home bar; re-derive it when the device rotates:

```cpp
using namespace platform;
SafeAreaInsets insets = orientedInsets(backend->safeAreaInsets(), backend->orientation()); // Orientation.hpp
math::Rect2 safe = safeAreaRect(drawableW, drawableH, insets);                              // SafeArea.hpp
// place the stick/buttons within [safe.left()..safe.right()] × [safe.top()..safe.bottom()]
// size each button to a comfortable physical target for the display's DPI (TouchTarget.hpp):
float btnPx = recommendedTouchTargetPx(displayDpi);   // ~9mm/48dp floor; button = btnPx × btnPx
// or let ThumbZone.hpp place the whole control set in the natural bottom thumb zones for you:
TouchControlLayout ctl = thumbControlLayout(safe, /*stickPx*/220.f, /*clusterPx*/200.f, /*margin*/24.f);
// ctl.moveStick (bottom-left) and ctl.actionCluster (bottom-right); pass Handedness::LeftHanded to swap.
```

**3 — Input: touch = keyboard.** `input::VirtualControls` (sticks+buttons) fed by multi-touch, unified with
the keyboard; `input::GestureDetector` adds tap/double-tap/long-press/swipe and two-finger pinch/rotate/pan.
For tilt/shake controls, read the motion sensors:

```cpp
using namespace platform;
math::vec2 steer = tiltVector(backend->motionState());   // Motion.hpp — [-1,1] from device tilt
if (isShaking(backend->motionState())) { /* shake-to-reset */ }
```

**4 — Save the instant you're backgrounded.** A backgrounded mobile app can be killed with no further
notice, so persist on the lifecycle edge — not on a timer:

```cpp
backend->setOnSuspend([&]{ save.set("x", posX); save.save(); });   // fires on didEnterBackground / onPause
backend->setOnResume([]{ /* reacquire anything released */ });
```

**5 — Don't cook the battery.** Cap the loop with `core::FramePacer`, and let the cap follow the
power/thermal state; scale GPU cost with `render::DynamicResolution` and the `--mobile` render tier:

```cpp
core::FramePacer pacer;
pacer.setActiveFps(platform::recommendedFps(backend->powerState()));  // 60 → 30 on low battery / thermal
double sleepS = pacer.sleepFor(frameWorkSeconds);                     // you perform the sleep
float budget = platform::powerBudgetScale(backend->powerState());     // 0..1 for particle/shadow counts
```

**6 — Game feel.** `backend->triggerHaptic(platform::HapticFeedback::ImpactMedium);` on a hit/confirm.

**7 — Text entry.** `backend->showSoftKeyboard(platform::SoftKeyboardType::Email);` when a field focuses,
`hideSoftKeyboard()` when it blurs; gate UI on `isSoftKeyboardVisible()`.

**8 — Respect the data plan.** Before a big download: `if (platform::isUnmeteredOnline(backend->reachability()))`
— true only on Wi-Fi/Ethernet, so cellular players aren't charged for it (fails safe when unknown).

**9 — Package.** `tools/package_mobile.sh <app> <ver> --os android --abi arm64-v8a,armeabi-v7a --pack`
stages the bundle (fat `lib/<abi>/`, one `game.pck`, generated manifest) and verifies it; hand that tree to
Gradle/Xcode below.

**10 — Preflight (before you hand it off).** `tools/package_mobile.sh <app> <ver> --os android --abi
arm64-v8a,armeabi-v7a --preflight` re-validates an already-staged tree non-destructively — the game binary
and manifest are present, every planned file landed, resources ship (loose `.spv` or a `.pck`), and no stale
leftovers linger. It's the same `io::preflightMobileBundle` gate `mobilepack` runs at the end of staging, but
callable on its own so you can re-check a tree a Gradle step (or a manual edit) has since touched. Errors mean
"won't run" and exit non-zero; warnings are advisory.

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

- The concrete `AndroidBackend.cpp` / `IOSBackend.cpp` (skeletons specified above), including wiring the OS
  values into the seams (`safeAreaInsets` / `orientation` / `powerState` / `reachability` / `onHaptic` /
  `onSoftKeyboard`) — the engine side is done and tested; only the per-OS calls remain.
- Gradle / Xcode project generation and signing config.
- Assembling the staged tree (`tools/package_mobile.sh`, which already does multi-ABI + `.pck` packing) into
  a signed `.apk`/`.ipa` — that final packaging step needs Gradle+NDK / Xcode.
- Any on-device or emulator run.

Everything the engine can do without those toolchains — touch + gestures, virtual controls, the callback
loop, the backend seam and its device seams (safe area, orientation, power, haptics, soft keyboard, network),
autosave-on-suspend, the frame pacer + dynamic resolution, the MoltenVK-safe renderer + mobile render tier,
and the bundle staging (`mobilepack`) — is done, tested (500+ ctests), and on `main`.
