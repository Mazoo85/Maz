// Maz Engine — "XML" (io::XmlParser, toward Godot's XMLParser)
// A pull/streaming XML reader: read() walks the document node by node (elements, attributes, text,
// comments, self-closing tags) instead of building a DOM. This demo feeds a small embedded level-style
// document through the parser and renders exactly what the parser reports — one line per node, indented
// by the parser's depth(), with elements, attribute names, attribute values, text, and comments each in
// their own colour. It is a faithful readout of the token stream (not a re-formatter), so it proves the
// parser classifies every node, nests depth correctly, and entity-decodes values. Static -> deterministic,
// golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

// One coloured run of text on a line.
struct Span {
    std::string text;
    render::Color color;
};

struct Line {
    int depth = 0;
    std::vector<Span> spans;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("XML (io::XmlParser) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — XML Pull Parser";
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

    // A small level-style document exercising nesting, attributes, self-closing tags, an entity, text,
    // and a comment.
    const std::string doc =
        "<?xml version=\"1.0\"?>"
        "<level name=\"Ruins &amp; Rooftops\" gravity=\"980\">"
        "<!-- spawn points and props -->"
        "<player x=\"64\" y=\"128\" hp=\"100\"/>"
        "<enemies count=\"3\">"
        "<enemy type=\"bat\" x=\"300\" y=\"90\"/>"
        "<enemy type=\"slime\" x=\"512\" y=\"256\"/>"
        "</enemies>"
        "<sign text=\"Beware: 5 &lt; foes\"/>"
        "</level>";

    // Colours per role.
    const render::Color kElem{0.55f, 0.82f, 1.0f, 1.0f};    // element name / brackets
    const render::Color kAttr{0.75f, 0.92f, 0.6f, 1.0f};    // attribute name
    const render::Color kValue{1.0f, 0.72f, 0.45f, 1.0f};   // attribute value
    const render::Color kText{0.92f, 0.9f, 0.78f, 1.0f};    // text content
    const render::Color kComment{0.55f, 0.58f, 0.66f, 1.0f}; // comments
    const render::Color kEnd{0.5f, 0.68f, 0.86f, 1.0f};     // end tags (dimmer element colour)
    const render::Color kDecl{0.7f, 0.6f, 0.85f, 1.0f};     // <?xml ?>

    // Walk the document once and build the display lines from what the parser reports.
    std::vector<Line> lines;
    {
        io::XmlParser p;
        p.parse(doc);
        while (p.read()) {
            Line ln;
            ln.depth = p.depth();
            switch (p.nodeType()) {
            case io::XmlParser::NodeType::Declaration:
                ln.spans.push_back({"<?" + p.nodeData() + "?>", kDecl});
                break;
            case io::XmlParser::NodeType::Element: {
                ln.spans.push_back({"<" + p.nodeName(), kElem});
                for (std::size_t i = 0; i < p.attributeCount(); ++i) {
                    ln.spans.push_back({" " + p.attributeName(i) + "=", kAttr});
                    ln.spans.push_back({"\"" + p.attributeValue(i) + "\"", kValue});
                }
                ln.spans.push_back({p.isEmpty() ? "/>" : ">", kElem});
                break;
            }
            case io::XmlParser::NodeType::ElementEnd:
                ln.spans.push_back({"</" + p.nodeName() + ">", kEnd});
                break;
            case io::XmlParser::NodeType::Text:
                ln.spans.push_back({p.nodeData(), kText});
                break;
            case io::XmlParser::NodeType::Comment:
                ln.spans.push_back({"<!--" + p.nodeData() + "-->", kComment});
                break;
            default:
                break;
            }
            if (!ln.spans.empty()) {
                lines.push_back(ln);
            }
        }
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  XML PULL PARSER", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "io::XmlParser (Godot XMLParser): read() walks nodes one at a time - each line "
                          "below is one node the parser returned, indented by its depth()",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const float x0 = 40.0f;
            const float y0 = 110.0f;
            const float lineH = 30.0f;
            const float indent = 34.0f;
            const float scale = 0.42f;
            for (std::size_t i = 0; i < lines.size(); ++i) {
                const Line& ln = lines[i];
                float x = x0 + indent * static_cast<float>(ln.depth);
                const float y = y0 + lineH * static_cast<float>(i);
                for (const Span& sp : ln.spans) {
                    font.drawText(*renderer, x, y, sp.text.c_str(), sp.color, scale);
                    x += font.textWidth(sp.text.c_str(), scale);
                }
            }

            // Legend.
            const float ly = 682.0f;
            font.drawText(*renderer, 16.0f, ly, "element", kElem, 0.34f);
            font.drawText(*renderer, 150.0f, ly, "attr=", kAttr, 0.34f);
            font.drawText(*renderer, 250.0f, ly, "\"value\"", kValue, 0.34f);
            font.drawText(*renderer, 370.0f, ly, "text", kText, 0.34f);
            font.drawText(*renderer, 470.0f, ly, "<!--comment-->", kComment, 0.34f);
            font.drawText(*renderer, 690.0f, ly, "entities decoded: & and <", rgba(0.7f, 0.75f, 0.85f, 1),
                          0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("XML shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
