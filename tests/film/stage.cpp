// tests/film/stage.cpp — the room, and whether the camera actually frames what it says it frames.
//
// A framing is a promise: "close" says a face fills the frame, "wide" says you can see the whole
// person and the room around them, "two" says both people are in it. In the flat renderer those were
// scales and offsets and could be eyeballed. In three dimensions a framing is a position, a focal
// length and an aim, and whether it keeps its promise is a question with an answer: put the subject
// through the lens and see where they land on the film.
//
// That is what most of this does, and it is not academic — the first version of the camera clamped
// itself to the inside of the room, so every wide shot in the film was secretly a mid shot, and it
// took a contact sheet and a careful look to notice. Projected through the lens, it would have been
// one failing line.
//
// The rest is scale. A set is built against the bodies that stand in it, so a door has to be a door's
// height and a table a table's, or every shot in the room is quietly wrong.
#include "maz/film/Perform.hpp"
#include "maz/film/Sets.hpp"   // kAspect: the film's own shape
#include "maz/film/Stage.hpp"
#include "maz/render/MeshRayBvh.hpp"
#include "maz/render/SoftRaster.hpp"

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
static void near(float got, float want, float tol, const std::string& what) {
    if (!(std::fabs(got - want) <= tol)) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s (got %.4f, wanted %.4f +/- %.4f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

static const char* kSets[] = {"woods", "field",      "street", "water",  "lighthouse",
                              "room",  "corridor",   "office", "kitchen", "bar",
                              "ward",  "chapel",     "ship",   "vehicle", "industrial"};

// Where a world point lands on the film: (0,0) is the middle, (±1, ±1) the edges.
struct OnFilm {
    float x = 0.0f;
    float y = 0.0f;
    bool inFront = false;
    bool inFrame() const { return inFront && std::fabs(x) <= 1.0f && std::fabs(y) <= 1.0f; }
};

static OnFilm through(const film::Lens& lens, float aspect, const math::vec3& p) {
    const math::mat4 view = glm::lookAt(lens.eye, lens.at, math::vec3(0.0f, 1.0f, 0.0f));
    const math::mat4 vp = math::perspective(lens.fovY, aspect, 0.04f, 220.0f) * view;
    const math::vec4 clip = vp * math::vec4(p, 1.0f);
    OnFilm out;
    if (clip.w <= 1e-6f) {
        return out;
    }
    out.inFront = true;
    out.x = clip.x / clip.w;
    out.y = clip.y / clip.w; // down-positive, which is the engine's clip convention
    return out;
}

int main() {
    const float aspect = static_cast<float>(maz::film::kAspect);
    const film::Palette pal = film::paletteFor("drama", "DAY", 0.3);

    // ------------------------------------------------------------------ 1. every place is a place
    {
        for (const char* name : kSets) {
            film::Shot sh;
            sh.set = name;
            sh.scene = 3;
            const film::Stage st = film::buildStage(sh, pal, 11u);
            check(st.mesh.indices.size() >= 3 * 20,
                  std::string("the ") + name + " is built out of something");

            float lo = 1e9f;
            float hi = -1e9f;
            float wide = 0.0f;
            for (const maz::render::MeshVertex& v : st.mesh.vertices) {
                lo = v.py < lo ? v.py : lo;
                hi = v.py > hi ? v.py : hi;
                wide = std::fabs(v.px) > wide ? std::fabs(v.px) : wide;
            }
            check(lo > -0.30f, std::string("nothing in the ") + name + " is buried under the floor");
            if (st.indoors) {
                check(hi <= st.ceiling + 0.20f,
                      std::string("and nothing in the ") + name + " pokes through its ceiling");
                // A room has to clear the tallest person who will stand in it, with headroom — except
                // the inside of a vehicle, which nobody stands up in, and which is low ON PURPOSE:
                // that is the whole feeling of a scene played in one.
                const bool standing = std::string(name) != "vehicle";
                check(st.ceiling > (standing ? film::adultMale().height + 0.55f : 1.40f),
                      std::string("the ") + name + " has the headroom it ought to have");
            }
            check(wide <= st.halfWidth * 7.0f,
                  std::string("the ") + name + " stays roughly inside the width it claims");
        }
    }

    // ------------------------------------------------------------------ 2. built to human scale
    {
        // A corridor is a corridor because of its WIDTH. Two people cannot pass comfortably and that
        // is the whole feeling of the shot; if it is built five metres across it is a hall.
        film::Shot corridor;
        corridor.set = "corridor";
        const film::Stage c = film::buildStage(corridor, pal, 5u);
        check(c.halfWidth * 2.0f < 3.0f, "a corridor is under three metres across");
        check(c.depth > 8.0f, "and long enough to see down");

        film::Shot chapel;
        chapel.set = "chapel";
        const film::Stage ch = film::buildStage(chapel, pal, 5u);
        check(ch.ceiling > 5.0f, "a chapel is tall — the height IS the room");

        film::Shot vehicle;
        vehicle.set = "vehicle";
        const film::Stage v = film::buildStage(vehicle, pal, 5u);
        check(v.ceiling < 2.2f, "and the inside of a vehicle is not");

        // A doorway in the back wall of a room. Asked the honest way: fire a ray at the wall and see
        // whether it comes out the other side. The first version of this looked for a gap between
        // VERTICES, which found gaps everywhere — a wall is a box, and a box only has corners — so it
        // passed whether or not there was a door, and a mutation that removed every door in the film
        // went straight past it.
        film::Shot room;
        room.set = "office";
        const film::Stage r = film::buildStage(room, pal, 5u);
        const maz::render::MeshRayBvh solid(r.mesh);
        int wayThrough = 0;
        int wall = 0;
        for (int i = -30; i <= 30; ++i) {
            const float x = static_cast<float>(i) * (r.halfWidth - 0.2f) / 30.0f;
            const math::vec3 from(x, 1.10f, r.depth - 1.2f);
            const maz::render::MeshRayHit hit =
                solid.intersect(from, math::vec3(0.0f, 0.0f, 1.0f), 3.0f);
            if (hit.hit) {
                ++wall;
            } else {
                ++wayThrough;
            }
        }
        check(wall > 30, "most of the back wall of a room is wall");
        check(wayThrough >= 3, "and somewhere in it there is a way out");
    }

    // ------------------------------------------------------------------ 3. the marks are somewhere
    //                                                                        two people can stand
    {
        for (const char* name : kSets) {
            film::Shot sh;
            sh.set = name;
            const film::Stage st = film::buildStage(sh, pal, 9u);
            const float gap = st.markRight.x - st.markLeft.x;
            check(gap > 0.75f, std::string("in the ") + name + ", the two marks are far enough apart");
            check(gap < 2.2f, "and close enough to be a conversation rather than a shouting match");
            check(std::fabs(st.markLeft.x) < st.halfWidth - 0.4f,
                  std::string("and both are inside the ") + name);
            check(st.markLeft.z > 0.2f && st.markLeft.z < st.depth - 0.6f,
                  "and forward of the back wall, so the room has depth behind them");
        }
    }

    // ------------------------------------------------------------------ 4. a framing frames what it
    //                                                                        says it frames
    {
        film::Shot sh;
        sh.set = "office";
        const film::Stage st = film::buildStage(sh, pal, 3u);
        const film::Build b = film::adultMale();

        film::Subject a;
        film::Subject other;
        a.stand = st.markLeft;
        other.stand = st.markRight;
        {
            film::Motive mv;
            mv.position = st.markLeft;
            const film::Skeleton sk = film::performSkeleton(b, mv, 0.0f);
            a.eyeY = sk.eyes(b).y;
            a.chestY = film::Skeleton::at(sk.chest).y;
            other.eyeY = a.eyeY;
            other.chestY = a.chestY;
        }
        const math::vec3 head(a.stand.x, a.eyeY, a.stand.z);
        const math::vec3 feet(a.stand.x, 0.02f, a.stand.z);
        const math::vec3 crown(a.stand.x, b.height, a.stand.z);

        // WIDE: the whole person, feet and all, with room to spare.
        {
            film::Shot w = sh;
            w.framing = "wide";
            const film::Lens L = film::lensFor(w, st, a, other, true, 0.5f, 0.0f);
            check(through(L, aspect, feet).inFrame(), "a wide shot has the subject's feet in it");
            check(through(L, aspect, crown).inFrame(), "and the top of their head");
            const float span = std::fabs(through(L, aspect, crown).y - through(L, aspect, feet).y);
            check(span < 1.6f, "and does not fill the frame with them: a wide shot is a shot of a place");
        }

        // CLOSE: the head, and NOT the feet. A close-up that still has somebody's shoes in it is a mid.
        {
            film::Shot c = sh;
            c.framing = "close";
            const film::Lens L = film::lensFor(c, st, a, other, true, 0.5f, 0.0f);
            check(through(L, aspect, head).inFrame(), "a close-up has the subject's eyes in it");
            check(!through(L, aspect, feet).inFrame(), "and does not have their feet in it");
            const float headSpan =
                std::fabs(through(L, aspect, crown).y -
                          through(L, aspect, math::vec3(a.stand.x, a.eyeY - 0.22f, a.stand.z)).y);
            check(headSpan > 0.55f, "and the head is a large part of the frame, which is what close means");
        }

        // TWO: both of them, with neither jammed against the edge.
        {
            film::Shot t = sh;
            t.framing = "two";
            const film::Lens L = film::lensFor(t, st, a, other, true, 0.5f, 0.0f);
            const OnFilm p = through(L, aspect, math::vec3(a.stand.x, a.chestY, a.stand.z));
            const OnFilm q = through(L, aspect, math::vec3(other.stand.x, other.chestY, other.stand.z));
            check(p.inFrame() && q.inFrame(), "a two-shot has both of them in it");
            check(std::fabs(p.x) < 0.82f && std::fabs(q.x) < 0.82f,
                  "and neither of them pressed against the edge of it");
            check(std::fabs(p.x - q.x) > 0.12f, "with daylight between them");
        }

        // OVER THE SHOULDER: the far one's face in frame and off-centre, the near one's head nearer
        // the camera than they are.
        {
            film::Shot o = sh;
            o.framing = "ots";
            const film::Lens L = film::lensFor(o, st, a, other, true, 0.5f, 0.0f);
            const OnFilm face = through(L, aspect, head);
            check(face.inFrame(), "an over-the-shoulder has the speaker's face in frame");
            const float toFar = glm::length(L.eye - a.stand);
            const float toNear = glm::length(L.eye - other.stand);
            check(toNear < toFar, "and stands behind the OTHER one, which is whose shoulder it is over");
            check(toNear > 0.45f, "far enough back from them to see past rather than through them");
        }

        // LOW: the camera is on the floor, and it is looking up.
        {
            film::Shot l = sh;
            l.framing = "low";
            const film::Lens L = film::lensFor(l, st, a, other, true, 0.5f, 0.0f);
            check(L.eye.y < 0.8f, "a low angle is low");
            check(L.at.y > L.eye.y, "and looking up");
        }

        // INSERT: the object, and only the object.
        {
            film::Shot i = sh;
            i.framing = "insert";
            const film::Lens L = film::lensFor(i, st, a, other, true, 0.5f, 0.0f);
            check(through(L, aspect, st.objectAt).inFrame(), "an insert has the object in it");
            check(glm::length(L.eye - st.objectAt) < 1.2f, "and is right on top of it");
        }

        // The closer the shot, the LONGER the lens. Shooting a close-up wide bends the face outward
        // and is the commonest way an amateur close-up goes wrong.
        auto fovOf = [&](const char* framing) {
            film::Shot f = sh;
            f.framing = framing;
            return film::lensFor(f, st, a, other, true, 0.5f, 0.0f).fovY;
        };
        check(fovOf("close") < fovOf("mid"), "a close-up is on a longer lens than a mid");
        check(fovOf("mid") < fovOf("wide"), "and a mid is on a longer lens than a wide");
    }

    // ------------------------------------------------------------------ 5. a move moves, and a
    //                                                                        static shot does not
    {
        film::Shot sh;
        sh.set = "room";
        sh.framing = "mid";
        const film::Stage st = film::buildStage(sh, pal, 2u);
        film::Subject a;
        film::Subject b2;
        a.stand = st.markLeft;
        b2.stand = st.markRight;

        auto at = [&](const char* move, float t) {
            film::Shot s2 = sh;
            s2.camera = move;
            return film::lensFor(s2, st, a, b2, true, t, t * 3.0f);
        };

        near(glm::length(at("static", 0.0f).eye - at("static", 1.0f).eye), 0.0f, 1e-6f,
             "a static shot does not move, at all, ever");

        const float pushStart = glm::length(at("push", 0.0f).eye - a.stand);
        const float pushEnd = glm::length(at("push", 1.0f).eye - a.stand);
        check(pushEnd < pushStart * 0.95f, "a push ends closer than it began");
        const float pullEnd = glm::length(at("pull", 1.0f).eye - a.stand);
        check(pullEnd > pushStart * 1.05f, "and a pull ends further away");
        check(glm::length(at("push-slow", 1.0f).eye - a.stand) > pushEnd,
              "a slow push moves less than a push, which is the only difference between them");

        check(at("pan-l", 1.0f).at.x < at("pan-l", 0.0f).at.x, "a pan to the left looks left");
        check(at("pan-r", 1.0f).at.x > at("pan-r", 0.0f).at.x, "and a pan to the right looks right");
        near(at("pan-l", 0.0f).eye.x, at("pan-l", 1.0f).eye.x, 1e-6f,
             "a pan turns the camera and does not carry it anywhere");
        check(std::fabs(at("track-l", 1.0f).eye.x - at("track-l", 0.0f).eye.x) > 0.5f,
              "a track DOES carry it");

        check(glm::length(at("handheld", 0.3f).eye - at("handheld", 0.7f).eye) > 0.001f,
              "a handheld shot is never quite still");
    }

    // ------------------------------------------------------------------ 5b. the shadows on an actor
    //                                                                         agree with a raycast
    {
        // A shadow map is an approximation, so the question is not whether it is exact but whether it
        // is ever WRONG in the direction that shows: darkening a surface the light plainly reaches.
        // That is shadow acne, and on a person it crawls over every rounded thing in the frame.
        //
        // The ground truth comes from a different mechanism entirely, and deliberately uses NO part of
        // the mesh's own normals — an earlier version of this test picked its sample points by their
        // vertex normals and was therefore blind to the exact bug it was written for, which was that
        // the normals were inverted. Instead: fire rays at the body from where the light is. Whatever
        // each ray hits FIRST is, by definition, a point the light reaches, with nothing in between.
        // Every one of those points must come back lit.
        const film::Build b = film::adultMale();
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        const maz::render::shapes::MeshData body = film::buildBody(b, sk);
        const math::vec3 key = math::normalize(math::vec3(-0.6f, -0.75f, 0.35f));

        maz::render::ShadowMap map(1024);
        map.begin(maz::render::directionalLight(math::vec3(-4.5f, -0.15f, -1.55f),
                                                math::vec3(4.5f, 2.6f, 4.45f), key));
        map.add(body);
        const maz::render::MeshRayBvh truth(body);

        // Two axes across the light's own beam, to sweep it with.
        const math::vec3 across = glm::normalize(glm::cross(key, math::vec3(0.0f, 1.0f, 0.0f)));
        const math::vec3 down = glm::normalize(glm::cross(key, across));
        const math::vec3 centre(0.0f, b.height * 0.5f, 0.0f);

        int reached = 0;
        int wrong = 0;
        for (int i = -30; i <= 30; ++i) {
            for (int j = -46; j <= 46; ++j) {
                const math::vec3 from = centre - key * 4.0f + across * (static_cast<float>(i) * 0.012f) +
                                        down * (static_cast<float>(j) * 0.012f);
                const maz::render::MeshRayHit hit = truth.intersect(from, key, 8.0f);
                if (!hit.hit) {
                    continue;
                }
                ++reached;
                // Step back a hair toward the light so the lookup is on the surface, not inside it.
                if (maz::render::shadowFactor(map, hit.point - key * 0.0012f,
                                              math::vec3(0.0f, 1.0f, 0.0f)) > 0.5f) {
                    ++wrong;
                }
            }
        }
        check(reached > 1500, "the light reaches plenty of the body to check");
        // The threshold is set from measurement, and it is worth writing down what was measured, so
        // that the number is not a mystery to whoever meets it next. On this body, with this light:
        //
        //     back-face casting + slope-scaled bias   1.48%   <- what ships
        //     the same bias, not slope-scaled         2.01%
        //     the textbook normal offset instead     23.9%
        //     no bias at all                         12.1%
        //     the body built inside out              ~100%
        //
        // One in fifty-five sits between the first two, so a change that quietly drops the slope
        // scaling fails here rather than passing. Nothing in this is random: same body, same rays,
        // same answer every run.
        check(wrong * 55 < reached, "almost nothing the light plainly reaches is put in shadow");
    }

    // ------------------------------------------------------------------ 5c. the room is not a lid
    {
        // The room shell must not cast. A key light is a conceit — it is not a lamp hanging in the
        // sky above a sealed box — and the first frame rendered with shadows switched on was a
        // correctly pitch-dark room with a ceiling on it. So: from above the ceiling, straight down at
        // the mark, nothing that casts is in the way.
        for (const char* name : {"office", "corridor", "ward", "bar", "vehicle"}) {
            film::Shot sh;
            sh.set = name;
            const film::Stage st = film::buildStage(sh, pal, 13u);
            const maz::render::MeshRayBvh casters(st.props);
            const math::vec3 from(st.markLeft.x, st.ceiling + 1.5f, st.markLeft.z);
            const maz::render::MeshRayHit hit =
                casters.intersect(from, math::vec3(0.0f, -1.0f, 0.0f), 2.6f);
            check(!hit.hit, std::string("the ceiling of the ") + name + " does not block the key light");
            // And the shell IS still drawn — it is only casting that it is excused from.
            check(st.mesh.indices.size() > st.props.indices.size(),
                  std::string("while the ") + name + " still has a room around it");
        }
    }

    // ------------------------------------------------------------------ 6. the same shot twice
    {
        film::Shot sh;
        sh.set = "bar";
        sh.scene = 4;
        const film::Stage a = film::buildStage(sh, pal, 31u);
        const film::Stage b = film::buildStage(sh, pal, 31u);
        check(a.mesh.vertices.size() == b.mesh.vertices.size(),
              "the same room is dressed the same way twice");
        bool same = true;
        for (std::size_t i = 0; i < a.mesh.vertices.size(); ++i) {
            if (a.mesh.vertices[i].px != b.mesh.vertices[i].px) {
                same = false;
            }
        }
        check(same, "down to the position of every last thing in it");

        // And a different scene of the same film is dressed differently: a film in which every room is
        // the same room is a film shot in one room.
        film::Shot other = sh;
        other.scene = 7;
        const film::Stage c = film::buildStage(other, pal, 31u);
        bool differs = false;
        for (std::size_t i = 0; i < a.mesh.vertices.size() && i < c.mesh.vertices.size(); ++i) {
            if (a.mesh.vertices[i].px != c.mesh.vertices[i].px) {
                differs = true;
            }
        }
        check(differs, "and a different scene of the same film is not the identical room");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("stage: all checks passed\n");
    return 0;
}
