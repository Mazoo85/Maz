#pragma once

#include "maz/anim/Timeline.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

// FRAME SEQUENCE — turn a continuous keyframe Timeline into the DISCRETE list of frames a renderer
// draws to export a cutscene as a video / image sequence. A Timeline (see Timeline.hpp) is sampled at
// any time; rendering to a film needs a fixed cadence: N frames at a chosen fps, each a sample time the
// camera + scene are posed at. This is that bridge — the deterministic "what time is frame i, and how
// many frames" arithmetic — kept pure so the export driver's timing unit-tests headlessly (the actual
// GPU render of each posed frame is the owner/CI-verified half).
namespace maz::anim {

// A fixed-cadence sampling of a clip: `fps` frames per second across a clip `duration` (seconds). The
// sequence covers [0, duration] INCLUSIVE, so a 2 s clip at 30 fps yields 61 frames at t = 0 … 2 s.
struct FrameSequence {
    float fps = 30.0f;
    float duration = 0.0f; // seconds; <= 0 means a single still frame at t = 0

    // Build a sequence that spans a timeline's full length at the given fps.
    static FrameSequence forTimeline(const Timeline& tl, float fps) {
        return FrameSequence{fps, tl.length()};
    }

    // Number of frames in the sequence (always >= 1: an empty/zero-length clip is one still frame).
    int count() const {
        if (duration <= 0.0f || fps <= 0.0f) {
            return 1;
        }
        return static_cast<int>(std::floor(duration * fps + 1e-4f)) + 1;
    }

    // The clip time (seconds) that frame `index` is posed at.
    float timeAt(int index) const {
        return fps > 0.0f ? static_cast<float>(index) / fps : 0.0f;
    }

    // Every frame's sample time, in order.
    std::vector<float> times() const {
        const int n = count();
        std::vector<float> out;
        out.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.push_back(timeAt(i));
        }
        return out;
    }

    // Sample one named timeline track at each frame time (unknown track -> all zeros). The track is
    // sampled by explicit time, independent of the timeline's own playhead.
    std::vector<float> sample(const Timeline& tl, const std::string& track) const {
        const int n = count();
        std::vector<float> out;
        out.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.push_back(tl.valueAt(track, timeAt(i)));
        }
        return out;
    }
};

} // namespace maz::anim
