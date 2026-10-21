// Maz Engine — "Iron Front" — a turn-based WWII hex strategy game in the style of World Conqueror 4.
// Pick one of five powers in Europe, 1939, and conquer the continent: move and attack with infantry,
// tanks, artillery, fighters and destroyers; flank to hit harder; take capitals to knock countries out.
//
// All the rules live in ironfront/*.hpp (pure logic, unit-tested in tests/ironfront/). This file is the
// glue: screens, unified touch/mouse input, drawing the hex map with flat shapes, the HUD, playing the
// AI's moves back one at a time, and auto-saving (after each round and when the app is backgrounded).
//
// Controls — tap/click a unit to select it (blue = can move there, red = can attack); tap a highlight to
// act. Drag to pan, pinch or mouse-wheel to zoom. Keyboard: Enter = End Turn, Esc = deselect/quit.
// --headless / --frames N play an all-AI game with no window (CI smoke test).

#include "ironfront/Ai.hpp"
#include "ironfront/Fog.hpp"
#include "ironfront/Game.hpp"
#include "ironfront/Save.hpp"

#include "maz/Engine.hpp"
#include "maz/core/FramePacer.hpp"
#include "maz/core/KeyValueStore.hpp"
#include "maz/io/VirtualFileSystem.hpp"
#include "maz/platform/DesktopBackend.hpp"
#include "maz/platform/Haptics.hpp"
#include "maz/platform/PlatformBackend.hpp"
#include "maz/platform/PowerState.hpp"
#include "maz/platform/SafeArea.hpp"
#include "maz/platform/TouchTarget.hpp"
#include "maz/platform/WebLoop.hpp"
#include "maz/ui/Font.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using namespace maz;
using ironfront::Hex;

namespace {

// --- look --------------------------------------------------------------------------------------
render::Color countryColor(int c) {
    switch (c) {
        case ironfront::Germany: return {0.46f, 0.49f, 0.52f, 1.0f};
        case ironfront::Britain: return {0.80f, 0.28f, 0.27f, 1.0f};
        case ironfront::France: return {0.27f, 0.47f, 0.86f, 1.0f};
        case ironfront::Soviet: return {0.86f, 0.56f, 0.18f, 1.0f};
        case ironfront::Italy: return {0.32f, 0.66f, 0.32f, 1.0f};
        default: return {0.74f, 0.71f, 0.60f, 1.0f};
    }
}
render::Color scale(render::Color c, float k) { return {c.r * k, c.g * k, c.b * k, c.a}; }
const render::Color kSea{0.11f, 0.22f, 0.38f, 1.0f};
const render::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
const render::Color kPanel{0.05f, 0.06f, 0.09f, 0.82f};
const render::Color kGold{0.98f, 0.84f, 0.30f, 1.0f};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px <= x + w && py <= y + h; }
};

enum class Screen { Title, Pick, Playing, Over };

// Unified pointer: the first touch on a phone, the left mouse button on desktop. Reports a tap (press
// and release without much movement), a drag delta, and a pinch zoom factor.
struct Pointer {
    bool down = false, wasDown = false;
    float x = 0, y = 0, startX = 0, startY = 0, dragX = 0, dragY = 0;
    bool dragging = false;
    bool tapped = false;
    float tapX = 0, tapY = 0;
    float pinch = 1.0f; // zoom factor this frame
    float lastPinchDist = 0.0f;

    void update(const platform::Input& in) {
        tapped = false;
        dragX = dragY = 0.0f;
        pinch = 1.0f;
        if (in.touchCount() >= 2) {
            const platform::Touch& a = in.touch(0);
            const platform::Touch& b = in.touch(1);
            const float d = std::hypot(a.x - b.x, a.y - b.y);
            if (lastPinchDist > 0.0f && d > 0.0f) pinch = d / lastPinchDist;
            lastPinchDist = d;
            dragging = true; // a pinch never becomes a tap
            down = true;
            wasDown = true;
            return;
        }
        lastPinchDist = 0.0f;
        float nx = in.mouseX(), ny = in.mouseY();
        bool nowDown = in.mouseDown(1);
        if (in.touchCount() == 1) {
            nx = in.touch(0).x;
            ny = in.touch(0).y;
            nowDown = true;
        } else if (!in.releasedTouches().empty()) {
            nx = in.releasedTouches().back().x;
            ny = in.releasedTouches().back().y;
            nowDown = false;
        }
        if (nowDown && !down) {
            startX = nx;
            startY = ny;
            dragging = false;
        }
        if (nowDown && down) {
            dragX = nx - x;
            dragY = ny - y;
            if (std::hypot(nx - startX, ny - startY) > 12.0f) dragging = true;
        }
        if (!nowDown && down && !dragging) {
            tapped = true;
            tapX = nx;
            tapY = ny;
        }
        wasDown = down;
        down = nowDown;
        x = nx;
        y = ny;
    }
};

