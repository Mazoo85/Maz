#pragma once

#include "maz/platform/AppFocus.hpp"
#include "maz/platform/Displays.hpp"

#include <cstdint>
#include <vector>

struct SDL_Window;
struct SDL_Gamepad;

namespace maz::platform {

class Input;

struct WindowConfig {
    const char* title = "Maz Engine";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool headless = false;   // use SDL's dummy video driver; no visible window
    bool resizable = true;
    bool fullscreen = false; // start in borderless-desktop fullscreen
    bool highDpi = true;     // request a HiDPI-aware pixel-dense surface where available
};

// Owns the SDL window + event pump. Vulkan-ready (created with SDL_WINDOW_VULKAN).
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool init(const WindowConfig& cfg);
    void shutdown();

    // Pump SDL events into `input`, updating close/resize state. Call once per frame.
    void pumpEvents(Input& input);

    bool shouldClose() const { return m_shouldClose; }
    void requestClose() { m_shouldClose = true; }

    // Pixel size of the drawable area (may differ from logical size on HiDPI).
    void drawableSize(uint32_t& w, uint32_t& h) const;
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    // True on the frame after a resize; renderer polls + clears this to rebuild the swapchain.
    bool consumeResized();

    // Window activation, tracked from SDL focus/minimize events. Feed activation() into
    // platform::decideFrame(...) with a FocusPolicy to throttle/pause when the window is in the
    // background. Defaults (focused, not minimized) mean full speed until an event says otherwise.
    WindowActivation activation() const { return {m_focused, m_minimized}; }
    bool isFocused() const { return m_focused; }
    bool isMinimized() const { return m_minimized; }

    // True when the window was created Vulkan-capable (i.e. a presentable surface is possible).
    // False in headless mode, where the renderer runs without presenting.
    bool supportsVulkan() const { return m_vulkanCapable; }

    // Enable relative mouse mode: the cursor is hidden/locked and motion arrives as deltas
    // (Input::mouseDX/DY) — the basis for first-person mouse-look.
    void setRelativeMouse(bool enabled);

    // Borderless-desktop fullscreen toggle (SDL3). No-op headless. isFullscreen() reflects the
    // last requested state; toggleFullscreen() flips it.
    void setFullscreen(bool enabled);
    void toggleFullscreen() { setFullscreen(!m_fullscreen); }
    bool isFullscreen() const { return m_fullscreen; }

    // The display's content scale (HiDPI factor): 1.0 on a standard display, 2.0 on Retina, etc.
    // Feed into platform::logicalToPixels / scaledSize for resolution-independent UI. Returns 1.0
    // headless or before the window exists.
    float contentScale() const;

    // Live multi-monitor enumeration (SDL3). displays() returns each connected monitor's virtual-
    // desktop bounds + content scale; currentDisplay() is the index of the monitor this window is
    // most on. Feed displays() into platform::displayForRect / centerRectOnDisplay to place windows.
    // Empty / -1 headless.
    std::vector<DisplayInfo> displays() const;
    int currentDisplay() const;

    SDL_Window* sdl() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;
    SDL_Gamepad* m_gamepad = nullptr; // first connected controller, or null
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_shouldClose = false;
    bool m_resized = false;
    bool m_ownsSdl = false;
    bool m_vulkanCapable = false;
    bool m_focused = true;    // updated by SDL focus gained/lost events
    bool m_minimized = false; // updated by SDL minimize/restore events
    bool m_fullscreen = false; // last requested fullscreen state
};

} // namespace maz::platform
