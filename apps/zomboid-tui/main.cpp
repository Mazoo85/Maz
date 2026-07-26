// ZOMBOID: ANCHORAGE — playable terminal front-end.
//
// Drives the ported deterministic Sim from the keyboard and renders each frame
// as truecolor half-block ANSI (two vertical pixels per character cell) plus a
// text HUD. No GPU/window needed — it's the first *playable* build of the port.
//
//   zomboid-tui            # play in the terminal
//   zomboid-tui --once     # headless: render one autopilot frame and exit (CI)
//
// Controls: WASD move · SPACE attack · E loot · R reload · 1-8 use · F light
//           P pause · N new game (also restarts after death) · Q quit
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

#include <csignal>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "zomboid/Sim.hpp"
#include "zomboid/render/SoftRenderer.hpp"

namespace {

volatile std::sig_atomic_t g_quit = 0;

#if !defined(_WIN32)
termios g_savedTermios;
bool g_rawActive = false;

void restoreTerminal() {
    if (g_rawActive) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_savedTermios);
        g_rawActive = false;
    }
    std::fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout); // reset, show cursor, leave alt screen
    std::fflush(stdout);
}

void enterRawMode() {
    tcgetattr(STDIN_FILENO, &g_savedTermios);
    termios raw = g_savedTermios;
    raw.c_lflag = raw.c_lflag & ~(static_cast<tcflag_t>(ICANON | ECHO));
    raw.c_cc[VMIN] = 0;  // non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    g_rawActive = true;
    std::fputs("\x1b[?1049h\x1b[?25l", stdout); // alt screen, hide cursor
    std::fflush(stdout);
}

void onSignal(int) { g_quit = 1; }

void querySize(int& cols, int& rows) {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
    } else {
        cols = 80;
        rows = 24;
    }
}
#endif

long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Encode the framebuffer as half-block cells: '▀' with fg = top pixel, bg =
// bottom pixel, so each character shows two stacked pixels.
void encodeHalfBlocks(const zb::Framebuffer& fb, std::string& out) {
    const int W = fb.width(), H = fb.height();
    const auto& px = fb.pixels();
    auto at = [&](int x, int y, int ch) -> int {
        const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)) *
                             3 + static_cast<size_t>(ch);
        return px[i];
    };
    out += "\x1b[H";
    char esc[32];
    for (int cy = 0; cy * 2 < H; cy++) {
        int lastFg = -1, lastBg = -1;
        for (int cx = 0; cx < W; cx++) {
            const int ty = cy * 2, by = std::min(H - 1, cy * 2 + 1);
            const int fr = at(cx, ty, 0), fg = at(cx, ty, 1), fb_ = at(cx, ty, 2);
            const int br = at(cx, by, 0), bg = at(cx, by, 1), bb = at(cx, by, 2);
            const int fgKey = (fr << 16) | (fg << 8) | fb_;
            const int bgKey = (br << 16) | (bg << 8) | bb;
            if (fgKey != lastFg) {
                std::snprintf(esc, sizeof(esc), "\x1b[38;2;%d;%d;%dm", fr, fg, fb_);
                out += esc;
                lastFg = fgKey;
            }
            if (bgKey != lastBg) {
                std::snprintf(esc, sizeof(esc), "\x1b[48;2;%d;%d;%dm", br, bg, bb);
                out += esc;
                lastBg = bgKey;
            }
            out += "\xe2\x96\x80"; // U+2580 UPPER HALF BLOCK
        }
        out += "\x1b[0m\r\n";
    }
}

// Aim at the nearest living zombie if one is close, else keep the last heading.
zb::Vec2 pickAim(const zb::Sim& sim, zb::Vec2 lastDir) {
    const zb::Zombie* best = nullptr;
    float bd = 8.0f;
    for (const auto& z : sim.zombies()) {
        if (z.dead) continue;
        const float d = zb::dist(z.pos, sim.player().pos);
        if (d < bd) {
            bd = d;
            best = &z;
        }
    }
    if (best) return best->pos;
    return zb::Vec2{sim.player().pos.x + lastDir.x, sim.player().pos.y + lastDir.y};
}

std::string statusText(const zb::Sim& sim, bool paused) {
    const zb::Player& p = sim.player();
    char buf[512];
    const int hh = static_cast<int>(sim.dayTime()) / 60;
    const int mm = static_cast<int>(sim.dayTime()) % 60;
    const char* state = p.dead ? "  *** YOU DIED — press N ***" : (paused ? "  [PAUSED]" : "");
    std::snprintf(buf, sizeof(buf),
                  "\x1b[0m\x1b[K HP %3.0f  FED %3.0f  HYD %3.0f  ENE %3.0f  MOOD %3.0f%s%s\r\n"
                  "\x1b[K DAY %d  %02d:%02d  KILLS %d  Z %d  WEAPON %s%s\r\n"
                  "\x1b[K WASD move  SPACE attack  E loot  R reload  1-8 use  F light  P pause  "
                  "N new  Q quit",
                  p.health, 100.0 - static_cast<double>(p.hunger),
                  100.0 - static_cast<double>(p.thirst), 100.0 - static_cast<double>(p.fatigue),
                  static_cast<double>(p.mood), p.infected ? "  [INFECTED]" : "", state, sim.day(),
                  hh, mm, sim.kills(), sim.aliveZombies(), zb::itemDef(p.weapon).name.c_str(),
                  p.flashlight ? "  (light)" : "");
    return buf;
}

