#pragma once

#include <array>
#include <cstdint>

namespace maz::platform {

// Keyboard + mouse state with edge detection. Scancodes are SDL scancodes (SDL_SCANCODE_*),
// kept as plain ints here so gameplay code needn't include SDL headers.
class Input {
public:
    static constexpr int kMaxScancodes = 512;
    static constexpr int kMaxMouseButtons = 8;

    // Called by Window at the top of each frame before events are pumped.
    void newFrame();

    // --- event feed (called by Window) ---
    void onKey(int scancode, bool down);
    void onMouseButton(int button, bool down);
    void onMouseMotion(float x, float y, float dx, float dy);
    void onMouseWheel(float dy);

    // --- queries (called by gameplay) ---
    bool keyDown(int scancode) const;
    bool keyPressed(int scancode) const;   // just went down this frame
    bool keyReleased(int scancode) const;  // just went up this frame

    bool mouseDown(int button) const;
    bool mousePressed(int button) const;

    float mouseX() const { return m_mouseX; }
    float mouseY() const { return m_mouseY; }
    float mouseDX() const { return m_mouseDX; }
    float mouseDY() const { return m_mouseDY; }
    float wheel() const { return m_wheel; }

private:
    std::array<bool, kMaxScancodes> m_keys{};
    std::array<bool, kMaxScancodes> m_keysPrev{};
    std::array<bool, kMaxMouseButtons> m_mouse{};
    std::array<bool, kMaxMouseButtons> m_mousePrev{};
    float m_mouseX = 0.0f, m_mouseY = 0.0f;
    float m_mouseDX = 0.0f, m_mouseDY = 0.0f;
    float m_wheel = 0.0f;
};

} // namespace maz::platform
