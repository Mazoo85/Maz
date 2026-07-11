#pragma once

#include <cstdint>

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

    // True when the window was created Vulkan-capable (i.e. a presentable surface is possible).
    // False in headless mode, where the renderer runs without presenting.
    bool supportsVulkan() const { return m_vulkanCapable; }

    // Enable relative mouse mode: the cursor is hidden/locked and motion arrives as deltas
    // (Input::mouseDX/DY) — the basis for first-person mouse-look.
    void setRelativeMouse(bool enabled);

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
};

} // namespace maz::platform
