#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

// maz::core::Replay — deterministic input record/replay for a fixed-step simulation, the foundation
// Godot leans on for reproducible runs (and the basis of replays, ghosts, netcode rollback, and
// automated play-tests). The idea: if the simulation is deterministic (fixed timestep + the seeded
// core::Random + no wall-clock reads), then the ONLY nondeterministic input is the player's
// controls. Record one small input snapshot per fixed step and you can replay the entire session
// bit-for-bit by feeding those snapshots back in the same order.
//
// Replay<T> stores a stream of frames of a trivially-copyable input type T (e.g. a bitmask of held
// buttons + an analog stick). record() appends a frame; frame(i) reads one back. serialize() writes
// a compact, versioned, little-endian binary blob that RLE-compresses identical consecutive frames
// — which is most of them, since input rarely changes every 1/60 s — so a idle minute costs a
// handful of bytes, not thousands. load() restores it. Endianness is fixed (LE) so a replay records
// on one machine and plays on another. Header-only, no GPU, no threads, no I/O — the caller owns
// the bytes.
namespace maz::core {

template <typename T> class Replay {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Replay<T> requires a trivially-copyable (POD-like) input type");

  public:
    // ---- recording ----
    void record(const T& input) { m_frames.push_back(input); }
    void clear() { m_frames.clear(); }

    size_t size() const { return m_frames.size(); }
    bool empty() const { return m_frames.empty(); }

    // Read frame i. Reading past the end returns the last frame (so a consumer that runs longer
    // than the replay simply holds the final input) — or a default-constructed T if empty.
    const T& frame(size_t i) const {
        static const T kDefault{};
        if (m_frames.empty()) {
            return kDefault;
        }
        return m_frames[i < m_frames.size() ? i : m_frames.size() - 1];
    }

    const std::vector<T>& frames() const { return m_frames; }

    // ---- serialization (versioned, little-endian, RLE) ----
    // Layout: "MZRP" | u32 version | u32 frameSize | u32 frameCount | u32 runCount |
    //         runCount x [ u32 count | frameSize bytes ]
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;
        putTag(out, 'M', 'Z', 'R', 'P');
        putU32(out, kVersion);
        putU32(out, static_cast<uint32_t>(sizeof(T)));
        putU32(out, static_cast<uint32_t>(m_frames.size()));

        // Build RLE runs of identical consecutive frames.
        std::vector<std::pair<uint32_t, T>> runs;
        for (const T& f : m_frames) {
            if (!runs.empty() && sameBytes(runs.back().second, f)) {
                ++runs.back().first;
            } else {
                runs.emplace_back(1u, f);
            }
        }
        putU32(out, static_cast<uint32_t>(runs.size()));
        for (const auto& [count, value] : runs) {
            putU32(out, count);
            const auto* p = reinterpret_cast<const uint8_t*>(&value);
            out.insert(out.end(), p, p + sizeof(T));
        }
        return out;
    }

    // Restore from bytes produced by serialize(). Returns false on a bad tag, version, or a
    // frameSize that doesn't match T (so a replay authored for a different input struct is rejected
    // rather than silently misread). On failure the replay is left empty.
    bool load(const std::vector<uint8_t>& bytes) {
        clear();
        size_t off = 0;
        if (bytes.size() < 20) {
            return false;
        }
        if (bytes[0] != 'M' || bytes[1] != 'Z' || bytes[2] != 'R' || bytes[3] != 'P') {
            return false;
        }
        off = 4;
        const uint32_t version = getU32(bytes, off);
        const uint32_t frameSize = getU32(bytes, off);
        const uint32_t frameCount = getU32(bytes, off);
        const uint32_t runCount = getU32(bytes, off);
        if (version != kVersion || frameSize != sizeof(T)) {
            return false;
        }
        // Guard against a hostile header. frameCount and each run's count are untrusted u32s (up to ~4.29e9);
        // trusting frameCount, reserve(frameCount) alone attempts a multi-gigabyte allocation and OOM-crashes
        // the process on a tiny replay file, and a single run with a huge count would then push_back billions
        // of frames. RLE means the frame total can legitimately exceed the byte count, so bound it by a
        // generous absolute ceiling (far beyond any real input replay) rather than by input size, cap the
        // up-front reservation, and reject any run that would overrun the declared total.
        constexpr size_t kMaxFrames = 1u << 24; // ~16.7M frames (~77 h at 60 Hz) — far above real use
        if (frameCount > kMaxFrames) {
            return false;
        }
        m_frames.reserve(frameCount < (1u << 20) ? frameCount : (1u << 20));
        for (uint32_t r = 0; r < runCount; ++r) {
            if (off + 4 + sizeof(T) > bytes.size()) {
                clear();
                return false;
            }
            const uint32_t count = getU32(bytes, off);
            // A run whose count would push the total past the declared frameCount (and thus the cap) is
            // corrupt; reject rather than expand unboundedly. m_frames.size() <= frameCount holds here, so
            // the subtraction cannot underflow.
            if (count > frameCount - m_frames.size()) {
                clear();
                return false;
            }
            T value{};
            std::memcpy(&value, bytes.data() + off, sizeof(T));
            off += sizeof(T);
            for (uint32_t c = 0; c < count; ++c) {
                m_frames.push_back(value);
            }
        }
        if (m_frames.size() != frameCount) { // run expansion must match the declared count
            clear();
            return false;
        }
        return true;
    }

  private:
    static constexpr uint32_t kVersion = 1;

    static bool sameBytes(const T& a, const T& b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }

    static void putTag(std::vector<uint8_t>& o, char a, char b, char c, char d) {
        o.push_back(static_cast<uint8_t>(a));
        o.push_back(static_cast<uint8_t>(b));
        o.push_back(static_cast<uint8_t>(c));
        o.push_back(static_cast<uint8_t>(d));
    }
    static void putU32(std::vector<uint8_t>& o, uint32_t v) {
        o.push_back(static_cast<uint8_t>(v & 0xFF));
        o.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        o.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        o.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }
    static uint32_t getU32(const std::vector<uint8_t>& b, size_t& off) {
        const uint32_t v =
            static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8) |
            (static_cast<uint32_t>(b[off + 2]) << 16) | (static_cast<uint32_t>(b[off + 3]) << 24);
        off += 4;
        return v;
    }

    std::vector<T> m_frames;
};

} // namespace maz::core
