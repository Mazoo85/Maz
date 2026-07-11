#include "maz/platform/Input.hpp"

#include <cstddef>

namespace maz::platform {

void Input::newFrame() {
    m_keysPrev = m_keys;
    m_mousePrev = m_mouse;
    m_mouseDX = 0.0f;
    m_mouseDY = 0.0f;
    m_wheel = 0.0f;
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

} // namespace maz::platform
