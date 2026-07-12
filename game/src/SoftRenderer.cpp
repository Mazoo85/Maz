#include "zomboid/render/SoftRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "zomboid/render/Font.hpp"

namespace zb {

namespace {

constexpr Color kBackground = rgb(0x05060a);

Color tileColor(Tile t) {
    switch (t) {
    case Tile::Grass: return rgb(0x16361f);
    case Tile::Street: return rgb(0x1c1f26);
    case Tile::Sidewalk: return rgb(0x33363f);
    case Tile::Floor: return rgb(0x241b2e);
    case Tile::Wall: return rgb(0x3a2350);
    case Tile::Door: return rgb(0x0b3a3f);
    case Tile::Water: return rgb(0x0a1a3a);
    case Tile::Tree: return rgb(0x0f2a16);
    case Tile::Lot: return rgb(0x202028);
    case Tile::Rail: return rgb(0x2a2a30);
    }
    return rgb(0x101010);
}

Color zombieColor(ZombieTint tint) {
    switch (tint) {
    case ZombieTint::Sprinter: return neon::pink;
    case ZombieTint::GreenA: return rgb(0x4a7c3a);
    case ZombieTint::GreenB: return rgb(0x5a6b4a);
    }
    return rgb(0x4a7c3a);
}

// A centered, outlined "block sprite" like the reference blockSprite().
void blockSprite(Framebuffer& fb, int px, int py, int size, Color body, Color outline) {
    fb.fillRect(px - size / 2, py - size / 2, size, size, body);
    fb.outlineRect(px - size / 2, py - size / 2, size, size, outline);
}

// A labelled HUD stat bar (mirrors the reference bar()).
void statBar(Framebuffer& fb, int x, int y, int w, int h, float val, Color col, const char* label) {
    fb.fillRect(x - 1, y - 1, w + 2, h + 2, rgb(0x000000));
    fb.fillRect(x, y, w, h, rgb(0x15151c));
    const int fillw = static_cast<int>(static_cast<float>(w) * clampf(val / 100.0f, 0.0f, 1.0f));
    fb.fillRect(x, y, fillw, h, col);
    fb.outlineRect(x, y, w, h, col);
    drawText(fb, x + 3, y + (h - kGlyphH) / 2, label, rgb(0xffffff), 1);
}

// A short uppercase item tag for the hotbar (first letters of the name).
void shortLabel(const std::string& name, char* out, size_t cap) {
    size_t j = 0;
    for (char c : name) {
        if (j + 1 >= cap) break;
        if (c == ' ' || c == '\'' || c == '(' || c == ')') continue;
        out[j++] = c;
        if (j >= 4) break;
    }
    out[j] = '\0';
}

void renderHud(const Sim& sim, Framebuffer& fb) {
    const Player& p = sim.player();
    const int W = fb.width(), H = fb.height();

    // --- Stat stack, top-left ---
    const int bx = 8, bw = 150, bh = 11, gap = 5;
    int by = 8;
    statBar(fb, bx, by, bw, bh, p.health, p.hurtFlash > 0.0f ? rgb(0xffffff) : neon::green,
            "HEALTH");
    by += bh + gap;
    statBar(fb, bx, by, bw, bh, 100.0f - p.hunger, neon::orange, "FED");
    by += bh + gap;
    statBar(fb, bx, by, bw, bh, 100.0f - p.thirst, neon::cyan, "HYDRO");
    by += bh + gap;
    statBar(fb, bx, by, bw, bh, 100.0f - p.fatigue, neon::purple, "ENERGY");
    by += bh + gap;
    statBar(fb, bx, by, bw, bh, p.mood, neon::pink, "MOOD");
    by += bh + gap;
    if (p.infected) {
        statBar(fb, bx, by, bw, bh, p.infection, neon::red, "INFECTION");
    }

    // --- Clock / day / kills panel, top-right ---
    char buf[64];
    const int hh = static_cast<int>(sim.dayTime()) / 60;
    const int mm = static_cast<int>(sim.dayTime()) % 60;
    int py = 8;
    std::snprintf(buf, sizeof(buf), "DAY %d", sim.day());
    drawText(fb, W - textWidth(buf, 1) - 8, py, buf, neon::cyan, 1);
    py += 12;
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
    drawText(fb, W - textWidth(buf, 1) - 8, py, buf, neon::yellow, 1);
    py += 12;
    std::snprintf(buf, sizeof(buf), "KILLS %d", sim.kills());
    drawText(fb, W - textWidth(buf, 1) - 8, py, buf, neon::pink, 1);
    py += 12;
    std::snprintf(buf, sizeof(buf), "Z ALIVE %d", sim.aliveZombies());
    drawText(fb, W - textWidth(buf, 1) - 8, py, buf, neon::green, 1);

    // --- Equipped weapon (+ ammo), bottom-center ---
    const ItemDef& wep = itemDef(p.weapon);
    char wtag[8];
    shortLabel(wep.name, wtag, sizeof(wtag));
    if (wep.ranged) {
        int ammoQty = 0;
        for (const auto& s : p.inv)
            if (s.id == wep.ammo) ammoQty = s.qty;
        std::snprintf(buf, sizeof(buf), "%s [%d]", wtag, ammoQty);
    } else {
        std::snprintf(buf, sizeof(buf), "%s", wtag);
    }
    drawText(fb, W / 2 - textWidth(buf, 2) / 2, H - 78, buf, neon::yellow, 2);

    // --- Hotbar, bottom-center ---
    const int n = p.slots;
    const int slotW = 40, sgap = 4;
    const int total = n * slotW + (n - 1) * sgap;
    const int x0 = W / 2 - total / 2;
    const int hy = H - 58;
    for (int i = 0; i < n; i++) {
        const int x = x0 + i * (slotW + sgap);
        fb.fillRect(x, hy, slotW, slotW, rgb(0x0a0d14));
        const bool has = i < static_cast<int>(p.inv.size());
        const bool equipped = has && p.inv[static_cast<size_t>(i)].id == p.weapon;
        fb.outlineRect(x, hy, slotW, slotW, equipped ? neon::yellow : rgb(0x2a6c74));
        char idx[2] = {static_cast<char>('1' + i), '\0'};
        drawText(fb, x + 2, hy + 2, idx, rgb(0x9999aa), 1);
        if (has) {
            const InvSlot& slot = p.inv[static_cast<size_t>(i)];
            char tag[8];
            shortLabel(itemDef(slot.id).name, tag, sizeof(tag));
            drawText(fb, x + 3, hy + slotW / 2 - 3, tag, rgb(0xffffff), 1);
            if (slot.qty > 1) {
                std::snprintf(buf, sizeof(buf), "%d", slot.qty);
                drawText(fb, x + slotW - textWidth(buf, 1) - 2, hy + slotW - 9, buf, neon::yellow,
                         1);
            }
        }
    }

    // --- Message log, bottom-left ---
    const auto& msgs = sim.messages();
    int my = H - 20;
    for (int i = static_cast<int>(msgs.size()) - 1;
         i >= 0 && i >= static_cast<int>(msgs.size()) - 5; i--) {
        char line[48];
        std::snprintf(line, sizeof(line), "%s", msgs[static_cast<size_t>(i)].text.c_str());
        drawText(fb, 8, my, line, rgb(0xd0d0e0), 1);
        my -= 11;
    }
}

} // namespace

void renderScene(const Sim& sim, Framebuffer& fb, const RenderOptions& opts) {
    const World& world = sim.world();
    const Player& player = sim.player();
    const int TILE = opts.tilePx;
    const float TILEf = static_cast<float>(TILE);
    const int W = fb.width(), H = fb.height();

    // Camera in pixel space (mirrors updateCamera): center on the player.
    float camX = 0.0f, camY = 0.0f;
    if (opts.centerPlayer) {
        camX = player.pos.x * TILEf - static_cast<float>(W) / 2.0f;
        camY = player.pos.y * TILEf - static_cast<float>(H) / 2.0f;
    }

    fb.clear(kBackground);

    auto sx = [&](float wxTiles) { return static_cast<int>(std::lround(wxTiles * TILEf - camX)); };
    auto sy = [&](float wyTiles) { return static_cast<int>(std::lround(wyTiles * TILEf - camY)); };

    // Visible tile range (+1 margin), clamped to the map.
    const int x0 = std::max(0, static_cast<int>(std::floor(camX / TILEf)) - 1);
    const int y0 = std::max(0, static_cast<int>(std::floor(camY / TILEf)) - 1);
    const int x1 = std::min(world.w, x0 + W / TILE + 3);
    const int y1 = std::min(world.h, y0 + H / TILE + 3);

    // --- Tiles + neon detailing ---
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            const Tile t = world.at(x, y);
            const int px = sx(static_cast<float>(x)), py = sy(static_cast<float>(y));
            fb.fillRect(px, py, TILE, TILE, tileColor(t));

            switch (t) {
            case Tile::Street:
                if ((x + y) % 2 == 0)
                    fb.blendRect(px + TILE / 2 - 1, py + 2, 2, TILE - 4, neon::yellow, 0.10f);
                break;
            case Tile::Sidewalk:
                fb.outlineRect(px, py, TILE, TILE, rgb(0x0a2c30));
                break;
            case Tile::Wall:
                fb.outlineRect(px + 1, py + 1, TILE - 2, TILE - 2, neon::purple);
                break;
            case Tile::Door:
                fb.blendRect(px + 3, py + 3, TILE - 6, TILE - 6, neon::green, 0.5f);
                break;
            case Tile::Water:
                fb.blendRect(px, py + TILE / 2, TILE, 2, neon::blue, 0.14f);
                break;
            case Tile::Tree:
                fb.fillCircle(px + TILE / 2, py + TILE / 2, static_cast<int>(TILEf * 0.42f),
                              rgb(0x1c5a2a));
                fb.blendRect(px + TILE / 4, py + TILE / 4, TILE / 2, TILE / 2, neon::green, 0.18f);
                break;
            case Tile::Rail:
                fb.blendRect(px, py + TILE / 3, TILE, 1, neon::orange, 0.4f);
                fb.blendRect(px, py + 2 * TILE / 3, TILE, 1, neon::orange, 0.4f);
                break;
            default: break;
            }
        }
    }

    // --- Loot containers ---
    for (const auto& c : world.containers) {
        const int px = sx(static_cast<float>(c.x)), py = sy(static_cast<float>(c.y));
        if (px < -TILE || px > W || py < -TILE || py > H) continue;
        const Color body = c.opened ? rgb(0x3a3a44) : neon::orange;
        fb.fillRect(px + TILE / 4, py + TILE / 4, TILE / 2, TILE / 2, body);
        fb.outlineRect(px + TILE / 4, py + TILE / 4, TILE / 2, TILE / 2,
                       c.opened ? rgb(0x222222) : rgb(0xffffff));
    }

    // --- Corpses ---
    for (const auto& corpse : sim.corpses()) {
        const int px = sx(corpse.pos.x), py = sy(corpse.pos.y);
        const float a = std::max(0.15f, 1.0f - corpse.t / 60.0f);
        fb.blendRect(px - TILE / 2, py - TILE / 3, TILE, 2 * TILE / 3, rgb(0x5a1020), a);
    }

    // --- Blood particles (under entities) ---
    for (const auto& p : sim.particles()) {
        if (p.kind != ParticleKind::Blood) continue;
        const int px = sx(p.pos.x), py = sy(p.pos.y);
        const int r = std::max(1, static_cast<int>(p.r));
        fb.blendRect(px - r / 2, py - r / 2, r, r, neon::red, std::max(0.0f, p.life * 2.0f));
    }

    // --- Zombies ---
    for (const auto& z : sim.zombies()) {
        if (z.dead) continue;
        const int px = sx(z.pos.x), py = sy(z.pos.y);
        if (px < -TILE || px > W + TILE || py < -TILE || py > H + TILE) continue;
        const int sz = static_cast<int>(TILEf * 0.62f);
        blockSprite(fb, px, py, sz, zombieColor(z.tint), rgb(0x0a0a0a));
        // Eyes toward facing.
        const int ex = static_cast<int>(std::cos(z.dir) * 4.0f);
        const int ey = static_cast<int>(std::sin(z.dir) * 4.0f);
        const Color eye = z.sprinter ? neon::yellow : neon::green;
        fb.fillRect(px + ex - 3, py + ey - 2, 2, 2, eye);
        fb.fillRect(px + ex + 1, py + ey - 2, 2, 2, eye);
        // Health bar if hurt.
        if (z.health < z.max) {
            fb.fillRect(px - sz / 2, py - sz / 2 - 4, sz, 2, rgb(0x000000));
            const int hw = static_cast<int>(static_cast<float>(sz) * std::max(0.0f, z.health / z.max));
            fb.fillRect(px - sz / 2, py - sz / 2 - 4, hw, 2, neon::red);
        }
    }

    // --- Bullets ---
    for (const auto& b : sim.bullets()) {
        const int px = sx(b.pos.x), py = sy(b.pos.y);
        fb.drawLine(px, py, px - static_cast<int>(b.vx * TILEf * 0.03f),
                    py - static_cast<int>(b.vy * TILEf * 0.03f), neon::yellow, 2);
    }

    // --- Player ---
    {
        const int px = sx(player.pos.x), py = sy(player.pos.y);
        const int sz = static_cast<int>(TILEf * 0.6f);
        const Color flash = player.hurtFlash > 0.0f ? neon::red : neon::cyan;
        blockSprite(fb, px, py, sz, rgb(0x11212b), flash);
        // Facing weapon line.
        const ItemDef& wep = itemDef(player.weapon);
        const Color line = wep.ranged ? neon::yellow : neon::cyan;
        fb.drawLine(px, py, px + static_cast<int>(std::cos(player.dir) * TILEf * 0.7f),
                    py + static_cast<int>(std::sin(player.dir) * TILEf * 0.7f), line, 3);
    }

    // --- Muzzle particles (over entities) ---
    for (const auto& p : sim.particles()) {
        if (p.kind != ParticleKind::Muzzle) continue;
        const int px = sx(p.pos.x), py = sy(p.pos.y);
        const int r = std::max(1, static_cast<int>(p.r));
        fb.blendRect(px - r / 2, py - r / 2, r, r, neon::yellow, std::max(0.0f, p.life * 4.0f));
    }

    // --- Floating damage numbers / loot pickups ---
    for (const auto& f : sim.floatTexts()) {
        const int px = sx(f.pos.x) - textWidth(f.text.c_str(), 1) / 2;
        const int py = sy(f.pos.y);
        const Color col = f.kind == FloatKind::Damage ? neon::yellow : neon::green;
        drawText(fb, px, py, f.text.c_str(), col, 1);
    }

    // --- Night darkness overlay ---
    if (opts.applyNight) {
        const float d = sim.darknessAlpha();
        if (d > 0.01f) fb.overlay(rgb(0x020414), d);
    }

    // --- HUD (drawn on top, unaffected by darkness) ---
    if (opts.hud) renderHud(sim, fb);
}

