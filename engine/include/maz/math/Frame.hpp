#pragma once

#include "maz/math/Math.hpp" // vec3

// maz::math FRAME — the orientation carried along a curve: a unit TANGENT pointing the way the curve is
// going, and two perpendicular axes to go with it, so "which way is up" and "which way is sideways" have
// answers at every point. It is what you need the moment something has to follow a path with a facing —
// a camera on a rail, a rope or tube swept along a spline, a road's cross-section, text running around a
// curve, a character banking through a corner.
//
// The struct is deliberately here, on its own, rather than in whichever header first needed it:
// ParallelTransport.hpp and RotationMinimizingFrame.hpp both compute frames and both used to declare
// their OWN `maz::math::Frame` with the same members and different defaults, which meant the two headers
// could not be included in the same file — the second was a redefinition. Comparing the two methods,
// which is a thing anyone choosing between them wants to do, was a compile error.
//
// The convention both follow: all three axes are unit length, and `binormal` is `cross(tangent, normal)`.
// The defaults below are only a placeholder for a default-constructed Frame and satisfy that convention;
// every frame either producer returns has all three assigned.
namespace maz::math {

struct Frame {
    vec3 tangent{0.0f, 0.0f, 1.0f};
    vec3 normal{1.0f, 0.0f, 0.0f};
    vec3 binormal{0.0f, 1.0f, 0.0f}; // == cross(tangent, normal)
};

} // namespace maz::math
