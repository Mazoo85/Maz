#include "maz/platform/Window.hpp"

#include "maz/core/Log.hpp"
#include "maz/platform/Clipboard.hpp"
#include "maz/platform/Input.hpp"

#include <SDL3/SDL.h>

namespace maz::platform {

Window::~Window() { shutdown(); }

bool Window::init(const WindowConfig& cfg) {
    // Headless: force SDL's dummy video driver so a window can be created with no display
    // (CI containers). The dummy driver cannot present Vulkan, which the renderer handles.
    if (cfg.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
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
    if (cfg.highDpi && !cfg.headless) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY; // pixel-dense surface on HiDPI displays
    }
    if (cfg.fullscreen && !cfg.headless) {
        flags |= SDL_WINDOW_FULLSCREEN; // borderless-desktop fullscreen
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
    m_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    MAZ_LOG_INFO("window created %ux%u (driver: %s%s)", m_width, m_height,
                 SDL_GetCurrentVideoDriver(), cfg.headless ? ", headless" : "");
    return true;
}

void Window::shutdown() {
    if (m_gamepad) {
        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
    }
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
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            m_focused = true;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            m_focused = false;
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            m_minimized = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
            m_minimized = false;
            break;
        case SDL_EVENT_TEXT_INPUT:
            input.onTextInput(e.text.text);
            break;
        case SDL_EVENT_DROP_FILE:
            input.onDropFile(e.drop.data);
            break;
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
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!m_gamepad) {
                m_gamepad = SDL_OpenGamepad(e.gdevice.which);
                if (m_gamepad) {
                    MAZ_LOG_INFO("gamepad connected: %s",
                                 SDL_GetGamepadName(m_gamepad) ? SDL_GetGamepadName(m_gamepad) : "?");
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (m_gamepad &&
                e.gdevice.which == SDL_GetGamepadID(m_gamepad)) {
                SDL_CloseGamepad(m_gamepad);
                m_gamepad = nullptr;
            }
            break;
        default:
            break;
        }
    }

    // Snapshot the gamepad state for this frame (SDL axes are Sint16; normalize to [-1,1]).
    if (m_gamepad) {
        float axes[pad::AxisCount];
        for (int i = 0; i < pad::AxisCount; ++i) {
            const Sint16 raw = SDL_GetGamepadAxis(m_gamepad, static_cast<SDL_GamepadAxis>(i));
            axes[i] = static_cast<float>(raw) / 32767.0f;
        }
        bool buttons[pad::ButtonCount];
        for (int i = 0; i < pad::ButtonCount; ++i) {
            buttons[i] = SDL_GetGamepadButton(m_gamepad, static_cast<SDL_GamepadButton>(i));
        }
        input.onGamepadState(true, axes, buttons);
    } else {
        input.onGamepadState(false, nullptr, nullptr);
    }
}

void Window::setRelativeMouse(bool enabled) {
    if (m_window) {
        SDL_SetWindowRelativeMouseMode(m_window, enabled);
    }
}

void Window::setTextInputActive(bool active) {
    if (!m_window) {
        return;
    }
    if (active) {
        SDL_StartTextInput(m_window);
    } else {
        SDL_StopTextInput(m_window);
    }
}

void Window::setFullscreen(bool enabled) {
    m_fullscreen = enabled;
    if (m_window && m_vulkanCapable) { // dummy/headless windows can't go fullscreen
        SDL_SetWindowFullscreen(m_window, enabled);
    }
}

float Window::contentScale() const {
    if (m_window) {
        const float s = SDL_GetWindowDisplayScale(m_window);
        if (s > 0.0f) {
            return s;
        }
    }
    return 1.0f;
}

std::vector<DisplayInfo> Window::displays() const {
    std::vector<DisplayInfo> out;
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids) {
        return out;
    }
    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        SDL_Rect bounds{};
        DisplayInfo d;
        d.index = i;
        if (SDL_GetDisplayBounds(ids[i], &bounds)) {
            d.x = bounds.x;
            d.y = bounds.y;
            d.w = bounds.w;
            d.h = bounds.h;
        }
        const float s = SDL_GetDisplayContentScale(ids[i]);
        d.scale = s > 0.0f ? s : 1.0f;
        out.push_back(d);
    }
    SDL_free(ids);
    return out;
}

int Window::currentDisplay() const {
    if (!m_window) {
        return -1;
    }
    const SDL_DisplayID here = SDL_GetDisplayForWindow(m_window);
    if (here == 0) {
        return -1;
    }
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids) {
        return -1;
    }
    int result = -1;
    for (int i = 0; i < count; ++i) {
        if (ids[i] == here) {
            result = i;
            break;
        }
    }
    SDL_free(ids);
    return result;
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

std::string clipboardText() {
    char* text = SDL_GetClipboardText(); // never null; "" when empty
    std::string out = text ? text : "";
    SDL_free(text);
    return out;
}

bool setClipboardText(const std::string& text) {
    return SDL_SetClipboardText(text.c_str());
}

bool hasClipboardText() {
    return SDL_HasClipboardText();
}

} // namespace maz::platform
