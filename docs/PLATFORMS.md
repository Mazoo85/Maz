# Platforms & Porting

Maz reaches a new platform by implementing **one backend** behind a single interface,
`maz::platform::PlatformBackend` (`engine/include/maz/platform/PlatformBackend.hpp`). The engine,
renderer, and games talk only to that interface, so a port never touches game code. Each backend
answers the same questions: how to boot/teardown, what native surface handle the GPU renderer binds
to, where the readable (assets) and writable (saves) directories are, what input exists, and whether
the OS can suspend/resume the app.

## Status of each target

| Platform | Backend | Status | What's left |
|---|---|---|---|
| **Headless** | `HeadlessBackend` | ✅ Implemented + unit-tested (`ctest -R platform_backend`) | — |
| **Desktop (Linux/Win/Mac)** | SDL3 + Vulkan | ✅ Shipping (the native path the samples/editor use) | — |
| **Web / WASM** | Emscripten + WebGL2 | ⚙️ Build path + main-loop done ([WEB_BUILD.md](WEB_BUILD.md)); needs Emscripten to emit `.wasm` | Install emsdk, run `tools/build_web.sh` |
| **Android** | `PlatformId::Android` | 🧱 **Engine-side foundation done** (touch + gestures, virtual controls, callback loop, the backend seam and its device seams — safe area, orientation, power/thermal, haptics, soft keyboard, network, motion sensors — autosave-on-suspend, frame pacer + dynamic resolution, MoltenVK-safe renderer + mobile tier, and bundle staging + preflight); backend `.cpp` + APK build not possible here | **Device step** — [MOBILE_BUILD.md](MOBILE_BUILD.md) |
| **iOS** | `PlatformId::iOS` | 🧱 **Engine-side foundation done** (same as Android; renderer is MoltenVK/portability-safe) | **Device step** — [MOBILE_BUILD.md](MOBILE_BUILD.md) |
| **VR (OpenXR)** | `PlatformId::VrOpenXR` | 🔩 Seam defined | **Human step** below |
| **Consoles** | `PlatformId::ConsoleA/B/C` | 🔩 Seam defined; SDKs are under NDA | **Human step** below |

These are deliberately **not** marked "100%": each needs hardware, a toolchain, and/or an account this
cloud environment cannot have. The engine-side seam is done and tested; the remaining work is a
per-platform backend that only you, on the right machine with the right access, can build and sign.

## Mobile foundation (delivered — desktop-verified)

The shared, engine-side work that makes Maz mobile-shaped is done and verified on this Linux box
(software Vulkan + `ctest`), so the remaining iOS/Android work is a clean, documented device build
([MOBILE_BUILD.md](MOBILE_BUILD.md)). What shipped:

- **Touch input** in the core input path — `platform::Input` tracks multi-touch contacts with the same
  edge detection keys/buttons use, fed from `Window::pumpEvents` via SDL finger events
  (`ctest -R platform_touch`).
- **On-screen virtual controls** — `input::VirtualControls` (twin-stick + buttons) turns touch into
  gamepad-shaped input, so a stick+buttons game plays with thumbs and no controller
  (`ctest -R input_virtualcontrols`).
- **Callback-driven main loop** — apps run through `platform::runMainLoop(step, user)` instead of a
  blocking `while`, which iOS/Android/web require (they own the loop). See `apps/_template/` for the
  copy-me starting point and ZOMBOID for the flagship.
- **`DesktopBackend` + VFS mounts** — the first real `PlatformBackend` fills the seam the future
  `AndroidBackend`/`IOSBackend` mirror (caps, native handle, `directory(DirKind)`, minimize→suspend
  lifecycle); on boot it mounts `res://`→assets and `user://`→save dir so packaging is a mount remap
  (`ctest -R platform_backend`).
- **MoltenVK/portability-safe renderer + a mobile render tier** — the Vulkan context opts into
  `VK_KHR_portability_enumeration`/`_subset` when present (no-op on desktop, required on iOS/macOS via
  Metal), the sky push constant fits the 128-byte mobile floor, and `RenderTier::Mobile` (the
  `--mobile` flag) forces MSAA off and drops bloom/SSAO for tilers (verified by the `scene3d_mobile`
  golden).
