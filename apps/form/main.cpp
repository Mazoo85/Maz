// Maz Engine — "FORM" (UI text input + focus navigation, toward Godot's LineEdit + Control focus)
// A settings form of editable single-line fields (ui::TextField) wired into a focus chain
// (ui::FocusChain). Click a field or press Tab / Shift+Tab to move focus; the focused field shows a
// caret and accepts typed characters, Backspace/Delete, arrows, and Home/End. The demo maps SDL
// scancodes to characters and feeds the immediate-mode ui::Context::textField widget, which does the
// drawing + click-to-focus. Deterministic initial state (fields pre-filled, one focused with a caret),
// so the render is golden-stable; live typing works in a window. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Append the character a just-pressed key produces (letters, digits, space, a few punctuation) so the
// demo can type into the focused field without the platform layer exposing SDL text input.
void gatherTyped(const platform::Input& in, std::string& out) {
    const bool shift = in.keyDown(SDL_SCANCODE_LSHIFT) || in.keyDown(SDL_SCANCODE_RSHIFT);
    for (int sc = SDL_SCANCODE_A; sc <= SDL_SCANCODE_Z; ++sc) {
        if (in.keyPressed(sc)) {
            out.push_back(static_cast<char>((shift ? 'A' : 'a') + (sc - SDL_SCANCODE_A)));
        }
    }
    static const char* digits = "1234567890";
    for (int sc = SDL_SCANCODE_1; sc <= SDL_SCANCODE_0; ++sc) {
        if (in.keyPressed(sc)) {
            out.push_back(digits[sc - SDL_SCANCODE_1]);
        }
    }
    if (in.keyPressed(SDL_SCANCODE_SPACE)) out.push_back(' ');
    if (in.keyPressed(SDL_SCANCODE_PERIOD)) out.push_back('.');
    if (in.keyPressed(SDL_SCANCODE_MINUS)) out.push_back(shift ? '_' : '-');
}

struct Field {
    std::string label;
    ui::TextField value;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FORM (UI text input) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Text Input Form";
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
    render::TextureHandle white = render::kInvalidTexture;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
        const uint8_t px[4] = {255, 255, 255, 255};
        white = renderer->createTexture(1, 1, px);
    }

    ui::Context gui;
    gui.init(*renderer, font, white);

    std::vector<Field> fields = {
        {"Player Name", ui::TextField("Ada Lovelace")},
        {"Server", ui::TextField("maz.local")},
        {"Port", ui::TextField("7777")},
        {"Message", ui::TextField("gg wp")},
    };
    fields[2].value.setMaxLength(5); // port: at most 5 chars

    ui::FocusChain focus;
    for (uint32_t i = 0; i < fields.size(); ++i) {
        focus.add(i + 1);
    }
    focus.focus(2); // start focused on "Server", caret at end (its initial state)

    const float sw = static_cast<float>(cfg.width);

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }
        // Tab / Shift+Tab move focus between fields.
        if (input.keyPressed(SDL_SCANCODE_TAB)) {
            if (input.keyDown(SDL_SCANCODE_LSHIFT) || input.keyDown(SDL_SCANCODE_RSHIFT)) {
                focus.prev();
            } else {
                focus.next();
            }
        }

        std::string typed;
        gatherTyped(input, typed);
        ui::TextEditInput edit;
        edit.typed = typed.empty() ? nullptr : typed.c_str();
        edit.backspace = input.keyPressed(SDL_SCANCODE_BACKSPACE);
        edit.del = input.keyPressed(SDL_SCANCODE_DELETE);
        edit.left = input.keyPressed(SDL_SCANCODE_LEFT);
        edit.right = input.keyPressed(SDL_SCANCODE_RIGHT);
        edit.home = input.keyPressed(SDL_SCANCODE_HOME);
        edit.end = input.keyPressed(SDL_SCANCODE_END);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            gui.begin(input.mouseX(), input.mouseY(), input.mouseDown(0));

            const float panelW = 560.0f, panelH = 466.0f;
            const float px = sw * 0.5f - panelW * 0.5f, py = 110.0f;
            gui.panel(ui::Rect{px, py, panelW, panelH}, gui.colBg);

            font.drawText(*renderer, px + 28.0f, py + 22.0f, "ACCOUNT SETTINGS",
                          render::Color{1, 1, 1, 1}, 0.62f);

            const float fx = px + 28.0f, fw = panelW - 56.0f, fh = 42.0f;
            float y = py + 74.0f;
            for (uint32_t i = 0; i < fields.size(); ++i) {
                font.drawText(*renderer, fx, y, fields[i].label.c_str(),
                              render::Color{0.7f, 0.78f, 0.9f, 1}, 0.44f);
                gui.textField(i + 1, ui::Rect{fx, y + 26.0f, fw, fh}, fields[i].value, focus, edit,
                              0.5f);
                y += 80.0f;
            }

            font.drawText(*renderer, fx, py + panelH - 34.0f,
                          "Tab / Shift+Tab move  -  click a field to focus  -  type to edit",
                          render::Color{0.6f, 0.66f, 0.78f, 1}, 0.4f);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  UI TEXT INPUT + FOCUS",
                          render::Color{1, 1, 1, 1}, 0.7f);

            gui.end();
            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FORM shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
