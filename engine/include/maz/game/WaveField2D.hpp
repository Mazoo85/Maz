#pragma once

#include <cstddef>
#include <vector>

// maz::game 2D wave / ripple simulation — the "water surface" effect on a grid.
//
// Drop a stone in a pond and rings spread out, bounce off the edges, cross each other, and fade. This
// simulates that on a height grid with the classic two-buffer wave step (Hugo Elias' water algorithm,
// a discretisation of the wave equation): each cell's next height is the average of its four neighbours'
// current heights minus its own PREVIOUS height, times a damping factor. That one line reproduces
// travelling ripples, interference between multiple drops, reflection at the boundary, and gradual
// decay. Feed the height (or its gradient) into a normal map / UV distortion to render water, force
// fields, shockwaves, or a trampoline surface. Border cells are held at rest (a fixed shore). Pure CPU,
// header-only, deterministic — unit-tested for a still surface staying still, radial symmetry of a
// centred drop, outward propagation, and amplitude decay under damping.
namespace maz::game {

class WaveField2D {
public:
    WaveField2D() = default;
    WaveField2D(int w, int h, float damping = 0.02f) { resize(w, h, damping); }

    void resize(int w, int h, float damping = 0.02f) {
        width_ = w < 0 ? 0 : w;
        height_ = h < 0 ? 0 : h;
        damping_ = damping;
        const std::size_t n = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
        cur_.assign(n, 0.0f);
        prev_.assign(n, 0.0f);
    }

    int width() const { return width_; }
    int height() const { return height_; }

    // Height at (x,y). Out-of-range reads return 0.
    float at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0.0f;
        return cur_[idx(x, y)];
    }

    // Poke the surface at (x,y) — a drop of the given amplitude.
    void disturb(int x, int y, float amount) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
        cur_[idx(x, y)] += amount;
    }

    // Advance one time step. Interior cells update; the border stays at rest.
    void step() {
        if (width_ < 3 || height_ < 3) return;
        for (int y = 1; y < height_ - 1; ++y) {
            for (int x = 1; x < width_ - 1; ++x) {
                const std::size_t i = idx(x, y);
                const float neighbours = cur_[idx(x - 1, y)] + cur_[idx(x + 1, y)] +
                                         cur_[idx(x, y - 1)] + cur_[idx(x, y + 1)];
                float next = neighbours * 0.5f - prev_[i];
                next -= next * damping_; // dissipate energy
                prev_[i] = next;         // prev_ becomes the new heights this pass
            }
        }
        cur_.swap(prev_); // prev_ now holds the freshly computed field; make it current
        // Hold the border at rest (a fixed shore), independent of history.
        for (int x = 0; x < width_; ++x) {
            cur_[idx(x, 0)] = 0.0f;
            cur_[idx(x, height_ - 1)] = 0.0f;
        }
        for (int y = 0; y < height_; ++y) {
            cur_[idx(0, y)] = 0.0f;
            cur_[idx(width_ - 1, y)] = 0.0f;
        }
    }

    // Sum of absolute heights — a proxy for the wave's total energy (useful to see it decay).
    float totalAmplitude() const {
        float s = 0.0f;
        for (float v : cur_) s += v < 0.0f ? -v : v;
        return s;
    }

private:
    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    int width_ = 0;
    int height_ = 0;
    float damping_ = 0.02f;
    std::vector<float> cur_;
    std::vector<float> prev_;
};

} // namespace maz::game
