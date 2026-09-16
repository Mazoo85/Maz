// Maz Engine — "HANDHELD" (platform::dpiFromDiagonal / recommendedTouchTargetPx / meetsTouchTarget /
// expandToTouchTarget, thumbReachRadius / withinThumbReach / thumbControlLayout, scaledSize /
// logicalToPixels, displayForRect / centerRectOnDisplay, decideFrame, isOnline / isMetered,
// captureBacktrace, softKeyboardTypeName — what the engine knows about the device it landed on)
// A game is written on one screen and played on a hundred, in a hand, by somebody holding a phone one-
// handed on a bus. LEFT: whether a button is big enough for a fingertip, which is a question about
// MILLIMETRES and only turns into pixels after you know the panel — and the answer disagrees with both
// platform guidelines, on purpose. Under it, how little of a modern phone a thumb can actually reach.
// MIDDLE: the same layout at four pixel densities, and which monitor a window lands on across a
// three-screen desk. RIGHT: what the loop should do when nobody is looking at it, what a downloader
// should do with the connection it has, and a backtrace captured live — the engine reading its own
// stack, which is what it will be doing at the worst possible moment.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/platform/AppFocus.hpp"
#include "maz/platform/CrashHandler.hpp"
#include "maz/platform/DisplayScale.hpp"
#include "maz/platform/Displays.hpp"
#include "maz/platform/Network.hpp"
#include "maz/platform/SoftKeyboard.hpp"
#include "maz/platform/ThumbZone.hpp"
#include "maz/platform/TouchTarget.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