void renderWorldMap(const Sim& sim, Framebuffer& fb) {
    const World& world = sim.world();
    const int W = fb.width(), H = fb.height();
    fb.clear(kBackground);

    const float sxs = static_cast<float>(W) / static_cast<float>(world.w);
    const float sys = static_cast<float>(H) / static_cast<float>(world.h);
    const int cw = std::max(1, static_cast<int>(std::ceil(sxs)));
    const int ch = std::max(1, static_cast<int>(std::ceil(sys)));

    for (int y = 0; y < world.h; y++) {
        for (int x = 0; x < world.w; x++) {
            const int px = static_cast<int>(static_cast<float>(x) * sxs);
            const int py = static_cast<int>(static_cast<float>(y) * sys);
            fb.fillRect(px, py, cw, ch, tileColor(world.at(x, y)));
        }
    }
    // Container blips (orange = unopened).
    for (const auto& c : world.containers) {
        const int px = static_cast<int>(static_cast<float>(c.x) * sxs);
        const int py = static_cast<int>(static_cast<float>(c.y) * sys);
        fb.fillRect(px, py, std::max(1, cw), std::max(1, ch),
                    c.opened ? rgb(0x3a3a44) : neon::orange);
    }
    // Zombie blips (red) + player (yellow).
    for (const auto& z : sim.zombies()) {
        if (z.dead) continue;
        fb.fillRect(static_cast<int>(z.pos.x * sxs), static_cast<int>(z.pos.y * sys), 2, 2,
                    neon::red);
    }
    const Player& p = sim.player();
    fb.fillCircle(static_cast<int>(p.pos.x * sxs), static_cast<int>(p.pos.y * sys), 3, neon::yellow);
}

} // namespace zb
