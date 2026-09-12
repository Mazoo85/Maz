#pragma once

#include <string>

// maz::platform system clipboard — read/write the OS clipboard as UTF-8 text (SDL3-backed). This is
// what lets a text field do copy/cut/paste against other applications (Godot's DisplayServer
// clipboard_set/get). Thin by nature; implemented in Window.cpp where SDL is already linked. Safe
// to call before/without a window (returns empty / does nothing) so headless code never crashes.
namespace maz::platform {

// The clipboard's current text (empty if none / unavailable).
std::string clipboardText();

// Replace the clipboard contents with `text`. Returns false if the platform rejected it.
bool setClipboardText(const std::string& text);

// True when the clipboard currently holds non-empty text.
bool hasClipboardText();

} // namespace maz::platform