// Run the autopilot for a few steps and print one frame — headless smoke path.
int runOnce(int frames) {
    zb::Sim sim(1);
    sim.newGame();
    zb::Vec2 aim{1, 0};
    for (int i = 0; i < frames; i++) {
        zb::Input in;
        in.aim = pickAim(sim, aim);
        const auto& zs = sim.zombies();
        for (const auto& z : zs) {
            if (z.dead) continue;
            if (zb::dist(z.pos, sim.player().pos) < 1.8f) in.attackHeld = true;
            break;
        }
        sim.step(in);
    }
    zb::Framebuffer fb(120, 60);
    zb::RenderOptions opts;
    opts.tilePx = 5;
    opts.hud = false;
    opts.postFx = false;
    std::string frame;
    zb::renderScene(sim, fb, opts);
    encodeHalfBlocks(fb, frame);
    frame += statusText(sim, false);
    frame += "\r\n";
    std::fwrite(frame.data(), 1, frame.size(), stdout);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    bool once = false;
    int onceFrames = 60;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--once") == 0) once = true;
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) onceFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("usage: zomboid-tui [--once [--frames N]]\n");
            return 0;
        }
    }

#if !defined(_WIN32)
    if (!once && !isatty(STDIN_FILENO)) once = true; // no keyboard -> headless
#else
    once = true; // interactive TUI is POSIX-only
#endif

    if (once) return runOnce(onceFrames);

#if !defined(_WIN32)
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::atexit(restoreTerminal);
    enterRawMode();

    zb::Sim sim(static_cast<uint64_t>(nowMs()));
    sim.newGame();

    bool paused = false;
    zb::Vec2 lastDir{1, 0};
    std::array<long, 256> lastSeen{};
    lastSeen.fill(-100000);
    std::array<long, 256> lastFired{};
    lastFired.fill(-100000);

    const long stepMs = static_cast<long>(zb::kStep * 1000.0f);
    long acc = 0;
    long prev = nowMs();
    int cols = 80, rows = 24;

    while (!g_quit) {
        const long t = nowMs();

        // --- read input ---
        char ibuf[64];
        const ssize_t n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
        zb::Input in;
        for (ssize_t k = 0; k < n; k++) {
            unsigned char c = static_cast<unsigned char>(ibuf[k]);
            if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
            lastSeen[c] = t;
            auto edge = [&](unsigned char key) {
                if (c == key && t - lastFired[key] > 180) {
                    lastFired[key] = t;
                    return true;
                }
                return false;
            };
            if (edge('e')) in.interact = true;
            if (edge('r')) in.reload = true;
            if (edge('f')) in.toggleFlashlight = true;
            if (c >= '1' && c <= '8' && edge(c)) in.useSlot = c - '1';
            if (edge('p')) paused = !paused;
            if (edge('n')) { sim = zb::Sim(static_cast<uint64_t>(t)); sim.newGame(); paused = false; }
            if (c == 'q') g_quit = 1;
        }

        // Held keys (terminal repeat): active if seen within a short window.
        auto held = [&](unsigned char key) { return t - lastSeen[key] < 160; };
        float mx = 0, my = 0;
        if (held('a')) mx -= 1;
        if (held('d')) mx += 1;
        if (held('w')) my -= 1;
        if (held('s')) my += 1;
        in.moveX = mx;
        in.moveY = my;
        if (mx != 0 || my != 0) {
            const float l = std::sqrt(mx * mx + my * my);
            lastDir = zb::Vec2{mx / l, my / l};
        }
        if (held(' ')) in.attackHeld = true;
        in.aim = pickAim(sim, lastDir);

        // --- advance sim in fixed steps ---
        acc += t - prev;
        prev = t;
        if (acc > 250) acc = 250; // avoid spiral of death
        while (acc >= stepMs) {
            if (!paused && !sim.player().dead) sim.step(in);
            acc -= stepMs;
        }

        // --- render ---
        querySize(cols, rows);
        const int textRows = 3;
        const int imgH = std::max(2, (rows - textRows) * 2);
        zb::Framebuffer fb(std::max(2, cols), imgH);
        zb::RenderOptions opts;
        opts.tilePx = 5;
        opts.hud = false;
        opts.postFx = false;
        zb::renderScene(sim, fb, opts);

        std::string frame;
        frame.reserve(static_cast<size_t>(cols) * static_cast<size_t>(rows) * 8);
        frame += "\x1b[H";
        encodeHalfBlocks(fb, frame);
        frame += statusText(sim, paused);
        std::fwrite(frame.data(), 1, frame.size(), stdout);
        std::fflush(stdout);

        // --- frame pacing (~30 fps) ---
        const long elapsed = nowMs() - t;
        if (elapsed < 33) {
            timespec ts{0, (33 - elapsed) * 1000000L};
            nanosleep(&ts, nullptr);
        }
    }

    restoreTerminal();
#endif
    return 0;
}
