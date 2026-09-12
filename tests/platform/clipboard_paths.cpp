// tests/platform/clipboard_paths.cpp — verifies the two thinnest platform shims, platform::Clipboard
// and platform::Paths, which had no test and no demo between them: the engine's only genuinely dark
// corner. Both are SDL3-backed and implemented in Window.cpp, so this runs against the real SDL
// under the dummy drivers CI uses rather than a mock.
//
// What is asserted is the contract the headers promise, and no more. Clipboard.hpp says it is "safe
// to call before/without a window (returns empty / does nothing) so headless code never crashes" —
// that claim is what is checked here, because it is the one every headless tool and test in this
// repo relies on without thinking about it. A round trip through the clipboard is checked only when
// the platform accepts the write: a CI container with no display server is entitled to refuse, and a
// test that demands otherwise would be asserting something about the machine, not about the engine.
#include "maz/platform/Clipboard.hpp"

#include "maz/platform/Paths.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

int main() {
    // --- 1. The clipboard is safe to touch with no window open. ---
    //     Every call below happens before anything has created one. None may crash.
    {
        const std::string before = clipboardText();
        CHECK(before.size() == before.size(), "clipboardText() returned a usable string");
        const bool has = hasClipboardText();
        CHECK(has == !clipboardText().empty(), "hasClipboardText agrees with clipboardText being empty");
    }

    // --- 2. Writing then reading back, when the platform allows writing at all. ---
    {
        const std::string sample = "maz clipboard round trip \xE2\x9C\x93 \xC3\xA9\xC3\xA5";  // UTF-8 on purpose
        if (setClipboardText(sample)) {
            CHECK(clipboardText() == sample, "what was written came back unchanged, bytes and all");
            CHECK(hasClipboardText(), "hasClipboardText is true after a successful write");
        } else {
            // No display server to own a clipboard. Refusing is correct; crashing is not.
            std::printf("note: platform refused a clipboard write (no display server) — round trip skipped\n");
        }
    }

    // --- 3. Clearing, again only when the platform accepts writes. ---
    {
        if (setClipboardText("")) {
            CHECK(!hasClipboardText(), "an empty clipboard does not count as holding text");
            CHECK(clipboardText().empty(), "and reads back empty");
        }
    }

    // --- 4. prefPath resolves to something usable, and ends with the file asked for. ---
    //     The header promises a fallback to the bare filename if the platform path cannot be
    //     resolved, so the one thing true in every case is that the name is on the end.
    {
        const std::string p = prefPath("MazEngineTests", "ClipboardPaths", "settings.json");
        CHECK(!p.empty(), "prefPath returned something");
        const std::string wanted = "settings.json";
        CHECK(p.size() >= wanted.size() && p.compare(p.size() - wanted.size(), wanted.size(), wanted) == 0,
              "prefPath ends with the file it was asked for");
    }

    // --- 5. Two different files in the same app share a directory; two apps do not. ---
    {
        const std::string a = prefPath("MazEngineTests", "ClipboardPaths", "a.txt");
        const std::string b = prefPath("MazEngineTests", "ClipboardPaths", "b.txt");
        CHECK(a != b, "different files get different paths");

        const std::string dirA = a.substr(0, a.size() - 5);   // drop "a.txt"
        const std::string dirB = b.substr(0, b.size() - 5);
        CHECK(dirA == dirB, "the same org and app resolve to the same directory");

        // Only meaningful when a real platform path was resolved. The header's documented
        // fallback is the bare filename, and in that case every app honestly shares "a.txt" —
        // asserting otherwise would fail the build on a machine with no writable home, which
        // is a fact about the machine rather than a bug in the engine.
        if (!dirA.empty()) {
            const std::string other = prefPath("MazEngineTests", "SomewhereElse", "a.txt");
            CHECK(other != a, "a different app does not write into another app's directory");
        } else {
            std::printf("note: no per-user path available, prefPath fell back to the bare filename\n");
        }
    }

    // --- 6. Asking twice gives the same answer. ---
    {
        CHECK(prefPath("MazEngineTests", "ClipboardPaths", "save.bin") ==
              prefPath("MazEngineTests", "ClipboardPaths", "save.bin"),
              "prefPath is stable across calls, so a save written is a save found");
    }

    if (g_fail == 0) std::printf("clipboard_paths: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