// A screen described the way a spec sheet describes one: how many pixels, how big the glass is, and
// how many real pixels the OS spends on one logical point.
struct Screen {
    const char* what;
    int w = 0;
    int h = 0;
    float diagonal = 0.0f;
    float scale = 1.0f;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("HANDHELD starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Handheld";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- is a button big enough for a fingertip? ----------------------------------------------------
    const Screen screens[] = {
        {"4.7in phone", 750, 1334, 4.7f, 2.0f},
        {"6.2in phone", 1080, 2400, 6.2f, 2.625f},
        {"6.7in phone", 1290, 2796, 6.7f, 3.0f},
        {"12.9in tablet", 2048, 2732, 12.9f, 2.0f},
    };
    struct Density {
        std::string what;
        float ppi = 0.0f;
        float scale = 1.0f;
        float minPixels = 0.0f;
        float minPoints = 0.0f;
        float buttonPx = 0.0f; // a 44-point button in real pixels
        bool buttonPasses = false;
    };
    std::vector<Density> densities;
    for (const Screen& s : screens) {
        const float ppi = platform::dpiFromDiagonal(s.w, s.h, s.diagonal);
        const float minPixels = platform::recommendedTouchTargetPx(ppi);
        const float buttonPx = 44.0f * s.scale;
        densities.push_back(Density{s.what, ppi, s.scale, minPixels, minPixels / s.scale, buttonPx,
                                    platform::meetsTouchTarget(buttonPx, buttonPx, ppi)});
    }
    math::Rect2 tinyButton(100.0f, 200.0f, 24.0f, 18.0f);
    const math::Rect2 grownButton =
        platform::expandToTouchTarget(tinyButton, densities[2].ppi);

    // ---- how much of a big phone can a thumb reach? --------------------------------------------------
    // A 6.7-inch phone's safe area in points, with the notch and the home indicator taken out.
    const math::Rect2 safe(0.0f, 59.0f, 430.0f, 833.0f);
    const float reach = platform::thumbReachRadius(safe);
    const math::vec2 thumbCorner(safe.position.x + safe.size.x, safe.position.y + safe.size.y);
    bool reachable[3][3] = {};
    int reachableCount = 0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const math::vec2 p(safe.position.x + safe.size.x * (0.15f + 0.35f * static_cast<float>(c)),
                               safe.position.y + safe.size.y * (0.12f + 0.38f * static_cast<float>(r)));
            reachable[r][c] = platform::withinThumbReach(p, thumbCorner, reach);
            if (reachable[r][c]) {
                ++reachableCount;
            }
        }
    }
    const platform::TouchControlLayout rightHanded =
        platform::thumbControlLayout(safe, 140.0f, 120.0f, 16.0f, platform::Handedness::RightHanded);
    const platform::TouchControlLayout leftHanded =
        platform::thumbControlLayout(safe, 140.0f, 120.0f, 16.0f, platform::Handedness::LeftHanded);

    // ---- one layout, four pixel densities ------------------------------------------------------------
    struct Scaled {
        float scale = 1.0f;
        platform::PixelSize size;
        int buttonPixels = 0;
        float backAgain = 0.0f;
    };
    std::vector<Scaled> scaled;
    for (float s : {1.0f, 1.5f, 2.0f, 3.0f}) {
        const int px = platform::logicalToPixels(44.0f, s);
        scaled.push_back(Scaled{s, platform::scaledSize(1280, 720, s), px,
                                platform::pixelsToLogical(px, s)});
    }

    // ---- which monitor is the window on? -------------------------------------------------------------
    const std::vector<platform::DisplayInfo> desk = {
        {0, 0, 0, 1920, 1080, 1.0f},      // the laptop
        {1, -2560, 0, 2560, 1440, 1.0f},  // one to the left
        {2, 0, -2160, 3840, 2160, 2.0f},  // one above, hidpi
    };
    struct Placed {
        std::string what;
        int display = -1;
        platform::Point2i centred;
    };
    std::vector<Placed> placed;
    {
        struct Probe {
            const char* what;
            int x, y, w, h;
        };
        const Probe probes[] = {
            {"opened at the origin", 10, 10, 800, 600},
            {"dragged left", -2000, 100, 800, 600},
            {"dragged up", 200, -1500, 800, 600},
            {"straddling two screens", -300, 100, 800, 600},
        };
        for (const Probe& p : probes) {
            const int d = platform::displayForRect(desk, p.x, p.y, p.w, p.h);
            platform::Point2i c{0, 0};
            if (d >= 0) {
                c = platform::centerRectOnDisplay(desk[static_cast<std::size_t>(d)], p.w, p.h);
            }
            placed.push_back(Placed{p.what, d, c});
        }
    }

    // ---- what the loop should do when nobody is looking -----------------------------------------------
    struct Decision {
        std::string what;
        platform::FrameAction action;
    };
    std::vector<Decision> decisions;
    const platform::FocusPolicy policy;
    {
        struct Case {
            const char* what;
            bool focused;
            bool minimized;
        };
        const Case cases[] = {{"focused, on screen", true, false},
                              {"behind another window", false, false},
                              {"minimised", false, true}};
        for (const Case& c : cases) {
            decisions.push_back(Decision{
                c.what, platform::decideFrame(platform::WindowActivation{c.focused, c.minimized},
                                              policy)});
        }
    }

    // ---- the connection, and what to do with it -------------------------------------------------------
    struct Link {
        std::string name;
        bool online = false;
        bool metered = false;
        bool safeForBigDownload = false;
    };
    std::vector<Link> links;
    for (platform::NetworkReachability r :
         {platform::NetworkReachability::Unknown, platform::NetworkReachability::Offline,
          platform::NetworkReachability::Cellular, platform::NetworkReachability::Wifi}) {
        links.push_back(Link{platform::reachabilityName(r), platform::isOnline(r),
                             platform::isMetered(r), platform::isUnmeteredOnline(r)});
    }

    // ---- the engine reading its own stack --------------------------------------------------------------
    const std::vector<std::string> backtrace = platform::captureBacktrace(6, 0);
    std::string keyboardTypes;
    for (platform::SoftKeyboardType t :
         {platform::SoftKeyboardType::Default, platform::SoftKeyboardType::Number,
          platform::SoftKeyboardType::Email, platform::SoftKeyboardType::Phone,
          platform::SoftKeyboardType::Url}) {
        keyboardTypes += (keyboardTypes.empty() ? "" : ", ");
        keyboardTypes += platform::softKeyboardTypeName(t);
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kNo{1.0f, 0.48f, 0.42f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.28f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  HANDHELD", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "what the engine knows about the device it landed on — and what that means "
                          "for a game held in one hand",
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };

            // ---- column 1 ----
            float y = 100.0f;
            font.drawText(*renderer, 24.0f, y, "BIG ENOUGH FOR A FINGERTIP?", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "the rule is 9 MILLIMETRES; everything else is conversion", kDim, 0.25f);
            y += 24.0f;
            cell(24.0f, y, "screen", kDim, 0.25f);
            cell(160.0f, y, "ppi", kDim, 0.25f);
            cell(225.0f, y, "9mm px", kDim, 0.25f);
            cell(300.0f, y, "9mm pt", kDim, 0.25f);
            cell(370.0f, y, "44pt?", kDim, 0.25f);
            y += 22.0f;
            for (const Density& d : densities) {
                cell(24.0f, y, d.what, kText, sz);
                cell(160.0f, y, num(static_cast<double>(d.ppi), 0), kDim, sz);
                cell(225.0f, y, num(static_cast<double>(d.minPixels), 0), kVal, sz);
                cell(300.0f, y, num(static_cast<double>(d.minPoints), 0), kVal, sz);
                cell(370.0f, y, d.buttonPasses ? "passes" : "under",
                     d.buttonPasses ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 24.0f, y,
                          ("9mm lands between " + num(static_cast<double>(densities[3].minPoints), 0) +
                           " and " + num(static_cast<double>(densities[0].minPoints), 0) +
                           " points depending on the panel, which is exactly why the rule is written in "
                           "millimetres and not in points. And it is STRICTER than both platform "
                           "guidelines: Apple asks for 44 points, Android for 48, and a 44-point "
                           "button fails the 9mm test on every screen here. Neither is wrong — they "
                           "draw the line in different places, and this makes the gap visible instead "
                           "of leaving it to be discovered by a player who keeps missing. A " +
                           num(static_cast<double>(tinyButton.size.x), 0) + "x" +
                           num(static_cast<double>(tinyButton.size.y), 0) +
                           " pixel button grows to " + num(static_cast<double>(grownButton.size.x), 0) +
                           " square, around the same centre.")
                              .c_str(),
                          kDim, 0.25f);

            y += 130.0f;
            font.drawText(*renderer, 24.0f, y, "AND CAN A THUMB REACH IT?", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y,
                 "a " + num(static_cast<double>(safe.size.x), 0) + "x" +
                     num(static_cast<double>(safe.size.y), 0) + " safe area; the arc sweeps " +
                     num(static_cast<double>(reach), 0) + " from the corner",
                 kDim, 0.25f);
            y += 24.0f;
            {
                const char* rowName[3] = {"top", "middle", "bottom"};
                for (int r = 0; r < 3; ++r) {
                    cell(24.0f, y, rowName[r], kDim, sz);
                    for (int c = 0; c < 3; ++c) {
                        cell(120.0f + 95.0f * static_cast<float>(c), y,
                             reachable[r][c] ? "in reach" : "too far", reachable[r][c] ? kOk : kNo, sz);
                    }
                    y += 23.0f;
                }
            }
            y += 8.0f;
            font.drawText(*renderer, 24.0f, y,
                          (std::to_string(reachableCount) +
                           " of nine positions are comfortable for a right thumb, and they are all "
                           "along the bottom. That is the ergonomics of a large phone, not a quirk of "
                           "the heuristic — anything above the halfway line needs the other hand or a "
                           "shuffle of the grip. thumbControlLayout puts the stick and the buttons in "
                           "the two bottom corners for that reason, and swaps them for a left-handed "
                           "player: stick at " +
                           num(static_cast<double>(rightHanded.moveStick.position.x), 0) + " becomes " +
                           num(static_cast<double>(leftHanded.moveStick.position.x), 0) + ".")
                              .c_str(),
                          kDim, 0.25f);

            // ---- column 2 ----
            y = 100.0f;
            font.drawText(*renderer, 500.0f, y, "ONE LAYOUT, FOUR DENSITIES", kHead, 0.34f);
            y += 28.0f;
            cell(500.0f, y, "scale", kDim, 0.25f);
            cell(570.0f, y, "1280x720 becomes", kDim, 0.25f);
            cell(740.0f, y, "44pt is", kDim, 0.25f);
            cell(820.0f, y, "back", kDim, 0.25f);
            y += 22.0f;
            for (const Scaled& s : scaled) {
                cell(500.0f, y, num(static_cast<double>(s.scale), 1) + "x", kText, sz);
                cell(570.0f, y,
                     std::to_string(s.size.width) + " x " + std::to_string(s.size.height), kVal, sz);
                cell(740.0f, y, std::to_string(s.buttonPixels) + "px", kVal, sz);
                cell(820.0f, y, num(static_cast<double>(s.backAgain), 0) + "pt",
                     s.backAgain == 44.0f ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 500.0f, y,
                          "The last column is the one worth checking: a size converted to pixels and "
                          "back must be the size you started with, or a layout drifts a little every "
                          "time it is measured.",
                          kDim, 0.25f);

            y += 68.0f;
            font.drawText(*renderer, 500.0f, y, "WHICH SCREEN IS IT ON?", kHead, 0.34f);
            y += 28.0f;
            cell(500.0f, y, "a laptop, one screen to its left, one above in hidpi", kDim, 0.25f);
            y += 24.0f;
            for (const Placed& p : placed) {
                cell(500.0f, y, p.what, kText, sz);
                cell(730.0f, y,
                     p.display < 0 ? "nowhere" : "display " + std::to_string(p.display),
                     p.display < 0 ? kNo : kVal, sz);
                cell(860.0f, y,
                     p.display < 0 ? "" : "-> " + std::to_string(p.centred.x) + ", " +
                                              std::to_string(p.centred.y),
                     kDim, 0.24f);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 500.0f, y,
                          "A window straddling two screens belongs to whichever it overlaps MORE, "
                          "which is the answer that matches where a person thinks the window is — and "
                          "the last column is where it would land if asked to centre itself there.",
                          kDim, 0.25f);

            y += 68.0f;
            font.drawText(*renderer, 500.0f, y, "WHAT THE KEYBOARD SHOULD BE", kHead, 0.34f);
            y += 28.0f;
            cell(500.0f, y, keyboardTypes, kVal, sz);
            y += 24.0f;
            font.drawText(*renderer, 500.0f, y,
                          "Asking for the right one is the difference between typing a score into a "
                          "number pad and hunting for digits on a letter keyboard.",
                          kDim, 0.25f);

            // ---- column 3 ----
            y = 100.0f;
            font.drawText(*renderer, 960.0f, y, "WHEN NOBODY IS LOOKING", kHead, 0.34f);
            y += 28.0f;
            cell(960.0f, y, "state", kDim, 0.25f);
            cell(1120.0f, y, "sim", kDim, 0.25f);
            cell(1175.0f, y, "draw", kDim, 0.25f);
            cell(1235.0f, y, "then sleep", kDim, 0.25f);
            y += 22.0f;
            for (const Decision& d : decisions) {
                cell(960.0f, y, d.what, kText, sz);
                cell(1120.0f, y, d.action.advanceSim ? "yes" : "no",
                     d.action.advanceSim ? kOk : kDim, sz);
                cell(1175.0f, y, d.action.render ? "yes" : "no", d.action.render ? kOk : kDim, sz);
                cell(1235.0f, y, num(d.action.throttleMs, 0) + "ms", kVal, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 960.0f, y,
                          ("Minimised, it stops drawing — there is no surface to present to — but the "
                           "simulation keeps running, because timers and a network connection do not "
                           "care that the window is hidden and a game that froze them would come back "
                           "to a world that had stopped. The sleep is one frame at " +
                           num(policy.backgroundFps, 0) +
                           " fps, which is what stops a background window spinning a core.")
                              .c_str(),
                          kDim, 0.25f);

            y += 100.0f;
            font.drawText(*renderer, 960.0f, y, "AND WHAT TO DOWNLOAD", kHead, 0.34f);
            y += 28.0f;
            for (const Link& l : links) {
                cell(960.0f, y, l.name, kText, sz);
                cell(1075.0f, y, l.online ? "online" : "offline", l.online ? kOk : kDim, sz);
                cell(1180.0f, y, l.metered ? "metered" : "free", l.metered ? kNo : kDim, sz);
                cell(1285.0f, y, l.safeForBigDownload ? "go" : "wait",
                     l.safeForBigDownload ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 960.0f, y,
                          "\"Unknown\" answers the same as offline, which is the safe way round: a "
                          "reachability nobody has asked for yet must not authorise spending somebody "
                          "else's data plan.",
                          kDim, 0.25f);

            y += 72.0f;
            font.drawText(*renderer, 960.0f, y, "READING ITS OWN STACK", kHead, 0.34f);
            y += 28.0f;
            if (backtrace.empty()) {
                cell(960.0f, y, "no backtrace on this build", kDim, sz);
                y += 23.0f;
            } else {
                for (std::size_t i = 0; i < backtrace.size() && i < 4; ++i) {
                    const std::string& f = backtrace[i];
                    cell(960.0f, y,
                         std::to_string(i) + ": " +
                             (f.size() > 46 ? "..." + f.substr(f.size() - 43) : f),
                         kVal, 0.24f);
                    y += 20.0f;
                }
            }
            y += 8.0f;
            font.drawText(*renderer, 960.0f, y,
                          ("Those frames were captured a few microseconds ago, by this program, about "
                           "itself — " + std::to_string(backtrace.size()) +
                           " of them. The same call runs from a signal handler, which is the only "
                           "moment it matters and the worst moment to discover it does not work.")
                              .c_str(),
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 700.0f,
                          "Every number on this screen is a decision something has to make before a "
                          "frame is drawn — how big to make a button, whether to keep simulating, "
                          "whether to start a download. None of them needs a device to answer.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("HANDHELD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
