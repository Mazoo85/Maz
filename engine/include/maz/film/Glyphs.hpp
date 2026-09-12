#pragma once

#include "maz/film/Canvas.hpp"
#include "maz/film/Palette.hpp"

#include <string>
#include <vector>

// maz::film glyphs — the thing the story turns on, lit on a dark surface. Ported from GLYPHS and
// glyphFor() in film/js/film-figures.js.
//
// An INSERT shot is the one shot in a film with no people in it: the camera goes to the object -- the
// letter, the photograph, the gun, the tape -- and holds on it. The reel names the object as a word
// ("a message in a bottle" gives "message"), and that word picks one of ten drawn shapes.
//
// Drawn around the origin, in world units, at the scale the insert shot places them.
namespace maz::film {

namespace detail {

inline void glyphRadio(Canvas& c, const Palette& p) {
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.95f});
    c.fillRect(-140, -80, 280, 160);
    c.setFill(p.key, 0.85f);
    c.fillRect(-118, -58, 150, 60);
    c.setFill(p.accent, 0.9f);
    c.fillRect(-110, 16, 190, 8);
    c.setFill(p.key);
    c.beginPath();
    c.circle(90, -30, 22);
    c.fill();
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.9f});
    c.fillRect(86, -52, 8, 24);
}

inline void glyphPhone(Canvas& c, const Palette& p) {
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.95f});
    c.fillRect(-70, -120, 140, 240);
    c.setFill(p.key, 0.8f);
    c.fillRect(-56, -104, 112, 190);
    c.setFill(p.accent, 0.9f);
    c.fillRect(-36, -60, 72, 10);
    c.fillRect(-36, -34, 52, 10);
}

inline void glyphLetter(Canvas& c, const Palette& p) {
    c.setFill(p.key, 0.9f);
    c.fillRect(-150, -100, 300, 200);
    c.setStroke(render::Color{0.0f, 0.0f, 0.0f, 0.85f});
    c.setLineWidth(8);
    c.beginPath();
    c.moveTo(-150, -100);
    c.lineTo(0, 10);
    c.lineTo(150, -100);
    c.stroke();
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.25f});
    for (int i = 0; i < 4; ++i) {
        c.fillRect(-110, static_cast<float>(30 + i * 18), static_cast<float>(220 - i * 40), 6);
    }
}

inline void glyphPhotograph(Canvas& c, const Palette& p) {
    c.setFill(p.key, 0.92f);
    c.fillRect(-150, -110, 300, 220);
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.8f});
    c.fillRect(-130, -90, 260, 150);
    c.setFill(p.accent, 0.5f);
    c.beginPath();
    c.circle(-40, -20, 34);
    c.fill();
    c.beginPath();
    c.circle(50, -14, 30);
    c.fill();
}

inline void glyphKey(Canvas& c, const Palette& p) {
    c.setStroke(p.key, 0.95f);
    c.setLineWidth(18);
    c.beginPath();
    c.circle(-70, 0, 50);
    c.stroke();
    c.beginPath();
    c.moveTo(-20, 0);
    c.lineTo(140, 0);
    c.stroke();
    c.beginPath();
    c.moveTo(110, 0);
    c.lineTo(110, 44);
    c.stroke();
    c.beginPath();
    c.moveTo(140, 0);
    c.lineTo(140, 34);
    c.stroke();
}

inline void glyphGun(Canvas& c, const Palette& p) {
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.95f});
    c.fillRect(-150, -30, 240, 34);
    c.beginPath();
    c.moveTo(-40, 4);
    c.lineTo(30, 4);
    c.lineTo(-10, 96);
    c.lineTo(-70, 96);
    c.closePath();
    c.fill();
    c.setFill(p.key, 0.5f);
    c.fillRect(-150, -30, 240, 6);
}

inline void glyphBook(Canvas& c, const Palette& p) {
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.95f});
    c.fillRect(-160, -110, 320, 220);
    c.setFill(p.key, 0.85f);
    c.fillRect(-140, -90, 280, 180);
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.9f});
    c.fillRect(-8, -90, 16, 180);
    c.setFill(p.accent, 0.6f);
    for (int i = 0; i < 5; ++i) {
        c.fillRect(-120, static_cast<float>(-60 + i * 26), 90, 6);
    }
}