// --- the app -----------------------------------------------------------------------------------
struct App {
    render::Renderer* r = nullptr;
    ui::Font* font = nullptr;
    render::TextureHandle white{};
    float sw = 1280, sh = 720;
    math::Rect2 safe{0, 0, 1280, 720};
    float buttonH = 56.0f;

    Screen screen = Screen::Title;
    std::optional<ironfront::Game> game;
    ironfront::AiTurn aiTurn;
    float aiTimer = 0.0f;
    bool aiFast = false; // tap during the AI's turn to skip its animation
    maz::game::FogOfWar fog;
    int selected = 0; // unit id, 0 = none
    std::vector<Hex> moveHexes, attackHexes;
    std::string message;

    // camera: screen = world * zoom + pan, with world = hexToPixel(h, kHex)
    static constexpr float kHex = 24.0f;
    float zoom = 1.0f;
    float panX = 0.0f, panY = 0.0f;

    core::KeyValueStore* save = nullptr;
    platform::PlatformBackend* backend = nullptr;
    bool headless = false;

    // ---- drawing helpers ----
    void rect(float x, float y, float w, float h, render::Color c) {
        render::SpriteDesc s;
        s.x = x;
        s.y = y;
        s.width = w;
        s.height = h;
        s.color = c;
        r->drawSprite(white, s);
    }
    void text(float x, float y, const char* t, render::Color c, float sc) {
        if (font) font->drawText(*r, x, y, t, c, sc);
    }
    void textCentered(float cx, float y, const char* t, render::Color c, float sc) {
        if (font) font->drawTextCentered(*r, cx, y, t, c, sc);
    }
    float textScale() const { return std::clamp(buttonH / 64.0f * 0.42f, 0.28f, 0.6f); }
    bool button(const Rect& b, const char* label, const Pointer& p, render::Color fill) {
        rect(b.x, b.y, b.w, b.h, fill);
        rect(b.x, b.y + b.h - 3.0f, b.w, 3.0f, scale(fill, 0.6f));
        const float ts = textScale();
        textCentered(b.x + b.w * 0.5f, b.y + (b.h - (font ? font->lineHeight(ts) : 0.0f)) * 0.5f, label, kWhite, ts);
        return p.tapped && b.contains(p.tapX, p.tapY);
    }

    math::vec2 hexScreen(const Hex& h) const {
        const math::vec2 w = maz::game::hexToPixel(h, kHex);
        return {w.x * zoom + panX, w.y * zoom + panY};
    }
    Hex screenHex(float x, float y) const {
        return maz::game::pixelToHex(math::vec2((x - panX) / zoom, (y - panY) / zoom), kHex);
    }
    void hexPoly(const Hex& h, render::Color c, float shrink = 1.0f) {
        const math::vec2 ctr = hexScreen(h);
        const float rad = kHex * zoom * shrink;
        render::Point2 pts[6];
        for (int i = 0; i < 6; ++i) {
            const float a = (60.0f * static_cast<float>(i) - 30.0f) * 3.14159265f / 180.0f;
            pts[i] = {ctr.x + rad * std::cos(a), ctr.y + rad * std::sin(a)};
        }
        r->drawConvexPolygon(pts, 6, c);
    }

    void centreOn(const Hex& h) {
        const math::vec2 w = maz::game::hexToPixel(h, kHex);
        panX = sw * 0.5f - w.x * zoom;
        panY = sh * 0.5f - w.y * zoom;
    }

