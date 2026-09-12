// Maz Engine — "INIFILE" (io::ConfigFile, toward Godot's ConfigFile)
// Godot's ConfigFile is the INI-style `[section]` + `key=value` store behind project settings, input maps,
// and hand-editable options/save files. This demo parses a game settings.cfg (with a global section,
// comments, quoted strings and typed values), renders it as a grouped table on the left, then mutates it
// (bumps a version, changes the resolution, mutes audio, adds a key) and shows the re-encoded INI text on
// the right — proving a lossless parse -> edit -> encode round-trip. Static input -> deterministic,
// golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, const ui::Rect& b, render::Color col) {
    const render::Point2 p[4] = {{b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, {b.x, b.y + b.h}};
    r.drawConvexPolygon(p, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("INIFILE (io::ConfigFile) starting");

    // A hand-editable settings file (as it might arrive on disk).
    const std::string source =
        "; user settings\n"
        "version = 4\n"
        "\n"
        "[video]\n"
        "fullscreen = true\n"
        "resolution = \"1280x720\"\n"
        "vsync = false\n"
        "gamma = 2.2\n"
        "\n"
        "[audio]\n"
        "master = 0.80\n"
        "music = 0.60\n"
        "muted = no\n"
        "\n"
        "[input]\n"
        "jump = space\n"
        "fire = mouse_left\n";

    io::ConfigFile ini;
    ini.parse(source);

    // Edit it: bump version, change resolution, mute audio, add a key.
    ini.setInt("", "version", 5);
    ini.setValue("video", "resolution", "1920x1080");
    ini.setBool("audio", "muted", true);
    ini.setValue("input", "crouch", "ctrl");
    const std::string encoded = ini.encode();

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ConfigFile";
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

    const render::Color label{0.82f, 0.86f, 0.94f, 1.0f};
    const render::Color sectionCol{0.95f, 0.8f, 0.4f, 1.0f};
    const render::Color keyCol{0.7f, 0.8f, 1.0f, 1.0f};
    const render::Color valCol{0.85f, 0.9f, 0.8f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.07f, 0.1f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CONFIGFILE",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "INI [section] + key=value settings - Godot's ConfigFile (project settings / "
                          "options) (io::ConfigFile)",
                          label, 0.4f);

            // --- Left: the parsed + edited store as a grouped table. ---
            font.drawText(*renderer, 60.0f, 96.0f, "parsed + edited (typed get/set)", keyCol, 0.4f);
            const ui::Rect panel{60.0f, 128.0f, 520.0f, 540.0f};
            fillRect(*renderer, panel, render::Color{0.11f, 0.12f, 0.16f, 1.0f});

            float y = panel.y + 16.0f;
            for (const std::string& sec : ini.sections()) {
                const std::string header = sec.empty() ? "(global)" : "[" + sec + "]";
                font.drawText(*renderer, panel.x + 16.0f, y, header.c_str(), sectionCol, 0.4f);
                y += 30.0f;
                for (const std::string& key : ini.sectionKeys(sec)) {
                    font.drawText(*renderer, panel.x + 36.0f, y, key.c_str(), keyCol, 0.34f);
                    font.drawText(*renderer, panel.x + 250.0f, y, "=", label, 0.34f);
                    font.drawText(*renderer, panel.x + 272.0f, y, ini.getValue(sec, key).c_str(), valCol,
                                  0.34f);
                    y += 26.0f;
                }
                y += 8.0f;
            }

            // --- Right: the re-encoded INI text. ---
            font.drawText(*renderer, 620.0f, 96.0f, "encode() -> text (round-trips through parse)", keyCol,
                          0.4f);
            const ui::Rect textPanel{620.0f, 128.0f, 600.0f, 540.0f};
            fillRect(*renderer, textPanel, render::Color{0.09f, 0.1f, 0.13f, 1.0f});

            float ty = textPanel.y + 14.0f;
            std::size_t start = 0;
            while (start <= encoded.size()) {
                const std::size_t nl = encoded.find('\n', start);
                const std::size_t end = (nl == std::string::npos) ? encoded.size() : nl;
                const std::string line = encoded.substr(start, end - start);
                const bool isHeader = !line.empty() && line.front() == '[';
                font.drawText(*renderer, textPanel.x + 16.0f, ty, line.c_str(),
                              isHeader ? sectionCol : render::Color{0.78f, 0.82f, 0.9f, 1}, 0.32f);
                ty += 21.0f;
                if (nl == std::string::npos) {
                    break;
                }
                start = nl + 1;
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("INIFILE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
