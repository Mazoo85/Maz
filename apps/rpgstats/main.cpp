// Maz Engine — "RPGSTATS" (game::Stat, game::Leveling, game::StatusEffect, game::Damage,
// game::Cooldown — the character-progression stack, toward Godot's lack of one)
// Five modules that only make sense together, shown as one character sheet mid-fight. LEFT: a stat with
// its modifier stack, so you can see flat, percent-add and percent-mult resolve in that fixed order and
// why the order matters. MIDDLE: an experience track on a geometric curve — the level it lands on, how
// far into it, and what the next one costs. RIGHT: a five-second fight simulated tick by tick — armour
// and resistance eating a hit through game::resolveDamage, a stacking poison ticking on its own interval
// through StatusEffectSystem, and two abilities coming off cooldown through CooldownManager, with the
// timeline printed as it happened. Nothing is random and no input changes it, so the picture IS the test
// and it holds still for a golden. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the five are in maz/Engine.hpp — that umbrella carries 153 of the engine's 692
// headers, and these are among the ones a game includes for itself.
#include "maz/game/Cooldown.hpp"
#include "maz/game/Damage.hpp"
#include "maz/game/Leveling.hpp"
#include "maz/game/Stat.hpp"
#include "maz/game/StatusEffect.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Ability and effect ids. Plain ints in the engine's API; named here so the demo reads.
constexpr int kStrike = 1;
constexpr int kHeal = 2;
constexpr int kPoison = 10;

