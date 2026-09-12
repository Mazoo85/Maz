#pragma once

#include <cmath>

namespace maz::game {

// How two bodies' per-body friction / restitution scalars combine into the effective pair value (Godot
// PhysicsMaterial / Box2D). GeometricMean = sqrt(a*b) is the physically-standard friction combine and
// the engine's historical default; Max is the usual restitution choice. Shared by the 2D and 3D
// physics solvers so the same material semantics apply in both.
enum class CombineMode { GeometricMean, Average, Multiply, Min, Max };

inline float combineValue(CombineMode mode, float a, float b) {
    switch (mode) {
    case CombineMode::Average:
        return 0.5f * (a + b);
    case CombineMode::Multiply:
        return a * b;
    case CombineMode::Min:
        return a < b ? a : b;
    case CombineMode::Max:
        return a > b ? a : b;
    case CombineMode::GeometricMean:
    default:
        return std::sqrt((a < 0.0f ? 0.0f : a) * (b < 0.0f ? 0.0f : b));
    }
}

} // namespace maz::game
