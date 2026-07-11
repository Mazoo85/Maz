#include "maz/platform/Window.hpp"

#include "maz/core/Log.hpp"
#include "maz/platform/Input.hpp"

#include <SDL3/SDL.h>

namespace maz::platform {

using core::LogLevel;

Window::~Window() { shutdown(); }

bool Window::init(const WindowConfig& cfg) {
    // Headless: force SDL's dummy video driver so a window can be created with no display
    // (CI containers). The dummy driver cannot present Vulkan, which the renderer handles.
    if (cfg.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        MAZ_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    m_ownsSdl = true;

    SDL_WindowFlags flags = 0;
    if (!cfg.headless) {
        flags |= SDL_WINDOW_VULKAN;   // dummy driver rejects this flag
    }
    if (cfg.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    m_window = SDL_CreateWindow(cfg.title, static_cast<int>(cfg.width),
                                static_cast<int>(cfg.height), flags);
    if (!m_window) {
        MAZ_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        shutdown();
        return false;
    }

    m_width = cfg.width;
    m_height = cfg.height;
    m_vulkanCapable = (flags & SDL_WINDOW_VULKAN) != 0;
    MAZ_LOG_INFO("window created %ux%u (driver: %s%s)", m_width, m_height,
                 SDL_GetCurrentVideoDriver(), cfg.headless ? ", headless" : "");
    return true;
}

void Window::shutdown() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_ownsSdl) {
        SDL_Quit();
        m_ownsSdl = false;
    }
}

void Window::pumpEvents(Input& input) {
    input.newFrame();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            m_shouldClose = true;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            m_shouldClose = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED: {
            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(m_window, &w, &h);
            m_width = static_cast<uint32_t>(w);
            m_height = static_cast<uint32_t>(h);
            m_resized = true;
            break;
        }
        case SDL_EVENT_KEY_DOWN:
            input.onKey(static_cast<int>(e.key.scancode), true);
            break;
        case SDL_EVENT_KEY_UP:
            input.onKey(static_cast<int>(e.key.scancode), false);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            input.onMouseButton(e.button.button, true);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            input.onMouseButton(e.button.button, false);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.onMouseMotion(e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            input.onMouseWheel(e.wheel.y);
            break;
        default:
            break;
        }
    }
}

void Window::setRelativeMouse(bool enabled) {
    if (m_window) {
        SDL_SetWindowRelativeMouseMode(m_window, enabled);
    }
}

void Window::drawableSize(uint32_t& w, uint32_t& h) const {
    int pw = 0, ph = 0;
    if (m_window) {
        SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
    }
    w = static_cast<uint32_t>(pw);
    h = static_cast<uint32_t>(ph);
}

bool Window::consumeResized() {
    bool r = m_resized;
    m_resized = false;
    return r;
}

} // namespace maz::platform
