#pragma once

#include "maz/assets/Model.hpp"

// Procedural primitive meshes — the geometry building blocks of the character/item creator.
//
// Each generator fills an assets::Mesh (interleaved position/normal/uv + a triangle index list)
// entirely on the CPU, so it can be unit-tested with no GPU and composited into a larger model by
// CompositeAsset. Meshes are centered on the origin; the caller positions them via a transform.
// The API is plain floats/ints on purpose so this header stays free of any math/GLM dependency.

namespace maz::assets {

// Which parametric shape a part is. The order is serialized by name (see CompositeAsset), so new
// kinds should be appended rather than inserted.
enum class PrimitiveKind { Box, Sphere, Cylinder, Plane };

// Parameters for makePrimitive(). Each shape reads only the fields it needs:
//   Box      -> size (full extents on x/y/z)
//   Sphere   -> radius, segments (longitude), rings (latitude)
//   Cylinder -> radius, height, segments (around the axis)
//   Plane    -> size.x (width) and size.z (depth)
struct PrimitiveParams {
    float size[3]{1.0f, 1.0f, 1.0f};
    float radius = 0.5f;
    float height = 1.0f;
    int segments = 16;
    int rings = 8;
};

// A box centered on the origin with full extents sx/sy/sz: 24 vertices (per-face normals) / 36 indices.
Mesh makeBox(float sx, float sy, float sz);

// A UV sphere of the given radius: (rings+1)*(segments+1) vertices, rings*segments*6 indices.
// `segments` (>= 3) is longitude subdivision; `rings` (>= 2) is latitude subdivision.
Mesh makeSphere(float radius, int segments, int rings);

// A capped cylinder of the given radius and height, axis along +Y, centered on the origin.
Mesh makeCylinder(float radius, float height, int segments);

// A flat w x d quad in the XZ plane at y=0, facing +Y: 4 vertices / 6 indices.
Mesh makePlane(float width, float depth);

// Dispatch to the generator for `kind`, reading the fields of `params` that shape uses.
Mesh makePrimitive(PrimitiveKind kind, const PrimitiveParams& params);

} // namespace maz::assets
