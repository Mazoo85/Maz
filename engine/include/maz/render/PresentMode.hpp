#pragma once

#include <algorithm>
#include <vector>

// maz::render present-mode selection — how the swapchain decides between the display sync modes the
// GPU/surface actually supports. This is the "vsync toggle / present-mode selection" policy, kept
// as a PURE function over an engine-side enum (mirroring Vulkan's VK_PRESENT_MODE_*) so it is
// unit-tested without a GPU; the Vulkan swapchain maps its real modes onto this and back.
//
//   Fifo        — hard vsync: queue images, present on vblank. No tearing, always supported. The
//                 safe default and the only mode guaranteed present.
//   FifoRelaxed — adaptive vsync: like Fifo, but if the app missed a vblank the next image tears
//                 instead of stalling a whole frame — smoother under load than hard vsync.
//   Mailbox     — triple-buffered: newest image replaces the queued one; no tearing AND low
//                 latency (the good "vsync off" for most desktops). Preferred when vsync is off.
//   Immediate   — no sync at all: lowest latency, but tears. Last resort when Mailbox is absent.
namespace maz::render {

enum class PresentMode { Fifo, FifoRelaxed, Mailbox, Immediate };

inline bool supportsMode(const std::vector<PresentMode>& supported, PresentMode m) {
    return std::find(supported.begin(), supported.end(), m) != supported.end();
}

// Pick the best present mode from what the surface supports.
//   vsync ON  → prefer FifoRelaxed (adaptive, smoother) when available, else Fifo (always there).
//   vsync OFF → prefer Mailbox (low-latency, no tearing); then Immediate if tearing is allowed;
//               otherwise fall back to Fifo. Fifo is the guaranteed-available backstop.
inline PresentMode choosePresentMode(const std::vector<PresentMode>& supported, bool vsync,
                                     bool allowTearing = true) {
    if (vsync) {
        if (supportsMode(supported, PresentMode::FifoRelaxed)) {
            return PresentMode::FifoRelaxed;
        }
        return PresentMode::Fifo;
    }
    if (supportsMode(supported, PresentMode::Mailbox)) {
        return PresentMode::Mailbox;
    }
    if (allowTearing && supportsMode(supported, PresentMode::Immediate)) {
        return PresentMode::Immediate;
    }
    return PresentMode::Fifo;
}

} // namespace maz::render
