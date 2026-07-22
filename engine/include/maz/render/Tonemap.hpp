#pragma once

// maz::render tonemapping operators — the HDR-to-displayable color curves every modern renderer applies
// as its final step. A physically-lit scene produces radiance well above 1.0 (a bright sky, a specular
// highlight, an explosion); a display can only show [0,1], so a tonemap curve compresses that unbounded
// range into the visible one while keeping shadows, midtones, and highlights looking natural. This is
// the CPU-side math (what the tonemap fragment shader evaluates per pixel): ACES filmic (Narkowicz's
// widely-used fit — the current default look, with its characteristic gentle shoulder and slight
// contrast), Reinhard (the classic x/(1+x)) and its white-point-extended form, and the Hejl-Burgess /
// Uncharted2 filmic curve. Pure float math, no allocation — exactly unit-testable (monotonic, maps 0 to
// 0, saturates to 1). The engine already has the HDR target + GPU tonemap pass; this is the reusable,
// testable curve itself (also handy for CPU-side color grading, thumbnails, and golden images).
namespace maz::render {

inline float tonemapClamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

struct Color3 {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

// ACES filmic tonemap (Krzysztof Narkowicz's fitted approximation). Input is linear HDR (>= 0); output
// is display-referred [0,1].
inline float acesFilmic(float x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return tonemapClamp01((x * (a * x + b)) / (x * (c * x + d) + e));
}
inline Color3 acesFilmic(const Color3& col) {
    return Color3{acesFilmic(col.r), acesFilmic(col.g), acesFilmic(col.b)};
}

// Reinhard tonemap: x / (1 + x). Simple, never clips, but desaturates highlights.
inline float reinhard(float x) {
    const float v = x < 0.0f ? 0.0f : x;
    return v / (1.0f + v);
}

// Extended Reinhard with a white point: values at or above whitePoint map to (nearly) 1.
inline float reinhardExtended(float x, float whitePoint) {
    const float v = x < 0.0f ? 0.0f : x;
    const float w = whitePoint > 0.0f ? whitePoint : 1.0f;
    return (v * (1.0f + v / (w * w))) / (1.0f + v);
}

// Uncharted 2 / Hejl-Burgess filmic curve (the "John Hable" operator), used with a white-scale divide.
inline float uncharted2Partial(float x) {
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
inline float uncharted2(float x, float exposureBias = 2.0f, float whitePoint = 11.2f) {
    const float mapped = uncharted2Partial(exposureBias * x);
    const float whiteScale = 1.0f / uncharted2Partial(whitePoint);
    return tonemapClamp01(mapped * whiteScale);
}

// Apply a linear exposure multiplier before tonemapping (stops = log2 of the multiplier).
inline float applyExposure(float x, float exposure) { return x * exposure; }

} // namespace maz::render