std::string num(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string num(long long v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%lld", v);
    return buf;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RPGSTATS starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — RPG Stats";
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

    // ---- 1. A stat and its modifier stack -----------------------------------------------------------
    // Base 100 attack. A +15 flat weapon, two percent-add buffs that SUM before applying once, and a
    // percent-mult that applies on top of the result. The order is fixed by the engine, and it is the
    // reason 10% + 10% is not the same as 21%.
    game::Stat attack(100.0);
    attack.addModifier(15.0, game::ModifierType::Flat, 1);          // weapon
    attack.addModifier(0.10, game::ModifierType::PercentAdd, 2);    // rally aura
    attack.addModifier(0.10, game::ModifierType::PercentAdd, 3);    // banner
    attack.addModifier(0.20, game::ModifierType::PercentMult, 4);   // berserk

    const double attackFlatOnly = 115.0;
    const double attackAfterAdd = attackFlatOnly * 1.20;
    const double attackFinal = attack.value();

    // Dropping the banner removes only its modifier: sources are how a game un-buffs.
    game::Stat attackNoBanner = attack;
    attackNoBanner.removeModifiersFromSource(3);
    const double attackWithoutBanner = attackNoBanner.value();

    // ---- 2. Experience and levels -------------------------------------------------------------------
    // A geometric curve: 100 XP for level 2, each level 1.5x the last, capped at 30.
    game::ExperienceTrack xp{game::LevelCurve::geometric(100, 1.5, 30)};
    const int levelsGained = xp.addXp(2400);
    const int level = xp.level();
    const long long intoLevel = xp.xpIntoLevel();
    const long long nextCost = xp.xpForNextLevel();
    const float levelProgress = xp.progress();

    // ---- 3. Five seconds of a fight -----------------------------------------------------------------
    // Everything below is computed by the engine's own systems over a fixed timeline, then printed.
    game::StatusEffectSystem statuses;
    game::CooldownManager cooldowns;
    std::vector<std::string> timeline;
    long long healthLost = 0;

    // A 6-second poison, stacking, ticking once a second.
    statuses.apply(kPoison, 6.0, 1, 1.0, game::StackMode::Add, 5);
    timeline.push_back("0.0s  poison applied            1 stack");

    const double step = 0.25;
    for (int i = 1; i <= 20; ++i) {          // 20 x 0.25s = 5 seconds
        const double t = static_cast<double>(i) * step;
        cooldowns.tick(step);

        // Re-apply the poison at 2s: StackMode::Add means it stacks rather than merely refreshing.
        if (i == 8) {
            const int stacks = statuses.apply(kPoison, 6.0, 1, 1.0, game::StackMode::Add, 5);
            timeline.push_back(num(t) + "s  poison re-applied         " + std::to_string(stacks) +
                               " stacks");
        }

        // Strike whenever it is off cooldown: tryUse both checks and restarts it.
        if (cooldowns.tryUse(kStrike, 1.5)) {
            game::DamageInfo hit;
            hit.amount = attackFinal;
            hit.type = game::DamageType::Physical;
            hit.armor = 60.0;            // 100/(100+60) = 0.625 of the hit gets through
            hit.resist = 0.10;           // then 10% of what is left is resisted
            hit.flatReduction = 4.0;     // then 4 flat off the top
            // Strike lands at i = 1, 7, 13 and 19; 13 is chosen so the crit actually fires.
            hit.isCrit = (i == 13);
            const game::DamageResult r = game::resolveDamage(hit);
            healthLost += r.damage;
            timeline.push_back(num(t) + "s  strike -> " + num(r.damage) + " damage" +
                               (r.crit ? "        CRIT" : ""));
        }

        // Poison ticks are reported by the system, not counted by the caller.
        for (const game::StatusTick& tick : statuses.update(step)) {
            const long long poisonDamage = 3LL * tick.stacks * tick.ticks;
            healthLost += poisonDamage;
            timeline.push_back(num(t) + "s  poison ticks x" + std::to_string(tick.stacks) +
                               "        " + num(poisonDamage) + " damage");
        }

        // A heal on a long cooldown, used once.
        if (i == 10 && cooldowns.tryUse(kHeal, 8.0)) {
            timeline.push_back(num(t) + "s  heal used                 8.0s cooldown");
        }
    }

    const double strikeLeft = cooldowns.remaining(kStrike);
    const double healLeft = cooldowns.remaining(kHeal);
    const double healFraction = static_cast<double>(cooldowns.fraction(kHeal));
    const int poisonStacks = statuses.stacksOf(kPoison);
    const double poisonLeft = statuses.remainingOf(kPoison);

    // Two hits side by side, to show what the armour curve actually costs an attacker.
    game::DamageInfo bare;
    bare.amount = 100.0;
    bare.armor = 0.0;
    game::DamageInfo armoured;
    armoured.amount = 100.0;
    armoured.armor = 100.0;              // exactly half the damage gets through
    const long long bareHit = game::resolveDamage(bare).damage;
    const long long armouredHit = game::resolveDamage(armoured).damage;

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kHot{1.0f, 0.48f, 0.42f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};

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

            const float sz = 0.32f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RPG STATS", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "Stat + Leveling + StatusEffect + Damage + Cooldown, one character sheet",
                          kDim, 0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 210.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1: the modifier stack ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "game::Stat  -  ATTACK", kHead, 0.38f);
            y += 38.0f;
            row(24.0f, y, "base", num(attack.base(), 0), kVal); y += 28.0f;
            row(24.0f, y, "+15 flat (weapon)", num(attackFlatOnly, 0), kVal); y += 28.0f;
            row(24.0f, y, "+10% +10% summed", num(attackAfterAdd, 1), kVal); y += 28.0f;
            row(24.0f, y, "x1.20 mult (berserk)", num(attackFinal, 1), kOk); y += 34.0f;
            font.drawText(*renderer, 24.0f, y,
                          "flat, then percent-add summed once, then percent-mult in sequence", kDim,
                          0.27f);
            y += 32.0f;
            row(24.0f, y, "banner dropped", num(attackWithoutBanner, 1), kVal); y += 26.0f;
            font.drawText(*renderer, 24.0f, y, "removeModifiersFromSource(banner) — one buff, not all",
                          kDim, 0.27f);

            // ---- column 2: levels, and the armour curve ----
            y = 106.0f;
            font.drawText(*renderer, 470.0f, y, "game::Leveling  -  2400 XP", kHead, 0.38f);
            y += 38.0f;
            row(470.0f, y, "levels gained", std::to_string(levelsGained), kVal); y += 28.0f;
            row(470.0f, y, "level reached", std::to_string(level), kOk); y += 28.0f;
            row(470.0f, y, "into this level", num(intoLevel), kVal); y += 28.0f;
            row(470.0f, y, "next level costs", num(nextCost), kVal); y += 28.0f;
            row(470.0f, y, "progress", num(levelProgress * 100.0, 0) + "%", kVal); y += 34.0f;
            font.drawText(*renderer, 470.0f, y, "geometric curve: 100 XP, x1.5 each level, cap 30",
                          kDim, 0.27f);

            y += 46.0f;
            font.drawText(*renderer, 470.0f, y, "game::Damage  -  ARMOUR", kHead, 0.38f);
            y += 38.0f;
            row(470.0f, y, "100 raw, 0 armour", num(bareHit), kVal); y += 28.0f;
            row(470.0f, y, "100 raw, 100 armour", num(armouredHit), kHot); y += 28.0f;
            font.drawText(*renderer, 470.0f, y,
                          "100/(100+armour): diminishing, never reaching zero damage", kDim, 0.27f);

            // ---- column 3: the fight, as it happened ----
            y = 106.0f;
            font.drawText(*renderer, 900.0f, y, "FIVE SECONDS", kHead, 0.38f);
            y += 36.0f;
            const std::size_t shown = timeline.size() > 15 ? 15 : timeline.size();
            for (std::size_t i = 0; i < shown; ++i) {
                const bool crit = timeline[i].find("CRIT") != std::string::npos;
                font.drawText(*renderer, 900.0f, y, timeline[i].c_str(), crit ? kHot : kText, 0.28f);
                y += 24.0f;
            }

            y += 16.0f;
            row(900.0f, y, "total damage", num(healthLost), kHot); y += 26.0f;
            row(900.0f, y, "poison stacks left", std::to_string(poisonStacks), kVal); y += 26.0f;
            row(900.0f, y, "poison seconds left", num(poisonLeft), kVal); y += 26.0f;
            row(900.0f, y, "strike ready in", num(strikeLeft) + "s", kVal); y += 26.0f;
            row(900.0f, y, "heal ready in", num(healLeft) + "s  (" +
                               num(healFraction * 100.0, 0) + "%)", kVal);

            font.drawText(*renderer, 24.0f, 664.0f,
                          "Every number above is computed by the engine over a fixed timeline — no "
                          "randomness, no input, so the picture is the test.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RPGSTATS shutting down after %d damage (renderer %s)",
                 static_cast<int>(healthLost), renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
