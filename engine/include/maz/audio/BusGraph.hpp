#pragma once

#include "maz/audio/Dsp.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace maz::audio {

// Audio bus graph — Godot's AudioServer bus layout. Every voice in a game plays into a named BUS
// ("Music", "SFX", "Voice", ...), and each bus carries a volume (in dB), mute / solo / bypass switches,
// an ordered chain of effects, and a SEND that routes its processed output into another bus. The buses
// form a forest rooted at "Master" (bus 0), whose output is what reaches the speakers — so you can drop
// a reverb on a "Reverb" bus and send several buses into it, duck all SFX with one fader, or solo the
// music while mixing. Maz already had a single linear effect chain (`Bus`); this is the multi-bus router
// on top of it. Pure per-sample math over the existing Effect chain — no device, no threads — so it
// unit-tests exactly (a −6 dB bus halves its signal; muting silences everything routed through it; solo
// keeps only the soloed bus's path to Master) and drives a golden mixer view.
//
// Processing model: push each source sample into its bus's input (`pushInput`), then call `process()`
// once per output sample. Buses are processed deepest-first so a bus sees all of its child sends before
// its own effects run; each bus applies its effects (unless bypassed), its dB gain, and its audibility
// (mute / solo), then adds the result into its send target's input. `process()` returns the Master
// output and clears the per-sample input accumulators.
//
// Scope note (honest): this is a MONO router — the routing, gain, mute/solo, bypass, and send semantics
// match Godot, but stereo bus processing (and stereo-aware effects) arrives with the stereo-effects
// milestone. A per-bus output level meter is exposed for metering/visualization.

class BusGraph {
public:
    BusGraph() { addBus("Master", -1); } // Master (index 0) sends to the output, not another bus

    // Add a bus that sends its output into bus `sendTo` (default: Master). Returns its index.
    int addBus(const std::string& name, int sendTo = 0) {
        Bus b;
        b.name = name;
        b.send = sendTo;
        m_buses.push_back(std::move(b));
        return static_cast<int>(m_buses.size()) - 1;
    }

    int busCount() const { return static_cast<int>(m_buses.size()); }
    int busIndex(const std::string& name) const {
        for (std::size_t i = 0; i < m_buses.size(); ++i) {
            if (m_buses[i].name == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    const std::string& busName(int i) const { return m_buses[static_cast<std::size_t>(i)].name; }

    void setVolumeDb(int i, float db) { at(i).volumeDb = db; }
    float volumeDb(int i) const { return at(i).volumeDb; }
    void setMute(int i, bool m) { at(i).mute = m; }
    bool muted(int i) const { return at(i).mute; }
    void setSolo(int i, bool s) { at(i).solo = s; }
    bool soloed(int i) const { return at(i).solo; }
    void setBypass(int i, bool b) { at(i).bypass = b; }
    bool bypassed(int i) const { return at(i).bypass; }
    void setSend(int i, int target) { at(i).send = target; }
    int send(int i) const { return at(i).send; }

    void addEffect(int i, std::unique_ptr<Effect> e) { at(i).effects.push_back(std::move(e)); }
    std::size_t effectCount(int i) const { return at(i).effects.size(); }

    // Feed one source sample into a bus's input for the current output sample.
    void pushInput(int bus, float sample) {
        if (bus >= 0 && bus < busCount()) {
            m_buses[static_cast<std::size_t>(bus)].input += sample;
        }
    }

    // Advance one sample: process every bus, route the sends, and return the Master output. Also updates
    // each bus's `level` (the absolute value of its output this sample) for metering, and clears inputs.
    float process() {
        const int n = busCount();
        bool anySolo = false;
        for (const Bus& b : m_buses) {
            anySolo = anySolo || b.solo;
        }
        // Solo keeps only buses on a soloed bus's send path to Master.
        std::vector<char> kept(static_cast<std::size_t>(n), 0);
        if (anySolo) {
            for (int i = 0; i < n; ++i) {
                if (!m_buses[static_cast<std::size_t>(i)].solo) {
                    continue;
                }
                int j = i, guard = 0;
                while (j >= 0 && j < n && guard++ <= n) {
                    kept[static_cast<std::size_t>(j)] = 1;
                    j = m_buses[static_cast<std::size_t>(j)].send;
                }
            }
        }
        // Depth = number of send-hops to Master; deeper buses process first.
        std::vector<int> depth(static_cast<std::size_t>(n), 0);
        for (int i = 0; i < n; ++i) {
            int j = i, d = 0;
            while (j > 0 && j < n && d <= n) {
                j = m_buses[static_cast<std::size_t>(j)].send;
                ++d;
            }
            depth[static_cast<std::size_t>(i)] = d;
        }
        std::vector<int> order(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            order[static_cast<std::size_t>(i)] = i;
        }
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return depth[static_cast<std::size_t>(a)] > depth[static_cast<std::size_t>(b)];
        });

        for (int idx : order) {
            Bus& b = m_buses[static_cast<std::size_t>(idx)];
            float x = b.input;
            if (!b.bypass) {
                for (auto& e : b.effects) {
                    x = e->process(x);
                }
            }
            x *= dbToLinear(b.volumeDb);
            const bool audible = anySolo ? (kept[static_cast<std::size_t>(idx)] != 0) : !b.mute;
            if (!audible) {
                x = 0.0f;
            }
            b.output = x;
            b.level = x < 0.0f ? -x : x;
            if (idx != 0 && b.send >= 0 && b.send < n && b.send != idx) {
                m_buses[static_cast<std::size_t>(b.send)].input += x;
            }
        }

        const float out = m_buses[0].output;
        for (Bus& b : m_buses) {
            b.input = 0.0f;
        }
        return out;
    }

    float busLevel(int i) const { return at(i).level; }

    void reset() {
        for (Bus& b : m_buses) {
            b.input = b.output = b.level = 0.0f;
            for (auto& e : b.effects) {
                e->reset();
            }
        }
    }

private:
    struct Bus {
        std::string name;
        float volumeDb = 0.0f;
        bool mute = false;
        bool solo = false;
        bool bypass = false;
        int send = 0; // index of the bus this one routes into (-1 = output, for Master)
        std::vector<std::unique_ptr<Effect>> effects;
        float input = 0.0f;
        float output = 0.0f;
        float level = 0.0f;
    };

    Bus& at(int i) { return m_buses[static_cast<std::size_t>(i)]; }
    const Bus& at(int i) const { return m_buses[static_cast<std::size_t>(i)]; }

    std::vector<Bus> m_buses;
};

} // namespace maz::audio
