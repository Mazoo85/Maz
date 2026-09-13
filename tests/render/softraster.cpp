// tests/render/softraster.cpp — the software rasteriser, against the five ways one goes wrong.
//
// A triangle rasteriser is easy to write and hard to write correctly, and it fails in five specific
// ways, every one of which is invisible in a still of a cube and ruinous in a film:
//
//   1. No depth test          — the last thing drawn wins, so a character is behind a wall or in front
//                               of it depending on the order the scene happened to be assembled in.
//   2. Wrong winding          — the inside of every object is drawn over the outside.
//   3. No near-plane clip     — a triangle with one corner behind the eye divides by a w near zero and
//                               smears across the whole frame. This is the one that looks like a crash.
//   4. A gap-prone fill rule  — two triangles sharing an edge leave a seam of unfilled pixels, so every
//                               flat surface in the film is stitched with background-coloured thread.
//   5. Flat light             — a surface facing away from the key goes to pure black rather than to the
//                               fill, which on a face reads as a hole.
//
// So each of those is a test, and each was run against a deliberately broken rasteriser first to prove
// it catches the break.
#include "maz/render/SoftRaster.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes3D.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using maz::render::Color;
using maz::render::Image;
using maz::render::SoftRaster;
using maz::render::Surface;
namespace shapes = maz::render::shapes;
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
        std::snprintf(buf, sizeof buf, "%s (got %.5f, wanted %.5f +/- %.5f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

// A flat quad on the XY plane at depth z, facing +Z (toward a camera on +Z), in one colour.
static shapes::MeshData quadAt(float z, float half, const Color& c) {
    shapes::MeshData m;
    const float n[3] = {0.0f, 0.0f, 1.0f};
    auto v = [&](float x, float y) {
        return maz::render::MeshVertex{x, y, z, n[0], n[1], n[2], c.r, c.g, c.b, 0.0f, 0.0f};
    };
    m.vertices = {v(-half, -half), v(half, -half), v(half, half), v(-half, half)};
    // Counter-clockwise seen from +Z looking back down -Z.
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

// A camera on +Z looking at the origin.
static math::mat4 frontCamera(float distance, float aspect) {
    const math::mat4 view =
        glm::lookAt(math::vec3(0.0f, 0.0f, distance), math::vec3(0.0f), math::vec3(0.0f, 1.0f, 0.0f));
    return math::perspective(0.8f, aspect, 0.1f, 100.0f) * view;
}

static Surface plainSurface() {
    Surface s;
    s.ambient = 0.3f;
    s.fill = 0.0f;
    s.keyDirection = math::vec3(0.0f, 0.0f, -1.0f); // travelling away from the camera, onto the quad
    return s;
}

int main() {
    const int W = 120;
    const int H = 90;
    const float aspect = static_cast<float>(W) / static_cast<float>(H);
    const math::mat4 vp = frontCamera(4.0f, aspect);

    // ------------------------------------------------------------------ 1. something is drawn at all
    {
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        r.draw(img, quadAt(0.0f, 1.0f, Color{1.0f, 0.0f, 0.0f, 1.0f}), math::mat4(1.0f), vp,
               plainSurface());
        check(r.trianglesDrawn() == 2, "both triangles of the quad reach the screen");
        const Color middle = img.getPixel(W / 2, H / 2);
        check(middle.r > 0.2f, "the centre of the frame is the quad, not the background");
        const Color corner = img.getPixel(1, 1);
        check(corner.r == 0.0f && corner.g == 0.0f, "the corner is still background");
    }

    // ------------------------------------------------------------------ 2. depth: near wins, whatever
    //                                                                        order it was drawn in
    {
        // Draw the FAR quad second. Without a depth buffer it would win, because it is last.
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        r.draw(img, quadAt(0.0f, 0.8f, Color{0.0f, 0.0f, 1.0f, 1.0f}), math::mat4(1.0f), vp,
               plainSurface());
        r.draw(img, quadAt(-1.5f, 2.2f, Color{1.0f, 0.0f, 0.0f, 1.0f}), math::mat4(1.0f), vp,
               plainSurface());
        const Color middle = img.getPixel(W / 2, H / 2);
        check(middle.b > middle.r, "the near quad is still in front after the far one is drawn over it");
        // And the far one IS visible where the near one does not cover it.
        const Color edge = img.getPixel(W / 2, 6);
        check(edge.r > edge.b, "the far quad shows where the near one does not reach");
    }

    // ------------------------------------------------------------------ 3. winding: a back face is
    //                                                                        not drawn
    {
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        shapes::MeshData flipped = quadAt(0.0f, 1.0f, Color{1.0f, 0.0f, 0.0f, 1.0f});
        flipped.indices = {0, 2, 1, 0, 3, 2}; // same quad, wound the other way
        r.draw(img, flipped, math::mat4(1.0f), vp, plainSurface());
        check(r.trianglesDrawn() == 0, "a back-facing triangle is culled");
        check(img.getPixel(W / 2, H / 2).r == 0.0f, "and nothing of it reaches the frame");
    }

    // ------------------------------------------------------------------ 4. the near plane: a triangle
    //                                                                        that straddles the eye
    {
        // One corner well behind the camera, two in front. There are two wrong answers here and the
        // test has to separate them from the right one: dividing by a w through zero smears the
        // triangle across the whole frame, and simply DROPPING any triangle with a corner behind the
        // eye loses geometry the camera can plainly see. The right answer is to cut it at the near
        // plane and draw the part that is in front.
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        shapes::MeshData m;
        auto v = [](float x, float y, float z) {
            return maz::render::MeshVertex{x, y, z, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};
        };
        m.vertices = {v(-0.9f, -0.6f, 0.0f), v(-0.4f, -0.6f, 0.0f), v(-0.9f, 0.6f, 9.0f)};
        m.indices = {0, 2, 1}; // wound to face the camera: this triangle is nearly edge-on, and the
                               // other winding is genuinely a back face, which is correctly culled
        r.draw(img, m, math::mat4(1.0f), vp, plainSurface());
        int covered = 0;
        int onTheRight = 0;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (img.getPixel(x, y).r > 0.0f) {
                    ++covered;
                    if (x > (W * 3) / 4) {
                        ++onTheRight;
                    }
                }
            }
        }
        check(covered > 40, "the part of the triangle in front of the eye is still drawn");
        check(onTheRight == 0, "and it does not smear across the frame");
    }

    // ------------------------------------------------------------------ 5. no seam between two
    //                                                                        triangles sharing an edge
    {
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        r.draw(img, quadAt(0.0f, 1.4f, Color{1.0f, 1.0f, 1.0f, 1.0f}), math::mat4(1.0f), vp,
               plainSurface());
        // A filled quad is convex, so on every row the covered pixels must be one unbroken run. Any
        // background pixel sitting BETWEEN two covered ones is a hole, and the diagonal the two
        // triangles share is where holes appear. Checking every row rather than guessing where the
        // diagonal lands means the test cannot miss it by being aimed slightly wrong.
        int holes = 0;
        for (int y = 0; y < H; ++y) {
            int first = -1;
            int last = -1;
            for (int x = 0; x < W; ++x) {
                if (img.getPixel(x, y).r > 0.0f) {
                    if (first < 0) {
                        first = x;
                    }
                    last = x;
                }
            }
            for (int x = first; x >= 0 && x < last; ++x) {
                if (img.getPixel(x, y).r == 0.0f) {
                    ++holes;
                }
            }
        }
        check(holes == 0, "the shared edge of two triangles has no gap in it");

        // And the case that actually bites: a shared edge lying EXACTLY on a row of pixel centres.
        // Floors, walls and boxes are axis-aligned, so this is not a corner case in a film, it is most
        // of the picture. Under an orthographic camera scaled one world unit to one pixel, a horizontal
        // seam can be put exactly on a pixel-centre row, and a rasteriser that tests its edges
        // exclusively drops that whole row on the floor. The row is SEARCHED for rather than assumed,
        // so the test says out loud that it found the case it means to test.
        const math::mat4 orthoVp =
            math::orthographicSize(static_cast<float>(H), aspect, 0.1f, 100.0f) *
            glm::lookAt(math::vec3(0.0f, 0.0f, 4.0f), math::vec3(0.0f), math::vec3(0.0f, 1.0f, 0.0f));
        float seamY = 0.0f;
        bool foundSeam = false;
        // The seam has to land INSIDE the rectangle it splits, so the search is over the middle of it.
        for (int i = -24; i <= 24 && !foundSeam; ++i) {
            const float y = static_cast<float>(i) * 0.5f;
            const math::vec4 c = orthoVp * math::vec4(0.0f, y, 0.0f, 1.0f);
            const float py = (c.y / c.w * 0.5f + 0.5f) * static_cast<float>(H);
            if (py > 30.0f && py < 60.0f &&
                static_cast<double>(py) - std::floor(static_cast<double>(py)) == 0.5) {
                seamY = y;
                foundSeam = true;
            }
        }
        check(foundSeam, "a seam can be placed exactly on a row of pixel centres");
        if (foundSeam) {
            shapes::MeshData split;
            auto sv = [](float x, float y) {
                return maz::render::MeshVertex{x, y, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                               0.0f};
            };
            split.vertices = {sv(-20.0f, -20.0f), sv(20.0f, -20.0f), sv(20.0f, seamY),
                              sv(-20.0f, seamY),  sv(20.0f, 20.0f),  sv(-20.0f, 20.0f)};
            split.indices = {0, 1, 2, 0, 2, 3,   // below the seam
                             3, 2, 4, 3, 4, 5};  // above it
            Image seamImg(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
            SoftRaster sr(W, H);
            sr.clearDepth();
            sr.draw(seamImg, split, math::mat4(1.0f), orthoVp, plainSurface());
            // The rectangle spans 40 world units either side of centre, which at one unit to the
            // pixel is screen rows 25 to 65. EVERY row in that band must carry paint. Looking only for
            // a gap BETWEEN covered pixels would miss this: when a seam falls out, the whole row goes,
            // and a row with nothing in it has no inside for a gap to be in.
            int emptyRows = 0;
            for (int y = 27; y <= 63; ++y) {
                bool any = false;
                for (int x = 0; x < W; ++x) {
                    if (seamImg.getPixel(x, y).r > 0.0f) {
                        any = true;
                    }
                }
                if (!any) {
                    ++emptyRows;
                }
            }
            check(emptyRows == 0, "a seam sitting on a row of pixel centres is still covered");
        }
    }

    // ------------------------------------------------------------------ 6. light: lit is brighter than
    //                                                                        unlit, and unlit is not black
    {
        Surface s;
        s.ambient = 0.25f;
        s.fill = 0.12f;
        s.keyDirection = math::normalize(math::vec3(0.0f, 0.0f, -1.0f));
        // A sphere: its middle faces the key, its rim faces across it.
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        r.draw(img, shapes::makeSphere(1.0f, 24, 32, Color{1.0f, 1.0f, 1.0f, 1.0f}), math::mat4(1.0f),
               vp, s);
        const Color lit = img.getPixel(W / 2, H / 2);
        check(lit.r > 0.8f, "the point of the sphere facing the key is near full brightness");

        // And the same sphere with the key coming from behind it: every visible pixel is in shade, but
        // none of it is pure black, because there is an ambient and a fill.
        Surface back = s;
        back.keyDirection = math::normalize(math::vec3(0.0f, 0.0f, 1.0f));
        Image img2(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r2(W, H);
        r2.clearDepth();
        r2.draw(img2, shapes::makeSphere(1.0f, 24, 32, Color{1.0f, 1.0f, 1.0f, 1.0f}), math::mat4(1.0f),
                vp, back);
        const Color shade = img2.getPixel(W / 2, H / 2);
        check(shade.r > 0.05f, "a surface turned away from the key still reads, it does not go to black");
        check(shade.r < lit.r, "and it is darker than the lit side");
    }

    // ------------------------------------------------------------------ 7. the depth buffer is
    //                                                                        readable, and empty means far
    {
        SoftRaster r(W, H);
        r.clearDepth();
        check(r.depthAt(0, 0) >= 1.0f, "a cleared depth buffer is at the far plane");
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        r.draw(img, quadAt(0.0f, 1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}), math::mat4(1.0f), vp,
               plainSurface());
        check(r.depthAt(W / 2, H / 2) < 1.0f, "and the covered middle now holds the quad's depth");
        check(r.depthAt(1, 1) >= 1.0f, "while the uncovered corner does not");
    }

    // ------------------------------------------------------------------ 8. a model matrix moves it
    {
        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        const math::mat4 model = glm::translate(math::mat4(1.0f), math::vec3(1.2f, 0.0f, 0.0f));
        r.draw(img, quadAt(0.0f, 0.5f, Color{1.0f, 0.0f, 0.0f, 1.0f}), model, vp, plainSurface());
        check(img.getPixel(W / 2, H / 2).r == 0.0f, "moved right, the quad has left the centre");
        bool foundRight = false;
        for (int x = W / 2; x < W; ++x) {
            if (img.getPixel(x, H / 2).r > 0.2f) {
                foundRight = true;
            }
        }
        check(foundRight, "and it is on the right of frame");
    }

    // ------------------------------------------------------------------ 9. perspective-correct
    //                                                                        interpolation
    {
        // A floor running away from the camera, shaded black at the near edge and white at the far one.
        // Interpolated correctly, the value at a pixel is the value at the WORLD POINT that projects
        // there; interpolated the cheap way (straight across the screen) the ramp is stretched toward
        // the horizon and everything on the ground is at the wrong distance. The floor is drawn with
        // its colour passed straight through the shade, so the number under test is the number read.
        const float nearZ = -2.0f;
        const float farZ = -20.0f;
        const float floorY = -1.2f;
        auto shadeless = []() {
            Surface s;
            s.ambient = 0.0f;
            s.fill = 0.0f;
            s.emissive = 1.0f;                                  // colour straight through, no lighting
            s.keyDirection = math::vec3(0.0f, 1.0f, 0.0f);
            return s;
        };
        auto floorVertex = [&](float x, float z) {
            const float t = (-z + nearZ) / (nearZ - farZ);       // 0 at the near edge, 1 at the far
            return maz::render::MeshVertex{x, floorY, z, 0.0f, 1.0f, 0.0f, t, t, t, 0.0f, 0.0f};
        };
        shapes::MeshData floorMesh;
        floorMesh.vertices = {floorVertex(-4.0f, nearZ), floorVertex(4.0f, nearZ),
                              floorVertex(4.0f, farZ), floorVertex(-4.0f, farZ)};
        floorMesh.indices = {0, 1, 2, 0, 2, 3};

        const math::mat4 view = glm::lookAt(math::vec3(0.0f), math::vec3(0.0f, 0.0f, -1.0f),
                                            math::vec3(0.0f, 1.0f, 0.0f));
        const math::mat4 floorVp = math::perspective(0.8f, aspect, 0.1f, 100.0f) * view;

        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        r.draw(img, floorMesh, math::mat4(1.0f), floorVp, shadeless());

        // Where does the point six units out land, and what is written there?
        const float probeZ = -6.0f;
        const math::vec4 clip = floorVp * math::vec4(0.0f, floorY, probeZ, 1.0f);
        const int probeX = static_cast<int>((clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(W));
        const int probeY = static_cast<int>((clip.y / clip.w * 0.5f + 0.5f) * static_cast<float>(H));
        const float expected = (-probeZ + nearZ) / (nearZ - farZ);  // 0.2222...
        const float got = img.getPixel(probeX, probeY).r;
        check(std::fabs(got - expected) < 0.02f,
              "a point on the floor is shaded for where it actually is, not for where it is on screen");
    }

    // ------------------------------------------------------------------ 10. aerial perspective
    {
        // Two identical quads, one near and one far, under fog. The far one must sit closer to the
        // colour of the air; the near one must be untouched by it. Without this a film has no depth
        // outdoors and the ground plane has to stop somewhere the audience can see.
        Surface s = plainSurface();
        s.fog = Color{0.0f, 0.0f, 1.0f, 1.0f}; // a colour nothing else in this test is
        s.fogStart = 3.0f;
        s.fogEnd = 9.0f;
        s.fogMax = 1.0f;

        Image img(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        // The camera is 4 units out, so the quad at z=+1 is 3 units away (at the fog's start) and the
        // one at z=-5 is 9 (at its end).
        // Red, with no blue in it at all: any blue in the result can only have come from the air.
        r.draw(img, quadAt(1.0f, 0.35f, Color{1.0f, 0.0f, 0.0f, 1.0f}), math::mat4(1.0f), vp, s);
        const Color near_ = img.getPixel(W / 2, H / 2);
        Image far(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r2(W, H);
        r2.clearDepth();
        r2.draw(far, quadAt(-5.0f, 2.4f, Color{1.0f, 0.0f, 0.0f, 1.0f}), math::mat4(1.0f), vp, s);
        const Color farC = far.getPixel(W / 2, H / 2);

        check(near_.b < 0.05f, "a surface at the near edge of the fog is not touched by it");
        check(farC.b > 0.9f, "one at the far edge has gone the whole way to the colour of the air");
        check(farC.r < near_.r, "and has lost its own colour on the way");

        // Switched off, the same far quad keeps its colour exactly.
        Surface clear = plainSurface();
        Image plain(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r3(W, H);
        r3.clearDepth();
        r3.draw(plain, quadAt(-5.0f, 2.4f, Color{1.0f, 0.0f, 0.0f, 1.0f}), math::mat4(1.0f), vp, clear);
        check(plain.getPixel(W / 2, H / 2).b < 0.05f, "with no fog set, nothing fades");
    }

    // ------------------------------------------------------------------ 11. shadows
    //
    // Shadows fail in two ways that have names, and both of them are worse than having no shadows at
    // all. ACNE is a lit surface shadowing itself, which crawls and stipples over every flat thing in
    // the frame. PETER-PANNING is a shadow that has come away from the foot of whatever is casting it,
    // so an object floats above its own shadow — which is precisely the thing shadows were added to
    // cure. Each has a test, and each was checked against a version that has the fault.
    {
        // A floor, and a box hanging over it. The light comes straight down.
        shapes::MeshData floorMesh;
        {
            auto v = [](float x, float z) {
                return maz::render::MeshVertex{x, 0.0f, z, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                               0.0f};
            };
            floorMesh.vertices = {v(-6.0f, 6.0f), v(6.0f, 6.0f), v(6.0f, -6.0f), v(-6.0f, -6.0f)};
            floorMesh.indices = {0, 1, 2, 0, 2, 3};
        }
        const shapes::MeshData blocker = maz::render::applyTransform(
            maz::render::applyTransform(shapes::makeBox(1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}),
                                        maz::render::scaleMatrix(math::vec3(1.2f, 0.6f, 1.2f))),
            maz::render::translationMatrix(math::vec3(0.0f, 1.4f, 0.0f)));

        const math::vec3 down = math::normalize(math::vec3(0.0f, -1.0f, 0.0f));
        maz::render::ShadowMap map(512);
        map.begin(maz::render::directionalLight(math::vec3(-6.0f, 0.0f, -6.0f),
                                                math::vec3(6.0f, 2.5f, 6.0f), down));
        map.add(floorMesh);
        map.add(blocker);

        // Straight down on the floor, so the shadow lands where the box is.
        const math::mat4 topView = glm::lookAt(math::vec3(0.0f, 7.0f, 0.01f), math::vec3(0.0f),
                                               math::vec3(0.0f, 1.0f, 0.0f));
        const math::mat4 topVp = math::perspective(0.9f, aspect, 0.1f, 40.0f) * topView;

        Surface lit;
        lit.ambient = 0.15f;
        lit.fill = 0.0f;
        lit.keyDirection = down;
        lit.shadows = &map;
        lit.shadowStrength = 1.0f;

        Image withShadow(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r(W, H);
        r.clearDepth();
        r.draw(withShadow, floorMesh, math::mat4(1.0f), topVp, lit);

        Surface noShadow = lit;
        noShadow.shadows = nullptr;
        Image plain(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r2(W, H);
        r2.clearDepth();
        r2.draw(plain, floorMesh, math::mat4(1.0f), topVp, noShadow);

        // Under the box: dark. Out at the edge of the floor: exactly as bright as with no shadow at all.
        const Color under = withShadow.getPixel(W / 2, H / 2);
        const Color openFloor = withShadow.getPixel(4, H / 2);
        check(under.r < 0.30f, "the floor under something is in shadow");
        check(openFloor.r > 0.85f, "and the floor out in the open is not");
        near(openFloor.r, plain.getPixel(4, H / 2).r, 1.0f / 255.0f,
             "open floor is lit exactly as it would be with no shadows at all");

        // ACNE: the open floor must be UNIFORM. Acne shows up as neighbouring pixels of a flat, evenly
        // lit surface disagreeing — so that is what is measured, rather than looked for by eye.
        int speckles = 0;
        for (int y = 6; y < H - 6; ++y) {
            for (int x = 2; x < 16; ++x) {
                if (std::fabs(withShadow.getPixel(x, y).r - withShadow.getPixel(x + 1, y).r) >
                    3.0f / 255.0f) {
                    ++speckles;
                }
            }
        }
        check(speckles == 0, "a flat lit floor has no shadow acne crawling over it");

        // PETER-PANNING: a box sitting ON the floor must be dark right at its own foot. A shadow that
        // starts a few centimetres away leaves the object floating above it.
        const shapes::MeshData standing = maz::render::applyTransform(
            maz::render::applyTransform(shapes::makeBox(1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}),
                                        maz::render::scaleMatrix(math::vec3(1.0f, 1.6f, 1.0f))),
            maz::render::translationMatrix(math::vec3(0.0f, 0.8f, 0.0f)));
        const math::vec3 slant = math::normalize(math::vec3(-0.55f, -0.80f, 0.0f));
        maz::render::ShadowMap map2(512);
        map2.begin(maz::render::directionalLight(math::vec3(-6.0f, 0.0f, -6.0f),
                                                 math::vec3(6.0f, 2.5f, 6.0f), slant));
        map2.add(floorMesh);
        map2.add(standing);

        Surface slanted = lit;
        slanted.keyDirection = slant;
        slanted.shadows = &map2;

        // Sample the floor by hand rather than through a camera, so the answer is about the shadow and
        // not about where a pixel happened to land.
        auto floorShadow = [&](float x, float z) {
            return maz::render::shadowFactor(map2, math::vec3(x, 0.0f, z),
                                             math::vec3(0.0f, 1.0f, 0.0f));
        };
        // `slant` is the direction the light TRAVELS — toward -x — so it comes from +x and the shadow
        // is thrown to -x. The box is half a metre wide and 1.6 tall, so its shadow reaches from its
        // own foot out to about x = -1.6 and stops.
        check(floorShadow(-0.56f, 0.0f) > 0.85f,
              "the floor right at the foot of a standing box is in its shadow");
        check(floorShadow(-0.90f, 0.0f) > 0.85f, "and so is the floor a little further along it");
        check(floorShadow(-3.0f, 0.0f) < 0.15f, "while the floor beyond the end of it is not");
        check(floorShadow(1.2f, 0.0f) < 0.15f,
              "and neither is the floor on the side the light comes from");

        check(floorShadow(-1.0f, 0.0f) > floorShadow(1.0f, 0.0f),
              "a shadow falls away from the light, not toward it");

        // OFF THE EDGE OF THE MAP. A shadow map covers a box of world; anything outside it is simply
        // unknown. Unknown has to read as LIT, because the alternative is that a character who walks
        // out of the lit box turns black on a frame boundary.
        check(floorShadow(400.0f, 0.0f) < 0.01f, "a point outside the light's box is lit, not black");
        check(floorShadow(-400.0f, 7.0f) < 0.01f, "and so is one outside it the other way");

        // GRAZING INCIDENCE. A surface nearly edge-on to the light is where a shadow map's own
        // resolution turns into self-shadowing: one texel sideways is a long way along the light's
        // depth axis, so the neighbours a soft edge samples disagree with the middle by more than the
        // thickness of the thing being sampled. This is the case the normal offset is for, and it is
        // the case that decides whether that line of code earns its place.
        {
            const shapes::MeshData plate = maz::render::applyTransform(
                maz::render::applyTransform(shapes::makeBox(1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}),
                                            maz::render::scaleMatrix(math::vec3(6.0f, 0.02f, 6.0f))),
                maz::render::translationMatrix(math::vec3(0.0f, 1.0f, 0.0f)));
            const math::vec3 grazing = math::normalize(math::vec3(-0.996f, -0.087f, 0.0f)); // 85 degrees
            maz::render::ShadowMap graze(512);
            graze.begin(maz::render::directionalLight(math::vec3(-4.0f, 0.9f, -4.0f),
                                                      math::vec3(4.0f, 1.1f, 4.0f), grazing));
            graze.add(plate);
            int selfShadowed = 0;
            for (int i = -12; i <= 12; ++i) {
                const float x = static_cast<float>(i) * 0.2f;
                if (maz::render::shadowFactor(graze, math::vec3(x, 1.01f, 0.0f),
                                              math::vec3(0.0f, 1.0f, 0.0f)) > 0.34f) {
                    ++selfShadowed;
                }
            }
            check(selfShadowed == 0,
                  "a surface nearly edge-on to the light does not shadow itself");
        }

        // A SOFT EDGE. Nine samples rather than one, so the boundary of a shadow is a gradient a few
        // pixels wide instead of a staircase. Somewhere across that boundary there must be a value
        // that is neither lit nor fully shadowed; with one sample there never is.
        {
            bool halfway = false;
            for (int i = 0; i <= 400; ++i) {
                const float f = floorShadow(-1.55f - static_cast<float>(i) * 0.0006f, 0.0f);
                if (f > 0.08f && f < 0.92f) {
                    halfway = true;
                }
            }
            check(halfway, "the edge of a shadow is soft, not a staircase");
        }

        // OFF THE SIDE of the map, rather than out of its depth range: a point at the same depth as
        // the floor but a long way outside the light's box. Lit, for the same reason.
        check(maz::render::shadowFactor(map, math::vec3(400.0f, 0.0f, 0.0f),
                                        math::vec3(0.0f, 1.0f, 0.0f)) < 0.01f,
              "a point beside the light's box, at a depth it knows, is still lit");

        // AND THE PRICE of casting from back faces, stated rather than discovered: a single-sided
        // surface has no far side, so it casts nothing. The test floor here is one bare quad, and it
        // throws no shadow of its own on anything.
        {
            maz::render::ShadowMap flat(256);
            flat.begin(maz::render::directionalLight(math::vec3(-6.0f, -2.0f, -6.0f),
                                                     math::vec3(6.0f, 2.0f, 6.0f), down));
            flat.add(floorMesh);
            check(maz::render::shadowFactor(flat, math::vec3(0.0f, -1.0f, 0.0f),
                                            math::vec3(0.0f, 1.0f, 0.0f)) < 0.01f,
                  "a single-sided surface casts no shadow, which is what back-face casting costs");
        }

        // THE WHOLE BOX, from every angle. The light's frustum is fitted to a sphere around the box
        // rather than to the box itself, because the box's own silhouette changes as the light turns
        // and a frustum that changes with it leaves corners of the world outside the map — which does
        // not look like a bug, it looks like shadows that stop at an invisible line.
        {
            const math::vec3 lo(-6.0f, 0.0f, -6.0f);
            const math::vec3 hi(6.0f, 2.5f, 6.0f);
            int outside = 0;
            for (int a = 0; a < 12; ++a) {
                const float ang = static_cast<float>(a) * 0.5236f; // every 30 degrees around
                const math::vec3 dir = math::normalize(
                    math::vec3(std::cos(ang) * 0.7f, -0.6f, std::sin(ang) * 0.7f));
                const math::mat4 lvp = maz::render::directionalLight(lo, hi, dir);
                for (int c = 0; c < 8; ++c) {
                    const math::vec3 corner((c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y,
                                            (c & 4) ? hi.z : lo.z);
                    const math::vec4 clip = lvp * math::vec4(corner, 1.0f);
                    if (clip.w <= 0.0f) {
                        ++outside;
                        continue;
                    }
                    const math::vec3 ndc = math::vec3(clip) / clip.w;
                    if (std::fabs(ndc.x) > 1.0f || std::fabs(ndc.y) > 1.0f || ndc.z < 0.0f ||
                        ndc.z > 1.0f) {
                        ++outside;
                    }
                }
            }
            check(outside == 0, "every corner of the world the light is told about is inside its map, "
                                "from every direction the light can come from");
        }

        // A CURVED surface at the map's own resolution. This is the one that actually bit, and it took
        // rendering a film to find: an arm is about five centimetres across and a shadow map over a
        // room has about one-centimetre texels, so near the silhouette of a limb the far side the map
        // recorded and the near side being lit are within a texel of each other, and the soft edge's
        // neighbouring samples straddle the two. The result is a crawling mottle over every rounded
        // thing in the frame — over the actors, in other words, which is all anybody is looking at.
        //
        // It cannot be reproduced with flat plates at any thickness or angle, which is why an earlier
        // version of this test file concluded the bias earned nothing and it was taken out. It earns
        // its place here.
        {
            const shapes::MeshData arm = maz::render::applyTransform(
                shapes::makeCapsule(0.027f, 0.26f, 16, 5, Color{1.0f, 1.0f, 1.0f, 1.0f}),
                maz::render::translationMatrix(math::vec3(0.0f, 1.2f, 0.0f)));
            const math::vec3 fromSide = math::normalize(math::vec3(-0.62f, -0.72f, 0.31f));
            maz::render::ShadowMap limb(1024);
            // A box the size of an acting area, which is what sets the texels to a centimetre.
            limb.begin(maz::render::directionalLight(math::vec3(-4.5f, -0.15f, -1.6f),
                                                     math::vec3(4.5f, 2.6f, 4.4f), fromSide));
            limb.add(arm);

            // Walk the lit side of the limb. Every one of these points faces the light with nothing
            // between, so every one of them must be lit.
            int mottled = 0;
            int looked = 0;
            for (int i = 0; i <= 40; ++i) {
                const float y = 1.09f + static_cast<float>(i) * 0.0075f;
                for (int a = -6; a <= 6; ++a) {
                    // The side that faces the light: the direction opposite the key, in the XZ plane.
                    const float ang = -0.464f + static_cast<float>(a) * 0.10f;
                    const math::vec3 n(std::cos(ang), 0.0f, std::sin(ang));
                    if (math::dot(n, fromSide) > -0.25f) {
                        continue;                      // not actually facing the light; skip
                    }
                    ++looked;
                    if (maz::render::shadowFactor(limb, math::vec3(n.x * 0.0271f, y, n.z * 0.0271f),
                                                  n) > 0.2f) {
                        ++mottled;
                    }
                }
            }
            check(looked > 100, "there are plenty of points on the lit side of the limb to check");
            check(mottled == 0, "a limb does not shadow itself where the light plainly reaches it");
        }

        // A surface already turned away from the key is not darkened twice.
        Surface facingAway = slanted;
        Image back(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r3(W, H);
        r3.clearDepth();
        r3.draw(back, quadAt(0.0f, 1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}), math::mat4(1.0f), vp,
                facingAway);
        Surface facingAwayNoShadow = facingAway;
        facingAwayNoShadow.shadows = nullptr;
        Image back2(W, H, Color{0.0f, 0.0f, 0.0f, 1.0f});
        SoftRaster r4(W, H);
        r4.clearDepth();
        r4.draw(back2, quadAt(0.0f, 1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}), math::mat4(1.0f), vp,
                facingAwayNoShadow);
        near(back.getPixel(W / 2, H / 2).r, back2.getPixel(W / 2, H / 2).r, 1.0f / 255.0f,
             "a surface already turned away from the key is not darkened a second time");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("softraster: all checks passed\n");
    return 0;
}