- **Device seams on the backend** — the `PlatformBackend` interface grew neutral-defaulted virtuals for
  everything a phone game reads, each paired with a pure-policy helper header and recorded by
  `HeadlessBackend` for unit tests: safe-area insets (`SafeArea.hpp` → `safeAreaRect`), screen
  orientation (`Orientation.hpp` → `orientedInsets`/`quarterTurnsFromPortrait`), power/thermal state
  (`PowerState.hpp` → `recommendedFps`/`powerBudgetScale`), haptics (`Haptics.hpp`), soft keyboard
  (`SoftKeyboard.hpp`), network reachability (`Network.hpp` → `isUnmeteredOnline`), and the motion
  sensors — accelerometer + gyroscope (`Motion.hpp` → `tiltVector`/`isShaking`/`deviceIsFlat`) for tilt
  steering and shake gestures. Desktop/headless are unaffected; a mobile backend overrides each with its
  OS call (`ctest -R platform_`).
- **Battery-aware loop + autosave** — `core::FramePacer` caps the frame rate and follows
  `recommendedFps(powerState())`; `render::DynamicResolution` scales GPU cost; the backend's
  `onSuspend`/`onResume` callbacks let a game persist on the lifecycle edge (a backgrounded app can be
  killed with no further notice). `apps/_template/` wires all of this as the copy-me showcase.
- **Bundle staging + preflight** — `tools/package_mobile.sh` + the `mobilepack` CLI drive
  `io::planMobileBundle` to stage a complete Android/iOS bundle (fat multi-ABI `lib/<abi>/`, optional
  single `game.pck`, generated `AndroidManifest.xml`/`Info.plist`, and on Android a ready-to-build
  `build.gradle`/`settings.gradle` via `io::androidBuildGradle`), and `io::preflightMobileBundle`
  validates the staged tree before Gradle/Xcode runs (`ctest -R "mobile_bundle|mobilepack|android_gradle"`).

## The exact human/hardware step per platform

### Android
1. Install the **Android SDK + NDK** and set `ANDROID_NDK_HOME`.
2. Implement an `AndroidBackend : PlatformBackend`: `nativeWindowHandle()` returns the `ANativeWindow*`
   from the `android_app`; `directory(Assets)` maps to the APK asset manager, `UserData` to the app's
   internal storage; `caps().hasTouch/canSuspend = true`; forward `onPause`/`onResume` to
   `transition(Suspended/Running)`. Fill the device seams from the OS: `safeAreaInsets()` from
   `WindowInsets`, `orientation()` from the display rotation, `powerState()` from `BatteryManager` +
   `PowerManager`, `reachability()` from `ConnectivityManager`, `motionState()` from `SensorManager`
   (TYPE_ACCELEROMETER + TYPE_GYROSCOPE), and route `triggerHaptic`/soft-keyboard calls to
   `Vibrator`/`InputMethodManager` — the engine side and its policy helpers are already done and tested;
   only these per-OS calls remain.
3. Build the Vulkan-for-Android surface, package an APK (Gradle), and deploy to a **physical device or
   emulator**. Publishing needs a **Google Play developer account**.

### iOS
1. Install **Xcode** on a Mac; you need an **Apple Developer account** to sign.
2. Implement an `IOSBackend`: `nativeWindowHandle()` → the `CAMetalLayer`/`UIView`; wire
   `applicationDidEnterBackground`/`willEnterForeground` to the lifecycle transitions. Fill the device
   seams from UIKit: `safeAreaInsets()` from `UIView.safeAreaInsets`, `orientation()` from
   `UIDevice.orientation`, `powerState()` from `UIDevice.batteryState`/`isLowPowerModeEnabled` +
   `ProcessInfo.thermalState`, `reachability()` from `NWPathMonitor`, `motionState()` from CoreMotion
   (`CMMotionManager` accelerometer + gyro), and route haptics to `UIFeedbackGenerator` and the soft
   keyboard to a `UITextField` first responder — again, only the per-OS calls; the engine side is done.
3. Build with MoltenVK (Vulkan-on-Metal), sign, and deploy to a **physical device** via Xcode.

### VR (OpenXR)
1. Install an **OpenXR runtime** (SteamVR, Meta, Monado) and connect a **physical headset**.
2. Implement a `VrBackend` with `caps().immersiveVr = true`: create the OpenXR session/swapchains, expose
   the per-eye views and head/hand poses, and drive stereo submission each frame.
3. Verified only wearing the headset.

### Consoles
1. Console SDKs are distributed under **NDA** by the platform holders — names, headers, and toolchains
   cannot live in this repo.
2. Once you have registered developer access, implement `ConsoleXBackend` against that SDK: its native
   surface, storage mounts, controller input, and mandatory suspend/resume + certification hooks.
3. Verified only on a devkit.

## Why the abstraction is enough

Because every subsystem already goes through `PlatformBackend` (surface handle, directories, input
caps, lifecycle), each of the above is a **self-contained backend file** — no engine changes. That is
the whole point of the seam, and it is why the headless backend and the registry that selects backends
are unit-tested here as the worked example the real ports follow.
