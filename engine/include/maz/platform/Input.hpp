#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace maz::platform {

// Gamepad axis/button ids. Values match SDL_GamepadAxis / SDL_GamepadButton ordering, so gameplay
// code can name inputs semantically without including SDL headers.
namespace pad {
enum Axis { LeftX = 0, LeftY = 1, RightX = 2, RightY = 3, LeftTrigger = 4, RightTrigger = 5, AxisCount = 6 };
enum Button {
    A = 0, B = 1, X = 2, Y = 3, Back = 4, Guide = 5, Start = 6, LeftStick = 7, RightStick = 8,
    LeftShoulder = 9, RightShoulder = 10, DpadUp = 11, DpadDown = 12, DpadLeft = 13, DpadRight = 14,
    ButtonCount = 15
};
} // namespace pad

// Keyboard + mouse + gamepad state with edge detection. Scancodes are SDL scancodes
// (SDL_SCANCODE_*), kept as plain ints here so gameplay code needn't include SDL headers.
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
    // Gamepad state for the frame: `connected`, 6 axes in [-1,1] (triggers [0,1]), 15 buttons.
    void onGamepadState(bool connected, const float* axes, const bool* buttons);
    // UTF-8 text committed this frame (from OS text input / IME) — appended, cleared each newFrame.
    void onTextInput(const char* utf8);
    // A file dropped onto the window this frame — accumulated, cleared each newFrame.
    void onDropFile(const char* path);

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

    // --- gamepad (first connected controller) ---
    bool gamepadConnected() const { return m_padConnected; }
    // Axis value with a radial-ish per-axis deadzone applied. `axis` is a pad::Axis.
    float gamepadAxis(int axis) const;
    bool gamepadButtonDown(int button) const;
    bool gamepadButtonPressed(int button) const; // just went down this frame

    // --- text input + drag-and-drop (this frame; cleared by newFrame) ---
    // UTF-8 characters typed this frame — feed into a focused text field. Empty when nothing typed.
    const std::string& textInput() const { return m_textInput; }
    // File paths dropped onto the window this frame. Empty when nothing was dropped.
    const std::vector<std::string>& droppedFiles() const { return m_droppedFiles; }

private:
    std::array<bool, kMaxScancodes> m_keys{};
    std::array<bool, kMaxScancodes> m_keysPrev{};
    std::array<bool, kMaxMouseButtons> m_mouse{};
    std::array<bool, kMaxMouseButtons> m_mousePrev{};
    float m_mouseX = 0.0f, m_mouseY = 0.0f;
    float m_mouseDX = 0.0f, m_mouseDY = 0.0f;
    float m_wheel = 0.0f;

    bool m_padConnected = false;
    std::array<float, pad::AxisCount> m_axes{};
    std::array<bool, pad::ButtonCount> m_buttons{};
    std::array<bool, pad::ButtonCount> m_buttonsPrev{};

    std::string m_textInput;                  // UTF-8 typed this frame
    std::vector<std::string> m_droppedFiles;  // files dropped this frame
};

} // namespace maz::platform
