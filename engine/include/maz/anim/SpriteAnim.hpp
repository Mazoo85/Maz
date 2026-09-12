#pragma once

#include <cstdint>
#include <vector>

namespace maz::anim {

// Sprite-sheet (flipbook) animation: play a sequence of UV sub-rectangles on a single texture over
// time — walk cycles, explosions, idle bobs, UI spinners. Pairs with SpriteDesc's uvMin/uvMax so a
// frame is drawn by copying the current SpriteFrame into those fields. Pure logic (frame timing),
// so it advances deterministically under the fixed timestep and unit-tests without a GPU.

// One frame: a UV sub-rectangle (0..1) of the sheet.
struct SpriteFrame {
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
};

// Build `count` frames from a regular `cols` x `rows` grid sheet, starting at cell `first` and
// walking row-major (left-to-right, top-to-bottom). Handy for evenly-sliced sprite strips/atlases.
inline std::vector<SpriteFrame> gridFrames(int cols, int rows, int first, int count) {
    std::vector<SpriteFrame> frames;
    if (cols <= 0 || rows <= 0 || count <= 0) {
        return frames;
    }
    frames.reserve(static_cast<size_t>(count));
    const float fw = 1.0f / static_cast<float>(cols);
    const float fh = 1.0f / static_cast<float>(rows);
    for (int k = 0; k < count; ++k) {
        const int idx = first + k;
        const int col = idx % cols;
        const int row = idx / cols;
        SpriteFrame f;
        f.u0 = static_cast<float>(col) * fw;
        f.u1 = f.u0 + fw;
        f.v0 = static_cast<float>(row) * fh;
        f.v1 = f.v0 + fh;
        frames.push_back(f);
    }
    return frames;
}

// Plays a frame list at a fixed rate. update(dt) advances the cursor; frame() is the current cell.
// Looping clips wrap forever; one-shot clips clamp on the last frame and report finished().
class SpriteAnim {
public:
    void play(std::vector<SpriteFrame> frames, float fps, bool loop = true) {
        m_frames = std::move(frames);
        m_frameDur = fps > 0.0f ? 1.0f / fps : 0.1f;
        m_loop = loop;
        reset();
    }
    void reset() {
        m_time = 0.0f;
        m_index = 0;
        m_finished = false;
    }

    void update(float dt) {
        if (m_frames.size() < 2 || m_finished) {
            return;
        }
        m_time += dt;
        while (m_time >= m_frameDur) {
            m_time -= m_frameDur;
            if (m_index + 1 < static_cast<int>(m_frames.size())) {
                ++m_index;
            } else if (m_loop) {
                m_index = 0;
            } else {
                m_finished = true; // clamp on the last frame
                m_time = 0.0f;
                break;
            }
        }
    }

    const SpriteFrame& frame() const {
        static const SpriteFrame kIdentity{};
        if (m_frames.empty()) {
            return kIdentity;
        }
        return m_frames[static_cast<size_t>(m_index)];
    }
    int index() const { return m_index; }
    int frameCount() const { return static_cast<int>(m_frames.size()); }
    bool finished() const { return m_finished; }

private:
    std::vector<SpriteFrame> m_frames;
    float m_frameDur = 0.1f;
    float m_time = 0.0f;
    int m_index = 0;
    bool m_loop = true;
    bool m_finished = false;
};

} // namespace maz::anim