    // ---- game flow ----
    void startGame(int country) {
        game = ironfront::Game::newGame(country, static_cast<uint64_t>(SDL_GetTicksNS()) | 1u);
        beginPlay();
    }
    void beginPlay() {
        screen = Screen::Playing;
        aiTurn = ironfront::AiTurn();
        selected = 0;
        message.clear();
        // Start zoomed so roughly a third of the map's height fills the screen.
        const float mapH = static_cast<float>(game ? game->world.map.height() : 40) * kHex * 1.5f;
        zoom = std::clamp(sh / mapH * 3.0f, 0.6f, 2.5f);
        if (game && game->player >= 0)
            if (const ironfront::City* cap = game->capitalOf(game->player)) centreOn(cap->hex);
        refreshFog();
    }
    void refreshFog() {
        if (game) ironfront::updateFog(game->world, game->player >= 0 ? game->player : game->current, fog);
    }
    bool playersTurn() const { return game && game->current == game->player && !game->isOver(); }
    void select(int id) {
        selected = id;
        moveHexes.clear();
        attackHexes.clear();
        if (!game || !id) return;
        moveHexes = game->moveTargets(id);
        attackHexes = game->attackTargets(id);
    }
    void writeSave() {
        if (!save || !game || headless) return;
        save->setString("game", ironfront::serialize(*game));
        save->save();
    }
    void endPlayerTurn() {
        if (!playersTurn()) return;
        select(0);
        game->endTurn();
        aiTurn = ironfront::AiTurn();
        aiFast = false;
    }
    void describe(const ironfront::Unit& a, const ironfront::Unit& d, const ironfront::CombatResult& res) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "%s %s hit %s %s for %d%s%s", ironfront::countryName(a.owner), a.st().name,
                      ironfront::countryName(d.owner), d.st().name, res.damageDealt,
                      res.defenderDestroyed ? " - destroyed!" : "",
                      res.damageTaken > 0 ? (std::string(", took ") + std::to_string(res.damageTaken)).c_str() : "");
        message = buf;
    }

    void handleMapTap(float x, float y) {
        const Hex h = screenHex(x, y);
        if (!game->world.map.inBounds(h)) {
            select(0);
            return;
        }
        if (selected) {
            const ironfront::Unit* sel = game->world.unitById(selected);
            for (const Hex& t : attackHexes)
                if (t == h && sel) {
                    const ironfront::Unit a = *sel;
                    const ironfront::Unit d = *game->world.unitAt(h);
                    ironfront::CombatResult res;
                    if (game->attack(selected, h, &res)) {
                        describe(a, d, res);
                        if (backend) backend->triggerHaptic(platform::HapticFeedback::ImpactHeavy);
                    }
                    select(game->world.unitById(a.id) ? a.id : 0);
                    refreshFog();
                    return;
                }
            for (const Hex& t : moveHexes)
                if (t == h) {
                    const int id = selected;
                    const ironfront::City* city = game->world.map.cityAt(h);
                    const bool enemyCity = city && city->owner != game->player;
                    if (game->move(id, h)) {
                        if (backend) backend->triggerHaptic(platform::HapticFeedback::Selection);
                        if (enemyCity && city->owner == game->player) message = "Captured " + city->name + "!";
                    }
                    refreshFog();
                    select(game->world.unitById(id) ? id : 0);
                    return;
                }
        }
        const ironfront::Unit* u = game->world.unitAt(h);
        select(u && u->owner == game->player ? u->id : 0);
    }

    // Plays the AI's moves back one every 0.12 s (all at once when skipping or headless).
    void stepAi(float dt) {
        if (!game || game->isOver() || playersTurn()) return;
        aiTimer -= dt;
        if (aiTimer > 0.0f && !aiFast && !headless) return;
        aiTimer = 0.12f;
        int budget = (aiFast || headless) ? 400 : 1;
        while (budget-- > 0 && !game->isOver() && !playersTurn()) {
            if (const auto a = aiTurn.next(*game)) {
                const ironfront::Unit* au = game->world.unitById(a->unitId);
                const ironfront::Unit* du = game->world.unitAt(a->target);
                bool ok = false;
                if (a->kind == ironfront::AiAction::Attack && au && du) {
                    const ironfront::Unit ac = *au, dc = *du;
                    ironfront::CombatResult res;
                    ok = game->attack(a->unitId, a->target, &res);
                    if (ok && dc.owner == game->player) describe(ac, dc, res);
                } else {
                    ok = ironfront::applyAiAction(*game, *a);
                }
                if (!ok) aiTurn.retire(a->unitId);
            } else {
                game->endTurn();
                aiTurn = ironfront::AiTurn();
                if (playersTurn()) {
                    aiFast = false;
                    writeSave(); // auto-save at the start of each of the player's turns
                }
            }
        }
        refreshFog();
        if (game->isOver()) screen = Screen::Over;
    }

    // ---- per-screen update + draw ----
    void drawMap() {
        const auto& w = game->world;
        const bool fogOn = game->player >= 0;
        for (int row = 0; row < w.map.height(); ++row)
            for (int col = 0; col < w.map.width(); ++col) {
                const Hex h = ironfront::offsetToHex(col, row);
                const math::vec2 c = hexScreen(h);
                const float m = kHex * zoom * 1.2f;
                if (c.x < -m || c.y < -m || c.x > sw + m || c.y > sh + m) continue;
                const ironfront::Terrain t = w.map.terrain(h);
                render::Color col4 = t == ironfront::Terrain::Sea ? kSea : countryColor(w.map.owner(h));
                if (t == ironfront::Terrain::Forest) col4 = scale(col4, 0.72f);
                if (t == ironfront::Terrain::Mountain) col4 = {col4.r * 0.6f + 0.3f, col4.g * 0.55f + 0.25f, col4.b * 0.5f + 0.2f, 1.0f};
                hexPoly(h, scale(col4, 0.55f), 1.0f);
                hexPoly(h, col4, 0.93f);
                if (t == ironfront::Terrain::Forest) hexPoly(h, scale(col4, 0.8f), 0.45f);
                if (t == ironfront::Terrain::Mountain) hexPoly(h, {0.92f, 0.92f, 0.95f, 0.7f}, 0.32f);
                if (fogOn && !ironfront::hexVisible(fog, h)) hexPoly(h, {0.0f, 0.0f, 0.0f, 0.28f}, 1.0f);
            }
        for (const Hex& h : moveHexes) hexPoly(h, {0.35f, 0.65f, 1.0f, 0.45f}, 0.9f);
        for (const Hex& h : attackHexes) hexPoly(h, {1.0f, 0.2f, 0.15f, 0.5f}, 0.9f);

        const float ts = std::clamp(0.2f * zoom, 0.14f, 0.36f);
        for (const ironfront::City& city : w.map.cities()) {
            const math::vec2 c = hexScreen(city.hex);
            const float s = kHex * zoom * (city.capital ? 0.5f : 0.34f);
            rect(c.x - s * 0.5f - 2.0f, c.y - s * 0.5f - 2.0f, s + 4.0f, s + 4.0f, {0.08f, 0.08f, 0.08f, 1.0f});
            rect(c.x - s * 0.5f, c.y - s * 0.5f, s, s, city.capital ? kGold : kWhite);
            if (zoom >= 0.9f) textCentered(c.x, c.y + kHex * zoom * 0.45f, city.name.c_str(), kWhite, ts);
        }

        for (const ironfront::Unit& u : w.units) {
            if (fogOn && u.owner != game->player && !ironfront::unitVisible(fog, u)) continue;
            const math::vec2 c = hexScreen(u.hex);
            const float s = kHex * zoom * 1.05f;
            const bool spent = u.owner == game->current && u.moved && u.attacked;
            const bool boat = ironfront::embarked(w, u);
            render::Color body = scale(countryColor(u.owner), spent ? 0.5f : 0.85f);
            if (u.id == selected) rect(c.x - s * 0.5f - 3.0f, c.y - s * 0.4f - 3.0f, s + 6.0f, s * 0.8f + 6.0f, kGold);
            rect(c.x - s * 0.5f, c.y - s * 0.4f, s, s * 0.8f, {0.05f, 0.05f, 0.05f, 1.0f});
            rect(c.x - s * 0.5f + 2.0f, c.y - s * 0.4f + 2.0f, s - 4.0f, s * 0.8f - 4.0f, body);
            const char label[3] = {boat ? '~' : u.st().letter, boat ? u.st().letter : '\0', '\0'};
            const float ls = std::clamp(0.3f * zoom, 0.16f, 0.5f);
            textCentered(c.x, c.y - (font ? font->lineHeight(ls) * 0.6f : 0.0f), label, kWhite, ls);
            const float frac = static_cast<float>(u.hp) / static_cast<float>(u.st().maxHp);
            rect(c.x - s * 0.42f, c.y + s * 0.24f, s * 0.84f, s * 0.1f, {0.1f, 0.0f, 0.0f, 1.0f});
            rect(c.x - s * 0.42f, c.y + s * 0.24f, s * 0.84f * frac, s * 0.1f,
                 frac > 0.5f ? render::Color{0.3f, 0.9f, 0.3f, 1.0f} : render::Color{0.95f, 0.6f, 0.15f, 1.0f});
        }
    }

    void updatePlaying(const Pointer& p, const platform::Input& in, float dt) {
        // camera
        if (p.dragging && p.down) {
            panX += p.dragX;
            panY += p.dragY;
        }
        float zf = p.pinch;
        if (in.wheel() != 0.0f) zf *= std::pow(1.12f, in.wheel());
        if (zf != 1.0f) {
            const float nz = std::clamp(zoom * zf, 0.35f, 3.5f);
            const float ax = p.x, ay = p.y; // zoom about the pointer
            panX = ax - (ax - panX) * (nz / zoom);
            panY = ay - (ay - panY) * (nz / zoom);
            zoom = nz;
        }
        if (in.keyPressed(SDL_SCANCODE_ESCAPE)) select(0);

        const float pad = 12.0f;
        const Rect endBtn{safe.left() + safe.size.x - pad - buttonH * 3.2f, safe.top() + safe.size.y - pad - buttonH,
                          buttonH * 3.2f, buttonH};
        const bool overHud = p.tapped && (endBtn.contains(p.tapX, p.tapY) || p.tapY < safe.top() + buttonH);

        if (playersTurn()) {
            if (in.keyPressed(SDL_SCANCODE_RETURN)) endPlayerTurn();
            else if (p.tapped && !overHud) handleMapTap(p.tapX, p.tapY);
        } else if (p.tapped) {
            aiFast = true;
        }
        stepAi(dt);

        drawMap();

        // top bar
        rect(0, 0, sw, safe.top() + buttonH, kPanel);
        char buf[160];
        const int cur = game->current;
        std::snprintf(buf, sizeof buf, "Turn %d  -  %s%s", game->turn, ironfront::countryName(cur),
                      cur == game->player ? " (you)" : "  thinking...");
        const float ts = textScale();
        rect(safe.left() + pad, safe.top() + buttonH * 0.25f, buttonH * 0.5f, buttonH * 0.5f, countryColor(cur));
        text(safe.left() + pad + buttonH * 0.7f, safe.top() + (buttonH - (font ? font->lineHeight(ts) : 0.0f)) * 0.5f,
             buf, kWhite, ts);
        if (game->player >= 0) {
            std::snprintf(buf, sizeof buf, "Cities: %d", game->cityCount(game->player));
            const float tw = font ? font->textWidth(buf, ts) : 0.0f;
            text(safe.left() + safe.size.x - pad - tw, safe.top() + (buttonH - (font ? font->lineHeight(ts) : 0.0f)) * 0.5f,
                 buf, kWhite, ts);
        }

        // selected unit panel + last message
        float by = safe.top() + safe.size.y - pad;
        if (!message.empty()) {
            const float mh = font ? font->lineHeight(ts * 0.85f) : 20.0f;
            by -= mh + 8.0f;
            text(safe.left() + pad, by, message.c_str(), kGold, ts * 0.85f);
        }
        if (const ironfront::Unit* u = selected ? game->world.unitById(selected) : nullptr) {
            std::snprintf(buf, sizeof buf, "%s%s  HP %d/%d   %s   %s", ironfront::embarked(game->world, *u) ? "Transport: " : "",
                          u->st().name, u->hp, u->st().maxHp, u->moved ? "moved" : "can move",
                          u->attacked ? "attacked" : "can attack");
            const float bh = buttonH;
            rect(safe.left() + pad, by - bh - 6.0f, (font ? font->textWidth(buf, ts) : 300.0f) + 24.0f, bh, kPanel);
            text(safe.left() + pad + 12.0f, by - bh - 6.0f + (bh - (font ? font->lineHeight(ts) : 0.0f)) * 0.5f, buf, kWhite, ts);
        }
        if (playersTurn()) {
            if (button(endBtn, "End Turn", p, {0.18f, 0.45f, 0.25f, 1.0f})) endPlayerTurn();
        }
    }

    void updateTitle(const Pointer& p) {
        const float ts = textScale();
        textCentered(sw * 0.5f, sh * 0.2f, "IRON FRONT", kGold, ts * 2.6f);
        textCentered(sw * 0.5f, sh * 0.2f + (font ? font->lineHeight(ts * 2.6f) : 60.0f) + 8.0f,
                     "Europe, 1939. Five powers. One continent.", kWhite, ts);
        const float bw = std::min(sw * 0.7f, buttonH * 6.0f);
        Rect b{sw * 0.5f - bw * 0.5f, sh * 0.48f, bw, buttonH};
        if (button(b, "New Game", p, {0.18f, 0.45f, 0.25f, 1.0f})) screen = Screen::Pick;
        const bool canContinue = save && !save->getString("game").empty();
        b.y += buttonH * 1.4f;
        if (canContinue && button(b, "Continue", p, {0.22f, 0.32f, 0.55f, 1.0f})) {
            if (auto loaded = ironfront::deserialize(save->getString("game"))) {
                game = std::move(*loaded);
                beginPlay();
                if (game->isOver()) screen = Screen::Over;
            } else {
                message = "Saved game could not be loaded.";
            }
        }
    }

    void updatePick(const Pointer& p) {
        const float ts = textScale();
        textCentered(sw * 0.5f, safe.top() + buttonH * 0.6f, "Choose your nation", kGold, ts * 1.4f);
        const float bw = std::min(sw * 0.8f, buttonH * 7.0f);
        for (int c = 0; c < ironfront::CountryCount; ++c) {
            const Rect b{sw * 0.5f - bw * 0.5f, safe.top() + buttonH * (2.0f + 1.35f * static_cast<float>(c)), bw, buttonH};
            if (button(b, ironfront::countryName(c), p, scale(countryColor(c), 0.8f))) startGame(c);
        }
    }

    void updateOver(const Pointer& p, const platform::Input& in, float dt) {
        Pointer none; // the map stays visible but frozen
        updatePlaying(none, in, dt);
        rect(0, 0, sw, sh, {0, 0, 0, 0.55f});
        const float ts = textScale();
        const char* title = game->playerWon() ? "VICTORY" : game->playerLost() ? "DEFEAT" : "WAR OVER";
        textCentered(sw * 0.5f, sh * 0.3f, title, game->playerWon() ? kGold : kWhite, ts * 2.6f);
        char buf[120];
        const int win = game->winner();
        std::snprintf(buf, sizeof buf, "%s %s Europe after %d turns.", win >= 0 ? ironfront::countryName(win) : "Nobody",
                      win >= 0 ? "conquered" : "holds", game->turn);
        textCentered(sw * 0.5f, sh * 0.3f + (font ? font->lineHeight(ts * 2.6f) : 60.0f) + 8.0f, buf, kWhite, ts);
        const float bw = std::min(sw * 0.7f, buttonH * 6.0f);
        if (button({sw * 0.5f - bw * 0.5f, sh * 0.6f, bw, buttonH}, "New Game", p, {0.18f, 0.45f, 0.25f, 1.0f})) {
            if (save) {
                save->setString("game", "");
                save->save();
            }
            screen = Screen::Pick;
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ironfront headless=%d frames=%d", cfg.headless, cfg.frames);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Iron Front";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) return 1;

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) return 1;

    platform::PlatformRegistry registry = platform::defaultRegistry();
    std::unique_ptr<platform::PlatformBackend> backend = registry.create(platform::DesktopBackend::hostDesktopId());
    io::VirtualFileSystem vfs;
    auto* desktop = dynamic_cast<platform::DesktopBackend*>(backend.get());
    if (desktop) {
        desktop->attachWindow(window.sdl());
        desktop->init();
        desktop->mountStandard(vfs);
    }

    platform::Input input;
    core::FramePacer pacer;
    if (backend) pacer.setActiveFps(platform::recommendedFps(backend->powerState()));

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath = (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        if (!font.load(*renderer, fontPath.c_str(), 64.0f)) MAZ_LOG_WARN("ironfront: font not found at %s", fontPath.c_str());
    }

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    App app;
    app.r = renderer.get();
    app.font = &font;
    app.white = renderer->createTexture(1, 1, whitePx);
    app.backend = backend.get();
    app.headless = cfg.headless;

    core::KeyValueStore save;
    if (desktop && !cfg.headless) {
        save.load(desktop->directory(platform::DirKind::UserData) + "ironfront.ini");
        app.save = &save;
        backend->setOnSuspend([&] { app.writeSave(); });
    }

    if (cfg.headless) {
        // CI: an all-AI spectator game exercises the whole rules + AI path every frame.
        app.game = ironfront::Game::newGame(ironfront::Germany, 12345);
        app.game->player = ironfront::kNeutral;
        app.beginPlay();
    }

    Pointer pointer;
    int rendered = 0;
    auto frame = [&]() -> bool {
        const uint64_t frameStartNs = SDL_GetTicksNS();
        window.pumpEvents(input);
        if (desktop) desktop->syncLifecycle(window.isMinimized());

        uint32_t dw = 0, dh = 0;
        window.drawableSize(dw, dh);
        if (dw == 0 || dh == 0) {
            dw = static_cast<uint32_t>(cfg.width);
            dh = static_cast<uint32_t>(cfg.height);
        }
        app.sw = static_cast<float>(dw);
        app.sh = static_cast<float>(dh);
        app.safe = math::Rect2(0.0f, 0.0f, app.sw, app.sh);
        if (backend) app.safe = platform::safeAreaRect(static_cast<int>(dw), static_cast<int>(dh), backend->safeAreaInsets());
        app.buttonH = std::max(56.0f, platform::recommendedTouchTargetPx(160.0f * window.contentScale()));

        pointer.update(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE) && app.screen != Screen::Playing) {
            if (app.screen == Screen::Pick) app.screen = Screen::Title;
            else window.requestClose();
        }

        const float dt = 1.0f / 60.0f;
        renderer->setClearColor(render::Color{0.06f, 0.08f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);
            switch (app.screen) {
                case Screen::Title: app.updateTitle(pointer); break;
                case Screen::Pick: app.updatePick(pointer); break;
                case Screen::Playing:
                    app.updatePlaying(pointer, input, dt);
                    if (app.game && app.game->isOver()) app.screen = Screen::Over;
                    break;
                case Screen::Over: app.updateOver(pointer, input, dt); break;
            }
            renderer->endFrame();
        } else if (app.screen == Screen::Playing) {
            app.stepAi(dt); // no GPU (headless CI): the game still plays on
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) window.requestClose();

        if (backend) pacer.setActiveFps(platform::recommendedFps(backend->powerState()));
        const double workS = static_cast<double>(SDL_GetTicksNS() - frameStartNs) * 1e-9;
        const double sleepS = pacer.sleepFor(workS);
        if (sleepS > 0.0 && !cfg.headless) SDL_DelayNS(static_cast<Uint64>(sleepS * 1e9));
        return !window.shouldClose();
    };

    using FrameFn = decltype(frame);
    platform::runMainLoop([](void* u) -> bool { return (*static_cast<FrameFn*>(u))(); }, &frame);

    if (app.game)
        MAZ_LOG_INFO("ironfront: stopped after %d frames at turn %d, %d powers left", rendered, app.game->turn,
                     app.game->aliveCount());
    app.writeSave();
    if (desktop) desktop->shutdown();
    renderer->shutdown();
    window.shutdown();
    return 0;
}
