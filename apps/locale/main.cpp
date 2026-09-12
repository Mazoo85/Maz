// Maz Engine — "LOCALE" (CSV localization, toward Godot's Translation / CSV import)
// One in-memory translation CSV (keys + en/es/fr/de columns) drives four side-by-side renderings of the
// same game menu — each panel calls TranslationTable::tr(key) after setLocale(...) so every string comes
// from the table, never a hard-coded literal. The QUIT row is intentionally missing its German cell to
// show the fallback: an empty translation degrades to the source (English) text rather than a blank.
// All static → deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillBox(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LOCALE (CSV localization) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Localization";
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
        font.load(*renderer, fontPath.c_str(), 32.0f);
    }

    // A Godot-style translation CSV: first column = key, the rest = one locale each. The QUIT row's de
    // cell is empty on purpose (fallback demo); TITLE has a quoted field with an embedded comma.
    const char* csv =
        "keys,en,es,fr,de\n"
        "TITLE,\"Maz, the Game\",\"Maz, el Juego\",\"Maz, le Jeu\",\"Maz, das Spiel\"\n"
        "NEW_GAME,New Game,Nuevo Juego,Nouvelle Partie,Neues Spiel\n"
        "CONTINUE,Continue,Continuar,Continuer,Fortsetzen\n"
        "OPTIONS,Options,Opciones,Options,Optionen\n"
        "QUIT,Quit,Salir,Quitter,\n"; // de empty -> falls back to English "Quit"

    io::TranslationTable tr;
    tr.loadCsv(csv);

    const std::array<const char*, 4> localeNames = {"en", "es", "fr", "de"};
    const std::array<const char*, 4> localeLabels = {"English", "Espanol", "Francais", "Deutsch"};
    const std::array<const char*, 4> menuKeys = {"NEW_GAME", "CONTINUE", "OPTIONS", "QUIT"};

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kPanel{0.14f, 0.16f, 0.22f, 1.0f};
    const render::Color kHeader{0.22f, 0.34f, 0.52f, 1.0f};
    const render::Color kItem{0.20f, 0.23f, 0.30f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1.0f};
    const render::Color kDim{0.68f, 0.74f, 0.86f, 1.0f};
    const render::Color kFallback{0.85f, 0.62f, 0.36f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  LOCALIZATION (Translation CSV)", kText,
                          0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one CSV, four locales  -  every label via TranslationTable::tr(key)", kDim,
                          0.34f);

            const float panelW = 292.0f;
            const float gap = 18.0f;
            const float x0 = 24.0f;
            for (int c = 0; c < 4; ++c) {
                tr.setLocale(localeNames[static_cast<std::size_t>(c)]);
                const float x = x0 + static_cast<float>(c) * (panelW + gap);

                fillBox(*renderer, x, 96.0f, panelW, 560.0f, kPanel);
                // Header: the locale name + the (translated) game title.
                fillBox(*renderer, x, 96.0f, panelW, 84.0f, kHeader);
                font.drawText(*renderer, x + 16.0f, 108.0f,
                              localeLabels[static_cast<std::size_t>(c)], kText, 0.44f);
                font.drawText(*renderer, x + 16.0f, 144.0f, tr.tr("TITLE").c_str(), kDim, 0.32f);

                // Menu items — each pulled from the table for the active locale.
                for (int i = 0; i < 4; ++i) {
                    const float y = 208.0f + static_cast<float>(i) * 88.0f;
                    fillBox(*renderer, x + 16.0f, y, panelW - 32.0f, 64.0f, kItem);
                    const char* key = menuKeys[static_cast<std::size_t>(i)];
                    const std::string label = tr.tr(key);
                    // QUIT in German is empty in the CSV → tr() returns English; flag it.
                    const bool fellBack =
                        (std::string(key) == "QUIT" && std::string(localeNames[static_cast<std::size_t>(c)]) == "de");
                    font.drawText(*renderer, x + 32.0f, y + 18.0f, label.c_str(),
                                  fellBack ? kFallback : kText, 0.42f);
                }
            }

            font.drawText(*renderer, 24.0f, 668.0f,
                          "Deutsch QUIT cell is empty -> falls back to English (orange)", kDim, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LOCALE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
