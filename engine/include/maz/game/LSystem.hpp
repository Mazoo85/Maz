#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

// maz::game L-system (Lindenmayer system) — grammar-based procedural generation. Starting from an axiom
// string and a set of rewrite rules (symbol -> replacement), it repeatedly expands the string; a turtle
// interpreter then walks the result to draw line segments. It is the classic compact way to generate
// plants, trees, roots, and space-filling fractals (Koch, dragon, Sierpinski) from a few characters, and
// to grow branching dungeon corridors or river networks. Godot ships no L-system, so this is a
// beyond-Godot procgen utility. The rewrite is deterministic (context-free, single-pass per iteration);
// the turtle uses the standard command alphabet. Header-only, std-only.
namespace maz::game {

struct LSystem {
    std::string axiom;
    std::unordered_map<char, std::string> rules; // symbol -> its replacement (missing symbols map to self)

    // Expand the axiom `iterations` times. A symbol without a rule is copied unchanged (a "constant").
    std::string generate(int iterations) const {
        std::string current = axiom;
        for (int it = 0; it < iterations; ++it) {
            std::string next;
            next.reserve(current.size() * 2);
            for (char c : current) {
                const auto r = rules.find(c);
                if (r != rules.end()) {
                    next += r->second;
                } else {
                    next += c;
                }
            }
            current.swap(next);
        }
        return current;
    }
};

// A drawn line segment in world space.
struct TurtleSegment {
    math::vec2 a{0.0f, 0.0f};
    math::vec2 b{0.0f, 0.0f};
};

struct TurtleConfig {
    math::vec2 start{0.0f, 0.0f};
    float headingRad = 0.0f; // initial heading, CCW from +X
    float step = 1.0f;       // distance moved by F/G/f
    float angleRad = 1.5707963f; // turn amount for + / -
};

// Interpret an L-system string as turtle graphics, returning the drawn segments. Command alphabet:
//   'F','G' move forward drawing a segment; 'f' move forward without drawing;
//   '+' turn left (CCW) by angle; '-' turn right (CW) by angle;
//   '[' push (position + heading); ']' pop the last pushed state; any other symbol is ignored.
inline std::vector<TurtleSegment> interpretTurtle(const std::string& s, const TurtleConfig& cfg) {
    struct State {
        math::vec2 pos;
        float heading;
    };
    std::vector<TurtleSegment> out;
    State st{cfg.start, cfg.headingRad};
    std::vector<State> stack;
    for (char c : s) {
        switch (c) {
        case 'F':
        case 'G': {
            const math::vec2 np(st.pos.x + cfg.step * std::cos(st.heading),
                                st.pos.y + cfg.step * std::sin(st.heading));
            out.push_back(TurtleSegment{st.pos, np});
            st.pos = np;
            break;
        }
        case 'f':
            st.pos = math::vec2(st.pos.x + cfg.step * std::cos(st.heading),
                                st.pos.y + cfg.step * std::sin(st.heading));
            break;
        case '+':
            st.heading += cfg.angleRad;
            break;
        case '-':
            st.heading -= cfg.angleRad;
            break;
        case '[':
            stack.push_back(st);
            break;
        case ']':
            if (!stack.empty()) {
                st = stack.back();
                stack.pop_back();
            }
            break;
        default:
            break; // variables / unknown symbols advance nothing
        }
    }
    return out;
}

} // namespace maz::game
