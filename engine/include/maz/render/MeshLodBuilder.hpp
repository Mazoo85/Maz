#pragma once

#include "maz/render/MeshLod.hpp"             // render::LodChain (selection half)
#include "maz/render/MeshSimplifyQuadric.hpp" // render::simplifyQuadric (feature-preserving decimation)
#include "maz/render/Shapes.hpp"              // shapes::MeshData

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// maz::render LOD-ladder builder — the one-call import step that turns a single authored mesh into a full
// level-of-detail set, the equivalent of Godot's ImporterMesh.generate_lods(). The two halves already exist
// separately: simplifyQuadric() (MeshSimplifyQuadric.hpp) GENERATES one lower-poly mesh at a triangle budget,
// and LodChain (MeshLod.hpp) SELECTS which level to draw from an object's on-screen pixel size. buildMeshLods()
// assembles them: it runs the quadric decimator at a geometric sequence of triangle budgets to produce the
// mesh ladder AND fills a LodChain with a descending pixel threshold per level, so the result drops straight
// into the renderer — draw meshes[chain.selectForCamera(radius, dist, fov, viewportH)]. Each coarser level is
// decimated from the ORIGINAL mesh (not the level above), so error never compounds down the ladder. Pure CPU
// geometry + arithmetic; unit-tests headlessly.
namespace maz::render {

// A generated LOD set: the mesh ladder (index 0 = full detail, coarsening) and a LodChain whose thresholds
// line up index-for-index with `meshes`, so a selected LOD index is a direct index into `meshes`.
struct MeshLodSet {
    std::vector<shapes::MeshData> meshes;
    LodChain chain;
    std::size_t count() const { return meshes.size(); }
};

// Build a LOD set from one authored mesh. meshes[0] is `base` unchanged. Each subsequent level targets
// `ratio` (0<ratio<1) of the PREVIOUS level's actual triangle count via quadric-error decimation, decimated
// from the original `base` so error does not compound. The ladder stops when the next target would fall below
// `minTriangles`, when a level fails to actually shrink (decimator converged / hit a topology floor), or when
// `maxLevels` levels exist. The finest level's selection threshold is `fullDetailPixels`; each coarser level's
// threshold is scaled by `ratio` so LOD swaps track the triangle reduction (descending, as LodChain requires).
inline MeshLodSet buildMeshLods(const shapes::MeshData& base, float ratio = 0.5f,
                                std::size_t minTriangles = 32, std::size_t maxLevels = 6,
                                float fullDetailPixels = 200.0f) {
    if (ratio <= 0.0f || ratio >= 1.0f) ratio = 0.5f;
    if (minTriangles < 1) minTriangles = 1;

    MeshLodSet set;
    const std::size_t baseTris = base.indices.size() / 3;
    set.meshes.push_back(base);
    set.chain.addLevel(fullDetailPixels);
    if (baseTris == 0 || maxLevels <= 1) return set;

    std::size_t prevTris = baseTris;
    float threshold = fullDetailPixels;
    while (set.meshes.size() < maxLevels) {
        const std::size_t target = static_cast<std::size_t>(
            std::floor(static_cast<double>(prevTris) * static_cast<double>(ratio)));
        if (target < minTriangles) break;
        shapes::MeshData lod = simplifyQuadric(base, target);
        const std::size_t tris = lod.indices.size() / 3;
        if (tris >= prevTris) break; // no progress — stop rather than emit a redundant level forever
        prevTris = tris;
        threshold *= ratio; // descend so the coarser level is chosen once the object shrinks past it
        set.meshes.push_back(std::move(lod));
        set.chain.addLevel(threshold);
    }
    return set;
}

} // namespace maz::render
