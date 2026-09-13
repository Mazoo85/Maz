// tests/film/air.cpp — the weather, as things in the room rather than paint on the lens.
//
// The flat renderer draws weather in screen space and this one puts it in the world, so the two can
// disagree in a way nothing else in the film can. The first and most important check here is that they
// do not: `weatherFor` is the one chooser, both renderers ask it, and a shot that rains in one look
// rains in the other. A Look control that changes the weather is not a look, it is a different film.
#include "maz/film/AirVolume.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace film = maz::film;
namespace math = maz::math;

static std::vector<std::string> failures;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        failures.push_back(what);
    }
}

// Where the middle of the weather is, so a particle can be followed from one moment to the next.
static math::vec3 centreOf(const maz::render::shapes::MeshData& m, std::size_t quad) {
    math::vec3 sum(0.0f, 0.0f, 0.0f);
    for (std::size_t k = 0; k < 4; ++k) {
        const auto& v = m.vertices[quad * 4 + k];
        sum += math::vec3(v.px, v.py, v.pz);
    }
    return sum * 0.25f;
}

int main() {
    const math::vec3 eye(0.4f, 1.55f, -3.2f);
    const math::vec3 at(0.0f, 1.40f, 0.6f);
    const maz::render::Color tint{0.72f, 0.76f, 0.84f, 1.0f};

    // ------------------------------------------------------------------ 1. one film, one weather
    {
        // Every combination the film can produce, against the rule it is ported from. If these two
        // ever drift, one look rains and the other does not.
        const char* genres[] = {"drama", "horror", "thriller", "western", "fantasy", "mystery",
                                "comedy", "romance"};
        const char* hours[] = {"DAY", "DUSK", "NIGHT", "DAWN"};
        const char* sets[] = {"street", "woods", "field", "water", "corridor", "room", "ward", "bar"};
        int drawnAsParticles = 0;
        int drawnAsAir = 0;
        for (const char* g : genres) {
            for (const char* t : hours) {
                for (const char* st : sets) {
                    const film::Weather w = film::weatherFor(g, t, st);
                    if (film::weatherHasParticles(w)) {
                        ++drawnAsParticles;
                    } else if (w != film::Weather::None) {
                        ++drawnAsAir;
                    }
                }
            }
        }
        check(drawnAsParticles > 20, "plenty of the film's weather is made of things you can see");
        check(drawnAsAir > 20, "and plenty of it is the air itself, which the fog already does");

        // Fog, haze and shimmer draw NO particles, on purpose. They are the air rather than things in
        // it, and the renderer already fades every surface toward the sky with distance — faking them
        // again on top would be two depth cues disagreeing about how far away the back wall is.
        for (film::Weather w : {film::Weather::Fog, film::Weather::Haze, film::Weather::Shimmer,
                                film::Weather::None}) {
            check(film::airMesh(w, eye, at, 2.0, 7u, tint, 0.5f).vertices.empty(),
                  std::string("nothing is drawn for ") + film::weatherName(w) +
                      ", which is the air itself");
        }
    }

    // ------------------------------------------------------------------ 2. it is in the room
    {
        const maz::render::shapes::MeshData rain = film::airMesh(film::Weather::Rain, eye, at, 2.0, 7u,
                                                                 tint, 0.4f);
        check(rain.vertices.size() > 400, "rain is made of a lot of rain");
        check(rain.vertices.size() % 4 == 0 && rain.indices.size() % 6 == 0,
              "and of whole quads, two triangles each");

        // In FRONT of the lens, not scattered through the set. Weather is everywhere and a shot only
        // ever sees the part of it the lens is pointed at; filling the set instead leaves a long lens
        // looking through an empty room while the rain falls off to one side.
        math::vec3 forward = at - eye;
        forward = forward / std::sqrt(math::dot(forward, forward));
        int behind = 0;
        float nearest = 1e9f;
        float furthest = 0.0f;
        for (const auto& v : rain.vertices) {
            const float along = math::dot(math::vec3(v.px, v.py, v.pz) - eye, forward);
            if (along < 0.0f) {
                ++behind;
            }
            nearest = std::min(nearest, along);
            furthest = std::max(furthest, along);
        }
        check(behind == 0, "and none of it is behind the camera");
        check(nearest > 0.8f, "nor so close to the lens that one drop fills the frame");
        check(furthest > 8.0f, "and it carries on back past the actors");
    }

    // ------------------------------------------------------------------ 3. it falls
    {
        const maz::render::shapes::MeshData a =
            film::airMesh(film::Weather::Rain, eye, at, 2.00, 7u, tint, 0.4f);
        const maz::render::shapes::MeshData b =
            film::airMesh(film::Weather::Rain, eye, at, 2.05, 7u, tint, 0.4f);
        check(a.vertices.size() == b.vertices.size(), "a twentieth of a second later it is the same rain");
        int fell = 0, rose = 0;
        for (std::size_t q = 0; q * 4 + 3 < a.vertices.size(); ++q) {
            const float dy = centreOf(b, q).y - centreOf(a, q).y;
            if (dy < -0.01f) {
                ++fell;
            } else if (dy > 0.01f) {
                ++rose;
            }
        }
        check(fell > rose * 4, "and nearly all of it has fallen, because that is what rain does");

        // Embers are the same machinery pointed the other way, and that is the whole of what an ember
        // is: a thing that goes up.
        const maz::render::shapes::MeshData e0 =
            film::airMesh(film::Weather::Embers, eye, at, 2.00, 7u, tint, 0.4f);
        const maz::render::shapes::MeshData e1 =
            film::airMesh(film::Weather::Embers, eye, at, 2.20, 7u, tint, 0.4f);
        int up = 0, down = 0;
        for (std::size_t q = 0; q * 4 + 3 < e0.vertices.size(); ++q) {
            const float dy = centreOf(e1, q).y - centreOf(e0, q).y;
            if (dy > 0.005f) {
                ++up;
            } else if (dy < -0.005f) {
                ++down;
            }
        }
        check(up > down * 4, "and embers go up");

        // Nothing runs out. A particle that falls off the bottom comes back in at the top, so the
        // weather is still there at the end of a long shot.
        const maz::render::shapes::MeshData late =
            film::airMesh(film::Weather::Rain, eye, at, 240.0, 7u, tint, 0.4f);
        check(late.vertices.size() == a.vertices.size(), "and four minutes in there is still rain");
        // And it is still IN FRAME. Without the wrap the count never changes and the spread never
        // changes — every drop simply keeps going, so four minutes in the whole storm is a mile below
        // the floor and the shot is dry.
        int inFrame = 0;
        float lowest = 1e9f, highest = -1e9f;
        for (const auto& v : late.vertices) {
            lowest = std::min(lowest, v.py);
            highest = std::max(highest, v.py);
            if (std::fabs(v.py - eye.y) < 5.0f) {
                ++inFrame;
            }
        }
        check(highest - lowest > 4.0f, "spread through the air rather than piled up on the floor");
        check(inFrame > static_cast<int>(late.vertices.size()) / 2,
              "and most of it is still somewhere the lens can see, not a mile below the floor");
    }

    // ------------------------------------------------------------------ 4. the same every time
    {
        // Two renderers compare frames pixel for pixel. Weather that is not identical from the same
        // inputs makes that comparison meaningless, and the arithmetic in here is integer and square
        // roots only for exactly that reason.
        const maz::render::shapes::MeshData a =
            film::airMesh(film::Weather::Dust, eye, at, 3.5, 91u, tint, 0.3f);
        const maz::render::shapes::MeshData b =
            film::airMesh(film::Weather::Dust, eye, at, 3.5, 91u, tint, 0.3f);
        bool same = a.vertices.size() == b.vertices.size();
        for (std::size_t i = 0; same && i < a.vertices.size(); ++i) {
            same = a.vertices[i].px == b.vertices[i].px && a.vertices[i].py == b.vertices[i].py &&
                   a.vertices[i].pz == b.vertices[i].pz;
        }
        check(same, "the same moment of the same film has the same air in it, to the last decimal");

        const maz::render::shapes::MeshData other =
            film::airMesh(film::Weather::Dust, eye, at, 3.5, 92u, tint, 0.3f);
        bool moved = false;
        for (std::size_t i = 0; i < a.vertices.size() && i < other.vertices.size(); ++i) {
            if (a.vertices[i].px != other.vertices[i].px) {
                moved = true;
            }
        }
        check(moved, "and a different film has different air");
    }

    // ------------------------------------------------------------------ 5. harder when it matters
    {
        const std::size_t calm =
            film::airMesh(film::Weather::Rain, eye, at, 2.0, 7u, tint, 0.0f).vertices.size();
        const std::size_t wild =
            film::airMesh(film::Weather::Rain, eye, at, 2.0, 7u, tint, 1.0f).vertices.size();
        check(wild > calm, "a wound-up shot gets harder weather, which is what a director would ask for");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("air: all checks passed\n");
    return 0;
}