inline void glyphBottle(Canvas& c, const Palette& p) {
    c.setFill(p.accent, 0.75f);
    c.beginPath();
    c.moveTo(-30, -140);
    c.lineTo(30, -140);
    c.lineTo(30, -60);
    c.quadTo(70, -20, 70, 40);
    c.lineTo(70, 120);
    c.lineTo(-70, 120);
    c.lineTo(-70, 40);
    c.quadTo(-70, -20, -30, -60);
    c.closePath();
    c.fill();
    c.setFill(p.key, 0.4f);
    c.fillRect(-50, 20, 14, 80);
}

inline void glyphBox(Canvas& c, const Palette& p) {
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.95f});
    c.fillRect(-150, -70, 300, 170);
    c.setFill(p.key, 0.7f);
    c.fillRect(-150, -100, 300, 34);
    c.setFill(p.accent, 0.85f);
    c.fillRect(-20, -100, 40, 200);
}

inline void glyphTape(Canvas& c, const Palette& p) {
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.95f});
    c.fillRect(-160, -100, 320, 200);
    c.setFill(p.key, 0.85f);
    c.fillRect(-130, -70, 260, 90);
    c.setFill(render::Color{0.0f, 0.0f, 0.0f, 0.9f});
    c.beginPath();
    c.circle(-60, -26, 34);
    c.fill();
    c.beginPath();
    c.circle(60, -26, 34);
    c.fill();
    c.setFill(p.accent, 0.8f);
    c.fillRect(-120, 40, 240, 12);
}

} // namespace detail

struct Glyph {
    const char* name;
    void (*draw)(Canvas&, const Palette&);
};

inline const std::vector<Glyph>& glyphs() {
    static const std::vector<Glyph> kGlyphs = {
        {"radio", detail::glyphRadio},   {"phone", detail::glyphPhone},
        {"letter", detail::glyphLetter}, {"photograph", detail::glyphPhotograph},
        {"key", detail::glyphKey},       {"gun", detail::glyphGun},
        {"book", detail::glyphBook},     {"bottle", detail::glyphBottle},
        {"box", detail::glyphBox},       {"tape", detail::glyphTape},
    };
    return kGlyphs;
}

// Which shape an object's name picks. The browser matches a regular expression per shape; every one
// of those patterns is a plain list of alternative words, so a substring search over the same lists
// in the same ORDER gives the same answer -- and the order matters, since "film reel" would match
// both `letter`'s "file" and `tape`'s "film" if the lists were checked the other way round.
inline const Glyph& glyphFor(const std::string& object) {
    std::string name;
    for (const char ch : object) {
        name.push_back(static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch));
    }
    struct Rule {
        const char* glyph;
        std::vector<const char*> words;
    };
    static const std::vector<Rule> kRules = {
        {"radio", {"radio", "transmitter", "walkie", "signal", "recording", "answering"}},
        {"phone", {"phone", "laptop", "computer", "drive", "monitor", "screen"}},
        {"letter", {"letter", "note", "telegram", "postcard", "essay", "manuscript", "papers",
                    "contract", "deed", "will", "receipt", "chart", "blueprint", "map", "file"}},
        {"photograph", {"photo", "photograph", "painting", "canvas", "sonogram", "picture"}},
        {"key", {"key", "badge", "star", "ring", "locket", "necklace", "coin", "watch"}},
        {"gun", {"gun", "rifle", "knife", "blade", "sword", "weapon", "crowbar", "wrench"}},
        {"book", {"book", "diary", "journal", "notebook", "bible"}},
        {"bottle", {"bottle", "jar", "canteen", "cup", "urn", "flask"}},
        {"tape", {"tape", "cassette", "video", "reel", "film"}},
    };
    for (const Rule& rule : kRules) {
        for (const char* word : rule.words) {
            if (name.find(word) != std::string::npos) {
                for (const Glyph& g : glyphs()) {
                    if (std::string(g.name) == rule.glyph) {
                        return g;
                    }
                }
            }
        }
    }
    // A word that matches nothing is still a thing in a box.
    return glyphs()[8];
}

} // namespace maz::film
