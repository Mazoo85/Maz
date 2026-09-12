// tests/core/appconfig.cpp — verifies core::parseArgs, the command-line parser every one of the 161
// apps under apps/ is started through, and which nothing exercised. It is what turns `--headless`
// into a run that opens no window and `--frames N` into a run that ends, so CI depends on it before
// it depends on anything else: if this silently stopped recognising a flag, every headless run would
// quietly become an interactive one that never exits, and the whole suite would hang rather than
// fail. Deterministic CPU: no GPU, no window.
#include "maz/core/Config.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::AppConfig;
using maz::core::parseArgs;

// parseArgs takes argv as the OS hands it over: argv[0] is the program name.
static AppConfig parse(std::vector<const char*> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("app"));
    for (const char* a : args) argv.push_back(const_cast<char*>(a));
    return parseArgs(static_cast<int>(argv.size()), argv.data());
}

int main() {
    // --- 1. No arguments: a windowed, vsynced, endless run. ---
    {
        const AppConfig c = parse({});
        CHECK(!c.headless, "defaults to a visible window");
        CHECK(c.vsync, "defaults to vsync on");
        CHECK(!c.demo, "defaults to being driven by a person");
        CHECK(!c.mobile, "defaults to the desktop render tier");
        CHECK(c.frames < 0, "defaults to running until closed");
        CHECK(c.width > 0 && c.height > 0, "defaults to a real window size");
    }

    // --- 2. Each flag on its own. ---
    {
        CHECK(parse({"--headless"}).headless, "--headless");
        CHECK(!parse({"--no-vsync"}).vsync, "--no-vsync");
        CHECK(parse({"--vsync"}).vsync, "--vsync");
        CHECK(parse({"--demo"}).demo, "--demo");
        CHECK(parse({"--mobile"}).mobile, "--mobile");
        CHECK(parse({"--frames", "42"}).frames == 42, "--frames N");
        CHECK(parse({"--width", "800"}).width == 800u, "--width N");
        CHECK(parse({"--height", "600"}).height == 600u, "--height N");
    }

    // --- 3. A headless run always gets a frame cap, or CI hangs forever. ---
    //     This is the single most load-bearing line in the parser: without it
    //     `--headless` with no `--frames` renders until something kills it, and
    //     a test suite that hangs is worse than one that fails.
    {
        const AppConfig c = parse({"--headless"});
        CHECK(c.frames > 0, "--headless alone gets a default frame cap so the run ends");
    }

    // --- 4. An explicit cap is not overwritten by that default. ---
    {
        CHECK(parse({"--headless", "--frames", "3"}).frames == 3, "an explicit cap wins");
        CHECK(parse({"--frames", "3", "--headless"}).frames == 3, "in either order");
    }

    // --- 5. Flags compose, and order does not matter. ---
    {
        const AppConfig a = parse({"--headless", "--demo", "--no-vsync", "--frames", "7", "--mobile"});
        const AppConfig b = parse({"--mobile", "--frames", "7", "--no-vsync", "--demo", "--headless"});
        CHECK(a.headless && a.demo && !a.vsync && a.mobile && a.frames == 7, "every flag took effect");
        CHECK(b.headless == a.headless && b.demo == a.demo && b.vsync == a.vsync &&
              b.mobile == a.mobile && b.frames == a.frames, "order does not change the result");
    }

    // --- 6. The last of a repeated flag wins. ---
    {
        CHECK(parse({"--vsync", "--no-vsync"}).vsync == false, "--no-vsync after --vsync");
        CHECK(parse({"--no-vsync", "--vsync"}).vsync == true, "--vsync after --no-vsync");
        CHECK(parse({"--frames", "5", "--frames", "9"}).frames == 9, "the later frame cap wins");
    }

    // --- 7. Unknown arguments are ignored, not fatal, and do not eat the next one. ---
    {
        const AppConfig c = parse({"--nonsense", "--headless", "--frames", "4"});
        CHECK(c.headless && c.frames == 4, "an unknown flag does not swallow what follows it");
    }

    // --- 8. A value flag at the very end, with nothing after it, is survivable. ---
    //     Someone types `app --frames` and presses enter. It must not read past
    //     the end of argv.
    {
        const AppConfig a = parse({"--frames"});
        CHECK(a.frames < 0, "a trailing --frames with no number leaves the default");
        const AppConfig b = parse({"--headless", "--width"});
        CHECK(b.headless, "and the flags before it still took effect");
    }

    // --- 9. argv[0] is the program name and is never read as a flag. ---
    {
        char* argv[] = {const_cast<char*>("--headless")};   // the program is literally named this
        const AppConfig c = parseArgs(1, argv);
        CHECK(!c.headless, "argv[0] is the program name, not an argument");
    }

    // --- 10. Nothing at all, not even a program name. ---
    {
        const AppConfig c = parseArgs(0, nullptr);
        CHECK(!c.headless && c.frames < 0, "an empty argv yields the defaults rather than a crash");
    }

    if (g_fail == 0) std::printf("appconfig: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
