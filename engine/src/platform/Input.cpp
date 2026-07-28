#include "maz/platform/Input.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace maz::platform {

void Input::newFrame() {
    m_keysPrev = m_keys;
    m_mousePrev = m_mouse;
    m_buttonsPrev = m_buttons;
    m_mouseDX = 0.0f;
    m_mouseDY = 0.0f;
    m_wheel = 0.0f;
    m_textInput.clear();
    m_droppedFiles.clear();

    // Touch edge detection: remember which contacts were down entering this frame, clear last frame's
    // releases, and zero the per-contact motion deltas so Move events accumulate cleanly.
    m_touchPrevIds.clear();
    m_touchPrevIds.reserve(m_touches.size());
    for (const auto& t : m_touches) {
        m_touchPrevIds.push_back(t.id);
    }
    m_touchReleased.clear();
    for (auto& t : m_touches) {
        t.dx = 0.0f;
        t.dy = 0.0f;
    }
}

void Input::onTextInput(const char* utf8) {
    if (utf8) {
        m_textInput += utf8;
    }
}

void Input::onDropFile(const char* path) {
    if (path) {
        m_droppedFiles.emplace_back(path);
    }
}

void Input::onKey(int scancode, bool down) {
    if (scancode >= 0 && scancode < kMaxScancodes) {
        m_keys[static_cast<size_t>(scancode)] = down;
    }
}

void Input::onMouseButton(int button, bool down) {
    if (button >= 0 && button < kMaxMouseButtons) {
        m_mouse[static_cast<size_t>(button)] = down;
    }
}

void Input::onMouseMotion(float x, float y, float dx, float dy) {
    m_mouseX = x;
    m_mouseY = y;
    m_mouseDX += dx;
    m_mouseDY += dy;
}

void Input::onMouseWheel(float dy) { m_wheel += dy; }

void Input::onTouch(int64_t id, float x, float y, float dx, float dy, TouchPhase phase) {
    const auto it =
        std::find_if(m_touches.begin(), m_touches.end(), [id](const Touch& t) { return t.id == id; });
    switch (phase) {
    case TouchPhase::Down:
        if (it == m_touches.end()) {
            if (static_cast<int>(m_touches.size()) >= kMaxTouches) {
                return; // ignore contacts beyond the tracked maximum
            }
            m_touches.push_back(Touch{id, x, y, 0.0f, 0.0f});
        } else {
            it->x = x;
            it->y = y;
        }
        break;
    case TouchPhase::Move:
        if (it == m_touches.end()) {
            // Motion for a finger we never saw go down (rare, e.g. focus change) — start tracking it.
            if (static_cast<int>(m_touches.size()) >= kMaxTouches) {
                return;
            }
            m_touches.push_back(Touch{id, x, y, dx, dy});
        } else {
            it->x = x;
            it->y = y;
            it->dx += dx;
            it->dy += dy;
        }
        break;
    case TouchPhase::Up:
        if (it != m_touches.end()) {
            Touch done = *it;
            done.x = x;
            done.y = y;
            m_touchReleased.push_back(done);
            m_touches.erase(it);
        } else {
            m_touchReleased.push_back(Touch{id, x, y, 0.0f, 0.0f});
        }
        break;
    }
}

const Touch& Input::touch(int i) const {
    static const Touch kNone{};
    if (i < 0 || i >= static_cast<int>(m_touches.size())) {
        return kNone;
    }
    return m_touches[static_cast<size_t>(i)];
}

const Touch* Input::touchById(int64_t id) const {
    for (const auto& t : m_touches) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

bool Input::touchPressed(int i) const {
    if (i < 0 || i >= static_cast<int>(m_touches.size())) {
        return false;
    }
    const int64_t id = m_touches[static_cast<size_t>(i)].id;
    for (const int64_t prev : m_touchPrevIds) {
        if (prev == id) {
            return false; // was already down last frame
        }
    }
    return true;
}

bool Input::keyDown(int scancode) const {
    return scancode >= 0 && scancode < kMaxScancodes && m_keys[static_cast<size_t>(scancode)];
}

bool Input::keyPressed(int scancode) const {
    if (scancode < 0 || scancode >= kMaxScancodes) {
        return false;
    }
    const auto s = static_cast<size_t>(scancode);
    return m_keys[s] && !m_keysPrev[s];
}

bool Input::keyReleased(int scancode) const {
    if (scancode < 0 || scancode >= kMaxScancodes) {
        return false;
    }
    const auto s = static_cast<size_t>(scancode);
    return !m_keys[s] && m_keysPrev[s];
}

bool Input::mouseDown(int button) const {
    return button >= 0 && button < kMaxMouseButtons && m_mouse[static_cast<size_t>(button)];
}

bool Input::mousePressed(int button) const {
    if (button < 0 || button >= kMaxMouseButtons) {
        return false;
    }
    const auto b = static_cast<size_t>(button);
    return m_mouse[b] && !m_mousePrev[b];
}

void Input::onGamepadState(bool connected, const float* axes, const bool* buttons) {
    m_padConnected = connected;
    if (!connected) {
        m_axes.fill(0.0f);
        m_buttons.fill(false);
        return;
    }
    for (int i = 0; i < pad::AxisCount; ++i) {
        m_axes[static_cast<size_t>(i)] = axes[i];
    }
    for (int i = 0; i < pad::ButtonCount; ++i) {
        m_buttons[static_cast<size_t>(i)] = buttons[i];
    }
}

float Input::gamepadAxis(int axis) const {
    if (axis < 0 || axis >= pad::AxisCount) {
        return 0.0f;
    }
    const float v = m_axes[static_cast<size_t>(axis)];
    // Triggers rest at 0 and need no deadzone; sticks get a small dead center.
    if (axis == pad::LeftTrigger || axis == pad::RightTrigger) {
        return v;
    }
    const float dead = 0.18f;
    if (std::fabs(v) < dead) {
        return 0.0f;
    }
    // Rescale so motion is continuous just past the deadzone.
    const float sign = v < 0.0f ? -1.0f : 1.0f;
    return sign * (std::fabs(v) - dead) / (1.0f - dead);
}

bool Input::gamepadButtonDown(int button) const {
    return button >= 0 && button < pad::ButtonCount && m_buttons[static_cast<size_t>(button)];
}

bool Input::gamepadButtonPressed(int button) const {
    if (button < 0 || button >= pad::ButtonCount) {
        return false;
    }
    const auto b = static_cast<size_t>(button);
    return m_buttons[b] && !m_buttonsPrev[b];
}

} // namespace maz::platform
