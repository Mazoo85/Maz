// tests/platform/softkeyboard.cpp — verifies the on-screen keyboard seam: PlatformBackend::showSoftKeyboard /
// hideSoftKeyboard / isSoftKeyboardVisible / softKeyboardType (dispatching to onSoftKeyboard, recorded by
// HeadlessBackend; a logical no-op on DesktopBackend) plus the softKeyboardTypeName() helper. Pure/headless —
// raising a real IME is the mobile backend's job.
#include "maz/platform/SoftKeyboard.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

int main() {
    // --- 1. Show/hide tracks visibility + type; HeadlessBackend counts show requests. ---
    {
        HeadlessBackend hb;
        CHECK(!hb.isSoftKeyboardVisible(), "keyboard starts hidden");
        CHECK(hb.softKeyboardShows() == 0, "no shows yet");

        hb.showSoftKeyboard(SoftKeyboardType::Email);
        CHECK(hb.isSoftKeyboardVisible(), "show makes it visible");
        CHECK(hb.softKeyboardType() == SoftKeyboardType::Email, "the requested type is recorded");
        CHECK(hb.softKeyboardShows() == 1, "show dispatched to onSoftKeyboard (counted)");

        hb.hideSoftKeyboard();
        CHECK(!hb.isSoftKeyboardVisible(), "hide makes it hidden");
        CHECK(hb.softKeyboardShows() == 1, "hide is not counted as a show");
        CHECK(hb.softKeyboardType() == SoftKeyboardType::Email, "the last type persists after hide");

        hb.showSoftKeyboard(SoftKeyboardType::Number);
        CHECK(hb.softKeyboardShows() == 2 && hb.softKeyboardType() == SoftKeyboardType::Number,
              "a second show counts and updates the type");
    }

    // --- 2. Default type when none specified. ---
    {
        HeadlessBackend hb;
        hb.showSoftKeyboard();
        CHECK(hb.softKeyboardType() == SoftKeyboardType::Default, "show() defaults to the Default layout");
    }

    // --- 3. DesktopBackend: the seam still tracks the logical state (hardware keyboard, no OS IME raised). ---
    {
        DesktopBackend db("MazEngine", "SoftKbTest");
        CHECK(!db.isSoftKeyboardVisible(), "desktop keyboard starts hidden");
        db.showSoftKeyboard(SoftKeyboardType::Url);
        CHECK(db.isSoftKeyboardVisible() && db.softKeyboardType() == SoftKeyboardType::Url,
              "desktop tracks the requested state uniformly");
        db.hideSoftKeyboard();
        CHECK(!db.isSoftKeyboardVisible(), "desktop hide clears the flag");
    }

    // --- 4. Type name strings. ---
    {
        CHECK(std::strcmp(softKeyboardTypeName(SoftKeyboardType::Default), "default") == 0, "default name");
        CHECK(std::strcmp(softKeyboardTypeName(SoftKeyboardType::Number), "number") == 0, "number name");
        CHECK(std::strcmp(softKeyboardTypeName(SoftKeyboardType::Email), "email") == 0, "email name");
        CHECK(std::strcmp(softKeyboardTypeName(SoftKeyboardType::Phone), "phone") == 0, "phone name");
        CHECK(std::strcmp(softKeyboardTypeName(SoftKeyboardType::Url), "url") == 0, "url name");
    }

    if (g_fail == 0) {
        std::printf("soft_keyboard: OK — show/hide state, type, headless count, desktop tracking, names.\n");
        return 0;
    }
    std::printf("soft_keyboard: %d failure(s).\n", g_fail);
    return 1;
}
