// Unit tests for maz::audio::Sequencer + DrumVoice — pure DSP/logic, no audio device.
// Verifies the pattern grid, transport timing (sample-accurate stepping), and that triggered
// drum voices produce sound while an empty pattern stays silent.

#include "maz/audio/DrumVoice.hpp"
#include "maz/audio/MixerTrack.hpp"
#include "maz/audio/Sequencer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

double rms(const std::vector<float>& buf) {
    if (buf.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float s : buf) {
        sum += static_cast<double>(s) * static_cast<double>(s);
    }
    return std::sqrt(sum / static_cast<double>(buf.size()));
}

// Render `frames` of the sequencer into a fresh interleaved-stereo buffer (2*frames floats).
std::vector<float> renderMono(audio::Sequencer& seq, int frames, int sampleRate) {
    std::vector<float> buf(static_cast<size_t>(frames) * 2, 0.0f);
    seq.render(buf.data(), frames, sampleRate);
    return buf;
}

// Left/right RMS of an interleaved-stereo buffer.
double rmsChannel(const std::vector<float>& buf, int ch) {
    const size_t n = buf.size() / 2;
    if (n == 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double v = static_cast<double>(buf[i * 2 + static_cast<size_t>(ch)]);
        sum += v * v;
    }
    return std::sqrt(sum / static_cast<double>(n));
}

} // namespace

int main() {
    const int sampleRate = 48000;

    // --- DrumVoice -----------------------------------------------------------
    audio::DrumVoice kick;
    kick.setType(audio::Drum::Kick);
    check(!kick.active(), "drum voice starts inactive");

    std::vector<float> quiet(4800, 0.0f);
    kick.render(quiet.data(), 4800, sampleRate);
    check(rms(quiet) == 0.0, "inactive voice renders silence");

    kick.trigger();
    check(kick.active(), "voice is active after trigger");
    std::vector<float> hit(4800, 0.0f); // 0.1 s
    kick.render(hit.data(), 4800, sampleRate);
    check(rms(hit) > 0.0, "triggered kick produces sound");

    // A closed hat is short — after ~0.3 s it should have decayed to inactive.
    audio::DrumVoice hat;
    hat.setType(audio::Drum::ClosedHat);
    hat.trigger();
    std::vector<float> tail(static_cast<size_t>(sampleRate) / 2, 0.0f);
    hat.render(tail.data(), sampleRate / 2, sampleRate);
    check(!hat.active(), "closed hat decays to inactive");

    // --- Per-voice noise decorrelation (setNoiseSeed) ------------------------
    {
        // Two closed hats (noise-driven) with DIFFERENT seeds must render different noise, so stacking
        // two same-type channels sums incoherently instead of doubling a bit-identical signal.
        auto renderHat = [&](uint32_t seed) {
            audio::DrumVoice h;
            h.setType(audio::Drum::ClosedHat);
            h.setNoiseSeed(seed);
            h.trigger();
            std::vector<float> b(2048, 0.0f);
            h.render(b.data(), 2048, sampleRate);
            return b;
        };
        const std::vector<float> a = renderHat(0x1234567u);
        const std::vector<float> b = renderHat(0x1234567u + 0x9E3779B9u);
        const std::vector<float> a2 = renderHat(0x1234567u);
        int diff = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) {
                ++diff;
            }
        }
        check(diff > 1000, "different seeds decorrelate two same-type drum voices' noise");
        check(a == a2, "the same seed reproduces bit-identical noise (still deterministic)");
    }

    // --- Drum pitch-envelope depth ("punch") --------------------------------
    {
        // High-frequency content via the first difference — a deeper initial pitch sweep starts
        // higher and moves faster, so it carries more HF energy over the attack.
        auto hfEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i]) - static_cast<double>(b[i - 1]);
                e += d * d;
            }
            return e;
        };
        const int n = 1500; // ~31 ms, spanning the kick's pitch sweep

        audio::DrumVoice flat;
        flat.setType(audio::Drum::Kick);
        check(std::fabs(flat.pitchEnv() - 1.0f) < 1e-6f, "kick pitch-env defaults to natural (1.0)");
        flat.setPitchEnv(0.0f); // no sweep — a flat sub tone
        flat.trigger(1.0f);
        std::vector<float> fb(static_cast<size_t>(n), 0.0f);
        flat.render(fb.data(), n, sampleRate);

        audio::DrumVoice punch;
        punch.setType(audio::Drum::Kick);
        punch.setPitchEnv(2.0f); // a deeper, snappier sweep
        punch.trigger(1.0f);
        std::vector<float> pb(static_cast<size_t>(n), 0.0f);
        punch.render(pb.data(), n, sampleRate);

        check(hfEnergy(pb) > hfEnergy(fb) * 1.5,
              "a deeper pitch-env gives the kick more attack (HF) than a flat sub");

        // The amount clamps into [0, 2].
        audio::DrumVoice clamp;
        clamp.setPitchEnv(5.0f);
        check(std::fabs(clamp.pitchEnv() - 2.0f) < 1e-6f, "pitch-env clamps to 2.0");
        clamp.setPitchEnv(-1.0f);
        check(clamp.pitchEnv() == 0.0f, "pitch-env clamps to 0");

        // Pitch-env time: a longer sweep keeps the pitch high longer → more HF energy over the hit.
        check(std::fabs(clamp.pitchEnvTime() - 1.0f) < 1e-6f, "pitch-env time defaults to 1.0");
        audio::DrumVoice tight;
        tight.setType(audio::Drum::Kick);
        tight.setPitchEnvTime(0.3f); // a fast drop (snappy click)
        tight.trigger(1.0f);
        std::vector<float> tb(static_cast<size_t>(n), 0.0f);
        tight.render(tb.data(), n, sampleRate);
        audio::DrumVoice boom;
        boom.setType(audio::Drum::Kick);
        boom.setPitchEnvTime(4.0f); // a long, boomy drop
        boom.trigger(1.0f);
        std::vector<float> bb(static_cast<size_t>(n), 0.0f);
        boom.render(bb.data(), n, sampleRate);
        check(hfEnergy(bb) > hfEnergy(tb) * 1.5,
              "a longer pitch-env time keeps the kick pitch up longer (more HF energy)");
        audio::DrumVoice ptc;
        ptc.setPitchEnvTime(10.0f);
        check(std::fabs(ptc.pitchEnvTime() - 4.0f) < 1e-6f, "pitch-env time clamps to 4.0");
    }

    // --- Per-channel drum tone (low-pass darkening) -------------------------
    {
        auto hfEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i]) - static_cast<double>(b[i - 1]);
                e += d * d;
            }
            return e;
        };
        const int n = sampleRate / 40; // ~25 ms, while the hat is ringing

        audio::DrumVoice open;
        open.setType(audio::Drum::ClosedHat);
        check(open.toneCutoff() == 20000.0f, "drum tone defaults to open (20 kHz)");
        open.trigger();
        std::vector<float> ob(static_cast<size_t>(n), 0.0f);
        open.render(ob.data(), n, sampleRate);

        audio::DrumVoice dark;
        dark.setType(audio::Drum::ClosedHat);
        dark.setToneCutoff(800.0f);
        dark.trigger();
        std::vector<float> db(static_cast<size_t>(n), 0.0f);
        dark.render(db.data(), n, sampleRate);

        check(hfEnergy(db) < hfEnergy(ob) * 0.5,
              "a low tone cutoff darkens the hit (much less HF energy)");
        check(rms(db) > 0.0, "the darkened hit still produces sound");

        audio::DrumVoice cl;
        cl.setToneCutoff(50.0f);
        check(cl.toneCutoff() == 200.0f, "drum tone clamps to a 200 Hz minimum");
    }

    // --- Per-channel drum high-pass / low-cut -------------------------------
    {
        const int hn = sampleRate / 10; // 100 ms of the kick body

        audio::DrumVoice full;
        full.setType(audio::Drum::Kick);
        check(full.highpassCutoff() == 0.0f, "drum high-pass defaults to off");
        full.trigger();
        std::vector<float> fb(static_cast<size_t>(hn), 0.0f);
        full.render(fb.data(), hn, sampleRate);

        audio::DrumVoice thin;
        thin.setType(audio::Drum::Kick);
        thin.setHighpassCutoff(1500.0f);
        thin.trigger();
        std::vector<float> tb(static_cast<size_t>(hn), 0.0f);
        thin.render(tb.data(), hn, sampleRate);

        check(rms(tb) < rms(fb) * 0.5,
              "a high-pass thins the kick (removes most of its low-frequency energy)");
        check(rms(tb) > 0.0, "the thinned kick still produces some sound");

        // hp = 0 is a bit-for-bit no-op vs an untouched (deterministic) kick.
        audio::DrumVoice noop1;
        noop1.setType(audio::Drum::Kick);
        noop1.trigger();
        std::vector<float> nb1(static_cast<size_t>(hn), 0.0f);
        noop1.render(nb1.data(), hn, sampleRate);
        audio::DrumVoice noop2;
        noop2.setType(audio::Drum::Kick);
        noop2.setHighpassCutoff(0.0f);
        noop2.trigger();
        std::vector<float> nb2(static_cast<size_t>(hn), 0.0f);
        noop2.render(nb2.data(), hn, sampleRate);
        check(nb1 == nb2, "drum high-pass at 0 is a bit-for-bit no-op");

        audio::DrumVoice clampV;
        clampV.setHighpassCutoff(5000.0f);
        check(clampV.highpassCutoff() == 2000.0f, "drum high-pass clamps to a 2 kHz maximum");
    }

    // --- New percussion voices: shaker + clave ------------------------------
    {
        // A clave is a pure high tone → measure its dominant frequency via zero crossings.
        auto freqOf = [](const std::vector<float>& b, int srate) {
            int cross = 0;
            for (size_t i = 1; i < b.size(); ++i) {
                if ((b[i - 1] <= 0.0f && b[i] > 0.0f) || (b[i - 1] >= 0.0f && b[i] < 0.0f)) {
                    ++cross;
                }
            }
            return static_cast<double>(cross) / 2.0 /
                   (static_cast<double>(b.size()) / static_cast<double>(srate));
        };

        audio::DrumVoice clave;
        clave.setType(audio::Drum::Clave);
        clave.trigger();
        std::vector<float> cbuf(static_cast<size_t>(sampleRate) / 100, 0.0f); // 10 ms
        clave.render(cbuf.data(), static_cast<int>(cbuf.size()), sampleRate);
        check(rms(cbuf) > 0.0, "clave produces sound");
        check(freqOf(cbuf, sampleRate) > 1800.0 && freqOf(cbuf, sampleRate) < 3200.0,
              "clave rings at its ~2.5 kHz woodblock pitch");
        std::vector<float> ctail(static_cast<size_t>(sampleRate) / 4, 0.0f); // 0.25 s
        clave.render(ctail.data(), static_cast<int>(ctail.size()), sampleRate);
        check(!clave.active(), "clave decays fast to inactive");

        audio::DrumVoice shaker;
        shaker.setType(audio::Drum::Shaker);
        shaker.trigger();
        std::vector<float> sbuf(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        shaker.render(sbuf.data(), static_cast<int>(sbuf.size()), sampleRate);
        check(rms(sbuf) > 0.0, "shaker produces sound");
        std::vector<float> stail(static_cast<size_t>(sampleRate) / 2, 0.0f); // 0.5 s
        shaker.render(stail.data(), static_cast<int>(stail.size()), sampleRate);
        check(!shaker.active(), "shaker decays to inactive");

        // Tambourine: bright jingling noise, produces sound and decays.
        audio::DrumVoice tamb;
        tamb.setType(audio::Drum::Tambourine);
        tamb.trigger();
        std::vector<float> tbuf(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        tamb.render(tbuf.data(), static_cast<int>(tbuf.size()), sampleRate);
        check(rms(tbuf) > 0.0, "tambourine produces sound");
        std::vector<float> ttail(static_cast<size_t>(sampleRate), 0.0f); // 1 s
        tamb.render(ttail.data(), static_cast<int>(ttail.size()), sampleRate);
        check(!tamb.active(), "tambourine decays to inactive");

        // Conga: a tuned hand drum → a mid-range tonal pitch (~220–450 Hz over its sweep).
        audio::DrumVoice conga;
        conga.setType(audio::Drum::Conga);
        conga.trigger();
        std::vector<float> gbuf(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        conga.render(gbuf.data(), static_cast<int>(gbuf.size()), sampleRate);
        check(rms(gbuf) > 0.0, "conga produces sound");
        const double gHz = freqOf(gbuf, sampleRate);
        check(gHz > 150.0 && gHz < 550.0, "conga rings at a mid tuned-drum pitch");

        // Woodblock: a hollow wooden "tok" → a mid tonal pitch (~800 Hz), well below the clave, with
        // a fast decay.
        audio::DrumVoice wood;
        wood.setType(audio::Drum::Woodblock);
        wood.trigger();
        std::vector<float> wbuf(static_cast<size_t>(sampleRate) / 100, 0.0f); // 10 ms
        wood.render(wbuf.data(), static_cast<int>(wbuf.size()), sampleRate);
        check(rms(wbuf) > 0.0, "woodblock produces sound");
        const double wHz = freqOf(wbuf, sampleRate);
        check(wHz > 500.0 && wHz < 1600.0, "woodblock rings at its mid ~800 Hz woody pitch");
        std::vector<float> wtail(static_cast<size_t>(sampleRate) / 4, 0.0f); // 0.25 s
        wood.render(wtail.data(), static_cast<int>(wtail.size()), sampleRate);
        check(!wood.active(), "woodblock decays fast to inactive");

        // Bongo: a tight high tuned drum → a pitch above the conga's, with a fast decay.
        audio::DrumVoice bongo;
        bongo.setType(audio::Drum::Bongo);
        bongo.trigger();
        std::vector<float> bbuf(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        bongo.render(bbuf.data(), static_cast<int>(bbuf.size()), sampleRate);
        check(rms(bbuf) > 0.0, "bongo produces sound");
        const double bHz = freqOf(bbuf, sampleRate);
        check(bHz > 300.0 && bHz < 800.0, "bongo rings at its tight high tuned-drum pitch");
        std::vector<float> btail(static_cast<size_t>(sampleRate), 0.0f); // 1 s
        bongo.render(btail.data(), static_cast<int>(btail.size()), sampleRate);
        check(!bongo.active(), "bongo decays to inactive");

        // Triangle: a bright tonal metallic ring that sustains far longer than the tuned drums.
        audio::DrumVoice tri;
        tri.setType(audio::Drum::Triangle);
        tri.trigger();
        std::vector<float> tribuf(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        tri.render(tribuf.data(), static_cast<int>(tribuf.size()), sampleRate);
        check(rms(tribuf) > 0.0, "triangle produces sound");
        check(freqOf(tribuf, sampleRate) > 3000.0, "triangle rings bright (high metallic partials)");
        std::vector<float> trimid(static_cast<size_t>(sampleRate) / 4, 0.0f); // to ~0.3 s
        tri.render(trimid.data(), static_cast<int>(trimid.size()), sampleRate);
        check(tri.active(), "triangle rings on well past a short percussion hit (long tail)");

        // 808: a pure sub kick — a very low fundamental (~50 Hz) that sustains far longer than the
        // short, punchy standard kick.
        audio::DrumVoice k808;
        k808.setType(audio::Drum::Kick808);
        k808.trigger();
        std::vector<float> kbuf(static_cast<size_t>(sampleRate) / 5, 0.0f); // 200 ms
        k808.render(kbuf.data(), static_cast<int>(kbuf.size()), sampleRate);
        check(rms(kbuf) > 0.0, "808 produces sound");
        const double kHz = freqOf(kbuf, sampleRate);
        check(kHz > 30.0 && kHz < 90.0, "808 rings at a very low sub fundamental (~50 Hz)");
        std::vector<float> ktail(static_cast<size_t>(sampleRate) / 2, 0.0f); // out to ~0.7 s
        k808.render(ktail.data(), static_cast<int>(ktail.size()), sampleRate);
        check(k808.active(), "808 sustains as a long sub tail");
        // Control: the short standard kick has fully decayed to inactive by 1 s, so the 808 outlasts it.
        audio::DrumVoice shortKick;
        shortKick.setType(audio::Drum::Kick);
        shortKick.trigger();
        std::vector<float> skbuf(static_cast<size_t>(sampleRate), 0.0f); // 1 s
        shortKick.render(skbuf.data(), static_cast<int>(skbuf.size()), sampleRate);
        check(!shortKick.active(), "standard kick has fully decayed by 1 s (808 outlasts it)");

        // Zap: a laser "pew" — a very fast, very wide downward pitch sweep. The first few ms sit
        // high (well above any kick) and it drops to a low pitch by the tail.
        audio::DrumVoice zap;
        zap.setType(audio::Drum::Zap);
        zap.trigger();
        std::vector<float> zhi(static_cast<size_t>(sampleRate) / 100, 0.0f); // first 10 ms
        zap.render(zhi.data(), static_cast<int>(zhi.size()), sampleRate);
        check(rms(zhi) > 0.0, "zap produces sound");
        check(freqOf(zhi, sampleRate) > 500.0,
              "zap starts high (a fast sweep from well above any kick)");
        std::vector<float> zskip(static_cast<size_t>(sampleRate) / 10, 0.0f); // advance to ~110 ms
        zap.render(zskip.data(), static_cast<int>(zskip.size()), sampleRate);
        std::vector<float> zlo(static_cast<size_t>(sampleRate) / 10, 0.0f); // settled tail (~110-210 ms)
        zap.render(zlo.data(), static_cast<int>(zlo.size()), sampleRate);
        check(freqOf(zlo, sampleRate) < 250.0, "zap sweeps down to a low pitch by its tail");

        // Riser (reverse cymbal): a noise sweep that SWELLS UP — a later window is louder than an
        // earlier one (the opposite of every decaying voice) — then cuts off after it peaks.
        audio::DrumVoice riser;
        riser.setType(audio::Drum::Riser);
        riser.trigger();
        std::vector<float> rEarly(static_cast<size_t>(sampleRate) / 10, 0.0f); // first ~100 ms
        riser.render(rEarly.data(), static_cast<int>(rEarly.size()), sampleRate);
        std::vector<float> rSkip(static_cast<size_t>(sampleRate) / 2, 0.0f); // advance ~0.6 s
        riser.render(rSkip.data(), static_cast<int>(rSkip.size()), sampleRate);
        std::vector<float> rLate(static_cast<size_t>(sampleRate) / 20, 0.0f); // ~0.6-0.65 s (near peak)
        riser.render(rLate.data(), static_cast<int>(rLate.size()), sampleRate);
        check(rms(rEarly) > 0.0 && rms(rLate) > rms(rEarly) * 2.0,
              "riser swells up (late window far louder than the early window)");
        std::vector<float> rTail(static_cast<size_t>(sampleRate), 0.0f); // 1 s
        riser.render(rTail.data(), static_cast<int>(rTail.size()), sampleRate);
        check(!riser.active(), "riser cuts off after it peaks");

        // 808 snare: a tuned two-partial shell (~185 + ~330 Hz) plus a snappy noise tail. At snap 0
        // it is the pure tonal shell (low, smooth); raising snap swaps in the bright wire noise. Its
        // fixed tuned partials make it distinct from the acoustic Snare's single body tone.
        auto snare808 = [&](float snap, float tune) {
            audio::DrumVoice sn;
            sn.setType(audio::Drum::Snare808);
            sn.setSnap(snap);
            sn.setTune(tune);
            sn.trigger();
            std::vector<float> b(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
            sn.render(b.data(), static_cast<int>(b.size()), sampleRate);
            return b;
        };
        auto hfEnergy = [](const std::vector<float>& b) {
            double s = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i]) - static_cast<double>(b[i - 1]);
                s += d * d;
            }
            return std::sqrt(s / static_cast<double>(b.size()));
        };
        const std::vector<float> shell = snare808(0.0f, 0.0f);
        check(rms(shell) > 0.0, "808 snare produces sound");
        check(freqOf(shell, sampleRate) < 500.0,
              "808 snare shell is a low tuned tone (not noise) at snap 0");
        check(hfEnergy(snare808(1.0f, 0.0f)) > hfEnergy(shell) * 10.0,
              "808 snare snap swaps in the bright wire noise");
        check(freqOf(snare808(0.0f, 12.0f), sampleRate) > freqOf(shell, sampleRate) * 1.5,
              "808 snare tune raises the shell pitch");

        // 808 hi-hat: a dense cluster of inharmonic square oscillators — bright/metallic and tight.
        audio::DrumVoice hat808;
        hat808.setType(audio::Drum::Hat808);
        hat808.trigger();
        std::vector<float> h808(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        hat808.render(h808.data(), static_cast<int>(h808.size()), sampleRate);
        check(rms(h808) > 0.0, "808 hat produces sound");
        check(freqOf(h808, sampleRate) > 1200.0, "808 hat is bright/metallic (high zero-crossing rate)");
        std::vector<float> h808tail(static_cast<size_t>(sampleRate) / 2, 0.0f); // out to ~0.55 s
        hat808.render(h808tail.data(), static_cast<int>(h808tail.size()), sampleRate);
        check(!hat808.active(), "808 hat decays fast (a tight closed hat)");

        // 808 clap: three noise bursts ~9 ms apart + a smeared tail. The signature is a re-rise after
        // the gap between bursts — the level dips, then jumps back up on the next burst (a single-burst
        // clap just decays monotonically).
        audio::DrumVoice clap808;
        clap808.setType(audio::Drum::Clap808);
        clap808.trigger();
        std::vector<float> c8(static_cast<size_t>(sampleRate) / 10, 0.0f); // 100 ms
        clap808.render(c8.data(), static_cast<int>(c8.size()), sampleRate);
        auto win = [&](const std::vector<float>& b, double aMs, double bMs) {
            const int a = static_cast<int>(aMs * 0.001 * sampleRate);
            const int c = static_cast<int>(bMs * 0.001 * sampleRate);
            double e = 0.0;
            for (int i = a; i < c; ++i) e += static_cast<double>(b[static_cast<size_t>(i)]) * b[static_cast<size_t>(i)];
            return std::sqrt(e / (c - a));
        };
        check(rms(c8) > 0.0, "808 clap produces sound");
        check(win(c8, 8.0, 10.0) > win(c8, 6.0, 8.0) * 2.0,
              "808 clap re-rises on the next burst after the inter-burst gap (stuttered structure)");
        // A single-burst acoustic clap decays monotonically across the same span (no re-rise).
        audio::DrumVoice clapStd;
        clapStd.setType(audio::Drum::Clap);
        clapStd.trigger();
        std::vector<float> cs(static_cast<size_t>(sampleRate) / 10, 0.0f);
        clapStd.render(cs.data(), static_cast<int>(cs.size()), sampleRate);
        check(win(cs, 8.0, 10.0) < win(cs, 6.0, 8.0) * 1.2,
              "the acoustic clap decays monotonically (no burst re-rise)");

        // Snare snap: at snap 0 the snare is its tuned body tone (smooth, low HF); at snap 1 it is
        // the noisy wire crack (much brighter). Measured as first-difference (HF) energy.
        auto snareHf = [&](float snap) {
            audio::DrumVoice sn;
            sn.setType(audio::Drum::Snare);
            sn.setSnap(snap);
            sn.trigger();
            std::vector<float> b(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
            sn.render(b.data(), static_cast<int>(b.size()), sampleRate);
            double hf = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                hf += d * d;
            }
            return hf;
        };
        check(snareHf(1.0f) > snareHf(0.0f) * 2.0,
              "snare snap 1 (wires/noise) is far brighter than snap 0 (body/tone)");
        audio::DrumVoice dsn;
        check(std::fabs(dsn.snap() - 0.5f) < 1e-6f, "snare snap defaults to 0.5 (classic mix)");

        // Finger snap: a single dry noise pop that decays fast and monotonically (no clap re-rise).
        audio::DrumVoice snap;
        snap.setType(audio::Drum::Snap);
        snap.trigger();
        std::vector<float> sp(static_cast<size_t>(sampleRate) / 10, 0.0f); // 100 ms
        snap.render(sp.data(), static_cast<int>(sp.size()), sampleRate);
        check(rms(sp) > 0.0, "finger snap produces sound");
        check(win(sp, 15.0, 18.0) < win(sp, 0.0, 3.0) * 0.7,
              "finger snap decays fast and monotonically (a single burst, no clap re-rise)");
        std::vector<float> sptail(static_cast<size_t>(sampleRate) / 4, 0.0f); // 0.25 s
        snap.render(sptail.data(), static_cast<int>(sptail.size()), sampleRate);
        check(!snap.active(), "finger snap decays fast to inactive");

        // Timbale: a high metal-shell drum — tonal + metallic ring, brighter than a tom, medium decay.
        auto drumHf = [&](audio::Drum type) {
            audio::DrumVoice d;
            d.setType(type);
            d.trigger();
            std::vector<float> b(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
            d.render(b.data(), static_cast<int>(b.size()), sampleRate);
            double hf = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double diff = static_cast<double>(b[i] - b[i - 1]);
                hf += diff * diff;
            }
            return hf;
        };
        audio::DrumVoice timb;
        timb.setType(audio::Drum::Timbale);
        timb.trigger();
        std::vector<float> tb(static_cast<size_t>(sampleRate) / 20, 0.0f); // 50 ms
        timb.render(tb.data(), static_cast<int>(tb.size()), sampleRate);
        check(rms(tb) > 0.0, "timbale produces sound");
        check(drumHf(audio::Drum::Timbale) > drumHf(audio::Drum::Tom) * 2.0,
              "timbale is brighter (more metallic HF ring) than the pure-membrane tom");
        std::vector<float> tbtail(static_cast<size_t>(sampleRate), 0.0f); // 1 s
        timb.render(tbtail.data(), static_cast<int>(tbtail.size()), sampleRate);
        check(!timb.active(), "timbale decays to inactive");

        // Agogo: a bright metallic bell with a clear pitched fundamental (~780 Hz), decays cleanly.
        auto goertzelMono = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (float v : b) {
                const double s0 = static_cast<double>(v) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        audio::DrumVoice ag;
        ag.setType(audio::Drum::Agogo);
        ag.trigger();
        std::vector<float> agb(static_cast<size_t>(sampleRate) / 10, 0.0f); // 100 ms
        ag.render(agb.data(), static_cast<int>(agb.size()), sampleRate);
        check(rms(agb) > 0.0, "agogo produces sound");
        check(goertzelMono(agb, 780.0, sampleRate) > goertzelMono(agb, 400.0, sampleRate) * 3.0,
              "agogo rings at its ~780 Hz bell pitch (a clear tonal fundamental)");
        std::vector<float> agt(static_cast<size_t>(sampleRate), 0.0f); // 1 s
        ag.render(agt.data(), static_cast<int>(agt.size()), sampleRate);
        check(!ag.active(), "agogo decays to inactive");

        // Splash: a bright cymbal like the crash, but with a much shorter, explosive decay.
        auto splashHf = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                e += d * d;
            }
            return e;
        };
        const int splWin = sampleRate / 20; // 50 ms
        audio::DrumVoice splash;
        splash.setType(audio::Drum::Splash);
        splash.trigger();
        std::vector<float> splEarly(static_cast<size_t>(splWin), 0.0f);
        splash.render(splEarly.data(), splWin, sampleRate);
        check(rms(splEarly) > 0.0, "splash produces sound");

        audio::DrumVoice splKickV;
        splKickV.setType(audio::Drum::Kick);
        splKickV.trigger();
        std::vector<float> splKick(static_cast<size_t>(splWin), 0.0f);
        splKickV.render(splKick.data(), splWin, sampleRate);
        check(splashHf(splEarly) > splashHf(splKick) * 3.0,
              "splash is bright (far more HF energy than a dark kick)");

        // Skip 0.2 s, then compare a 0.3 s tail: the long crash still rings; the splash has faded.
        const int splSkipN = sampleRate / 5;        // 0.2 s
        const int splTailN = (sampleRate * 3) / 10; // 0.3 s
        audio::DrumVoice splA;
        splA.setType(audio::Drum::Splash);
        splA.trigger();
        std::vector<float> splSkipBuf(static_cast<size_t>(splSkipN), 0.0f);
        splA.render(splSkipBuf.data(), splSkipN, sampleRate);
        std::vector<float> splTailBuf(static_cast<size_t>(splTailN), 0.0f);
        splA.render(splTailBuf.data(), splTailN, sampleRate);

        audio::DrumVoice splCrashV;
        splCrashV.setType(audio::Drum::Crash);
        splCrashV.trigger();
        std::vector<float> splCrashSkip(static_cast<size_t>(splSkipN), 0.0f);
        splCrashV.render(splCrashSkip.data(), splSkipN, sampleRate);
        std::vector<float> splCrashTail(static_cast<size_t>(splTailN), 0.0f);
        splCrashV.render(splCrashTail.data(), splTailN, sampleRate);
        check(rms(splTailBuf) < rms(splCrashTail) * 0.5,
              "splash decays much faster than the long crash");

        std::vector<float> splEnd(static_cast<size_t>(sampleRate), 0.0f); // 1 s more
        splA.render(splEnd.data(), static_cast<int>(splEnd.size()), sampleRate);
        check(!splA.active(), "splash decays to inactive");

        // China: a trashy cymbal — bright, and its decay sits between the splash and the crash.
        audio::DrumVoice chinaV;
        chinaV.setType(audio::Drum::China);
        chinaV.trigger();
        std::vector<float> chinaEarly(static_cast<size_t>(splWin), 0.0f);
        chinaV.render(chinaEarly.data(), splWin, sampleRate);
        check(rms(chinaEarly) > 0.0, "china produces sound");
        check(splashHf(chinaEarly) > splashHf(splKick) * 3.0,
              "china is bright (far more HF energy than a dark kick)");

        audio::DrumVoice chinaTailV;
        chinaTailV.setType(audio::Drum::China);
        chinaTailV.trigger();
        std::vector<float> chinaSkip(static_cast<size_t>(splSkipN), 0.0f);
        chinaTailV.render(chinaSkip.data(), splSkipN, sampleRate);
        std::vector<float> chinaTail(static_cast<size_t>(splTailN), 0.0f);
        chinaTailV.render(chinaTail.data(), splTailN, sampleRate);
        check(rms(chinaTail) > rms(splTailBuf) && rms(chinaTail) < rms(splCrashTail),
              "china's decay sits between the short splash and the long crash");

        // Guiro: a scraped idiophone whose amplitude is ratcheted (gated at ~55 Hz), so its short-time
        // energy pulses hard — unlike the steady noise of a shaker. Compare the coefficient of
        // variation (std/mean) of windowed RMS: the guiro's must be far higher than the shaker's.
        auto energyCV = [&](audio::Drum type) {
            audio::DrumVoice v;
            v.setType(type);
            v.trigger();
            const int n = sampleRate / 10; // 100 ms
            std::vector<float> buf(static_cast<size_t>(n), 0.0f);
            v.render(buf.data(), n, sampleRate);
            const int ewin = 64;
            std::vector<double> e;
            for (int i = 0; i + ewin <= n; i += ewin) {
                double sum = 0.0;
                for (int k = 0; k < ewin; ++k) {
                    const double x = static_cast<double>(buf[static_cast<size_t>(i + k)]);
                    sum += x * x;
                }
                e.push_back(std::sqrt(sum / ewin));
            }
            double mean = 0.0;
            for (double x : e) {
                mean += x;
            }
            mean /= static_cast<double>(e.size());
            double var = 0.0;
            for (double x : e) {
                var += (x - mean) * (x - mean);
            }
            var /= static_cast<double>(e.size());
            return mean > 0.0 ? std::sqrt(var) / mean : 0.0;
        };
        audio::DrumVoice guiroV;
        guiroV.setType(audio::Drum::Guiro);
        guiroV.trigger();
        std::vector<float> guiroBuf(static_cast<size_t>(splWin), 0.0f);
        guiroV.render(guiroBuf.data(), splWin, sampleRate);
        check(rms(guiroBuf) > 0.0, "guiro produces sound");
        check(energyCV(audio::Drum::Guiro) > energyCV(audio::Drum::Shaker) * 2.0,
              "the guiro's amplitude ratchets (far more energy variation than a steady shaker)");
    }

    // --- Per-pattern tempo multiplier ---------------------------------------
    {
        // At the same BPM, a higher per-pattern tempo multiplier walks the step clock faster, so the
        // transport reaches a higher step after the same number of rendered samples. Default 1.0 is
        // the song tempo. (bpm 120, 4 steps/beat → 6000 samples/step at 48 kHz; render 24000 samples.)
        auto stepAfter = [&](float mul) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setPatternTempoMul(mul);
            check(std::fabs(s.patternTempoMul() - mul) < 1e-4f, "pattern tempo multiplier is settable");
            s.play();
            renderMono(s, 24000, sampleRate); // < 16 steps at both speeds, so no wrap
            return s.currentStep();
        };
        const int slow = stepAfter(1.0f);
        const int fast = stepAfter(2.0f);
        check(fast >= slow * 2 - 1 && fast > slow,
              "double the pattern tempo advances the step clock about twice as far");
        audio::Sequencer def;
        check(std::fabs(def.patternTempoMul() - 1.0f) < 1e-4f, "pattern tempo defaults to 1x (song tempo)");
    }

    // --- Extra instrument channels (unlimited-channels foundation) -----------
    {
        audio::Sequencer s;
        check(s.instrumentChannelCount() == 0, "no extra instrument channels by default");
        const int ch = s.addInstrumentChannel();
        check(ch == 0 && s.instrumentChannelCount() == 1, "addInstrumentChannel appends a channel");
        // Adding a channel gives every pattern a parallel lane.
        const int pat = s.addPattern();
        check(static_cast<int>(s.instrumentRoll(pat, ch).notes().size()) == 0,
              "a newly added pattern gets the extra channel's (empty) lane");
        // A note on the extra channel is audible in the render (it sums into the lead bus for now).
        s.selectPattern(0);
        s.instrumentSynth(ch).setWaveform(audio::Waveform::Saw);
        s.instrumentRoll(ch).addNote(audio::Note{0, 4, 60, 0.9f});
        s.play();
        check(rms(renderMono(s, sampleRate / 4, sampleRate)) > 0.0,
              "a note on an extra instrument channel produces sound");

        // Per-channel routing: an extra channel routed to the bass bus lands in the bass stem, not the
        // lead stem. Render both stems separately and check where the energy shows up.
        auto stemEnergy = [&](int busTarget) {
            audio::Sequencer r;
            const int rc = r.addInstrumentChannel();
            r.setInstrumentBus(rc, busTarget);
            r.instrumentSynth(rc).setWaveform(audio::Waveform::Saw);
            r.instrumentRoll(rc).addNote(audio::Note{0, 4, 60, 0.9f});
            r.play();
            const int fr = sampleRate / 4;
            std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
            r.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
            return std::pair<double, double>{rms(l), rms(b)};
        };
        const auto toLead = stemEnergy(1);
        const auto toBass = stemEnergy(2);
        check(toLead.first > 0.0 && toLead.second == 0.0,
              "an extra channel routed to the lead bus appears only in the lead stem");
        check(toBass.second > 0.0 && toBass.first == 0.0,
              "an extra channel routed to the bass bus appears only in the bass stem");

        // Per-channel mixer-group routing: a channel routed to a group renders into that group's own
        // buffer (for its own insert chain), not into any fixed bus stem.
        {
            audio::Sequencer r;
            const int rc = r.addInstrumentChannel();
            r.instrumentSynth(rc).setWaveform(audio::Waveform::Saw);
            r.setInstrumentGroup(rc, 0); // route to mixer group 0
            check(r.instrumentGroup(rc) == 0, "setInstrumentGroup selects a mixer group");
            r.instrumentRoll(rc).addNote(audio::Note{0, 4, 60, 0.9f});
            r.play();
            const int fr = sampleRate / 4;
            std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> grp(static_cast<size_t>(fr) * 2, 0.0f);
            float* groups[1] = {grp.data()};
            r.renderStems(d.data(), l.data(), b.data(), groups, 1, fr, sampleRate);
            check(rms(grp) > 0.0, "a group-routed channel renders into the group buffer");
            check(rms(d) == 0.0 && rms(l) == 0.0 && rms(b) == 0.0,
                  "and not into any fixed bus stem");
            // The group's own insert strip processes that audio: a MixerTrack at gain 0 silences it.
            audio::MixerTrack group;
            group.setGain(0.0f);
            group.process(grp.data(), fr, sampleRate);
            check(rms(grp) == 0.0, "the group's insert strip (gain 0) processes the routed channel");
        }

        // Bit-identical default: the group-aware overload with no groups matches the 3-stem overload.
        {
            auto renderVia = [&](bool viaGroupOverload) {
                audio::Sequencer r;
                const int rc = r.addInstrumentChannel();
                r.instrumentSynth(rc).setWaveform(audio::Waveform::Saw);
                r.instrumentRoll(rc).addNote(audio::Note{0, 4, 60, 0.9f});
                r.roll().addNote(audio::Note{0, 4, 64, 0.8f});
                r.play();
                const int fr = sampleRate / 4;
                std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
                if (viaGroupOverload) {
                    r.renderStems(d.data(), l.data(), b.data(), nullptr, 0, fr, sampleRate);
                } else {
                    r.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
                }
                std::vector<float> out = l;
                out.insert(out.end(), b.begin(), b.end());
                out.insert(out.end(), d.begin(), d.end());
                return out;
            };
            const auto viaThree = renderVia(false);
            const auto viaGroup = renderVia(true);
            double maxDiff = 0.0;
            for (size_t i = 0; i < viaThree.size(); ++i) {
                maxDiff = std::max(maxDiff, std::fabs(static_cast<double>(viaThree[i] - viaGroup[i])));
            }
            check(maxDiff < 1e-6, "renderStems with no groups matches the 3-stem overload (default path)");
        }

        // Per-note attributes now apply to extra channels: a probability-0 note never fires (silence),
        // proving extra-channel notes run through the same per-note path as the lead/bass lanes.
        auto probEnergy = [&](float prob) {
            audio::Sequencer r;
            const int rc = r.addInstrumentChannel();
            r.instrumentSynth(rc).setWaveform(audio::Waveform::Saw);
            audio::Note nn{0, 4, 60, 0.9f};
            nn.probability = prob;
            r.instrumentRoll(rc).addNote(nn);
            r.play();
            return rms(renderMono(r, sampleRate / 4, sampleRate));
        };
        check(probEnergy(0.0f) == 0.0, "a probability-0 note on an extra channel never fires");
        check(probEnergy(1.0f) > 0.0, "a probability-1 note on an extra channel always fires");
    }

    // --- 2-D playlist clips compile to the song transport --------------------
    {
        audio::Sequencer s;
        const int pB = s.addPattern(); // pattern index 1
        const int pC = s.addPattern(); // pattern index 2
        // Place clips out of bar order across tracks; compile should sort by bar, then track.
        s.addClip(pC, 4, 0); // pattern 2 at bar 4
        s.addClip(0, 0, 1);  // pattern 0 at bar 0, track 1
        s.addClip(pB, 0, 0); // pattern 1 at bar 0, track 0
        const int len = s.compileClipsToPlaylist();
        check(len == 3 && s.playlist().size() == 3, "compile fills the playlist from all clips");
        check(s.playlist()[0] == pB && s.playlist()[1] == 0 && s.playlist()[2] == pC,
              "clips compile in bar order, then track order");
    }

    // --- Multi-bar clips: a clip spans several bars, tiling its pattern ------
    {
        audio::Sequencer s;
        const int pB = s.addPattern();  // pattern index 1
        s.addClip(pB, 0, 0, 2);         // pattern 1 at bar 0, spanning 2 bars
        s.addClip(0, 3, 0, 1);          // pattern 0 at bar 3, one bar (a gap at bar 2)
        check(s.clipBarCount() == 4, "clipBarCount spans multi-bar clips (bars 0-1 + bar 3 → 4)");
        check(s.primaryClipPattern(0) == pB && s.primaryClipPattern(1) == pB,
              "a 2-bar clip is active on both of its bars");
        check(s.primaryClipPattern(2) == -1, "the gap bar has no active clip");
        check(s.primaryClipPattern(3) == 0, "the later single-bar clip is active on its bar");
        const int len = s.compileClipsToPlaylist();
        check(len == 3 && s.playlist()[0] == pB && s.playlist()[1] == pB && s.playlist()[2] == 0,
              "a multi-bar clip tiles its pattern across bars when compiled");
    }

    // --- Per-clip mute: a muted clip is skipped in playback and compile -----
    {
        // Two clips share bar 0; muting one drops its voice from the simultaneous playback.
        auto voicesWithMute = [](bool muteSecond) {
            audio::Sequencer s;
            s.roll().addNote(audio::Note{0, 4, 60, 0.9f}); // pattern 0
            const int pB = s.addPattern();
            s.selectPattern(pB);
            s.roll().addNote(audio::Note{0, 4, 67, 0.9f}); // pattern 1
            s.selectPattern(0);
            s.addClip(0, 0, 0);
            const int c2 = s.addClip(pB, 0, 1);
            if (muteSecond) {
                s.clip(c2).muted = true;
            }
            s.setSongMode(true);
            s.setSongUsesClips(true);
            s.play();
            return s.synth().activeVoices();
        };
        check(voicesWithMute(false) == 2, "both clips play when neither is muted");
        check(voicesWithMute(true) == 1, "a muted clip is skipped in clip-song playback");

        // Compile skips muted clips too.
        audio::Sequencer s;
        const int pB = s.addPattern();
        s.addClip(0, 0, 0);
        const int cm = s.addClip(pB, 1, 0);
        s.clip(cm).muted = true;
        check(s.compileClipsToPlaylist() == 1 && s.playlist().size() == 1 && s.playlist()[0] == 0,
              "compile skips muted clips");
    }

    // --- Clip-song mode: true simultaneous multi-track clip playback ---------
    {
        // Two patterns, each with a lead note at step 0 (distinct pitches). Placing both as clips on
        // bar 0 (different tracks) in clip-song mode must play BOTH at once → two voices on the shared
        // lead synth. Without clip mode, only the current pattern plays → one voice. A fresh sequencer
        // per case so no voices from a previous play() linger.
        auto voicesFor = [](bool clipMode) {
            audio::Sequencer s;
            s.roll().addNote(audio::Note{0, 4, 60, 0.9f}); // pattern 0 lead note
            const int pB = s.addPattern();
            s.selectPattern(pB);
            s.roll().addNote(audio::Note{0, 4, 67, 0.9f}); // pattern 1 lead note
            s.selectPattern(0);
            s.addClip(0, 0, 0);  // pattern 0 @ bar 0, track 0
            s.addClip(pB, 0, 1); // pattern 1 @ bar 0, track 1
            s.setSongMode(true);
            s.setSongUsesClips(clipMode);
            s.play();
            return s.synth().activeVoices();
        };
        check(voicesFor(false) == 1, "without clip mode only one pattern plays on the bar");
        check(voicesFor(true) == 2,
              "clip-song mode plays both clips on the same bar simultaneously (multi-track)");
    }

    // --- Audio clips on the 2-D playlist -------------------------------------
    {
        // A short procedural sample injected into an audio clip plays back (one-shot) when the
        // clip-song transport reaches the clip's bar. Reuses the Sampler, so no WAV file is needed.
        auto makeSample = []() {
            std::vector<float> buf(4800, 0.0f); // 0.1 s @ 48k
            for (size_t i = 0; i < buf.size(); ++i) {
                buf[i] = 0.5f * std::sin(2.0f * 3.14159265f * 220.0f *
                                         static_cast<float>(i) / 48000.0f);
            }
            return buf;
        };

        // A clip at bar 0 sounds in the lead stem immediately after play().
        {
            audio::Sequencer s;
            const int c = s.addAudioClip(0, 0); // bar 0, track 0
            check(c == 0 && s.audioClipCount() == 1, "addAudioClip appends an audio clip");
            s.audioClipSampler(c).setSampleMono(makeSample(), 48000);
            s.setSongMode(true);
            s.setSongUsesClips(true);
            s.play();
            const int fr = sampleRate / 8; // well under a bar → transport stays on bar 0
            std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
            s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
            check(rms(l) > 0.0, "an audio clip at bar 0 sounds in the lead stem in clip-song mode");
            check(rms(b) == 0.0, "and not in an unrelated (bass) stem");
        }

        // A clip placed at a LATER bar is silent in the first bar (not yet triggered), and the
        // clip-song length spans far enough to reach it.
        {
            audio::Sequencer s;
            const int c = s.addAudioClip(2, 0); // bar 2
            s.audioClipSampler(c).setSampleMono(makeSample(), 48000);
            s.setSongMode(true);
            s.setSongUsesClips(true);
            s.play();
            const int fr = sampleRate / 8;
            std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
            s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
            check(rms(l) == 0.0, "an audio clip at a later bar is silent in the first bar");
            check(s.clipBarCount() >= 3, "clipBarCount covers an audio clip at bar 2");
        }

        // Per-clip target bus: a clip routed to the bass bus lands in the bass stem, not the lead.
        {
            audio::Sequencer s;
            const int c = s.addAudioClip(0, 0);
            s.setAudioClipBus(c, 2); // 0=drums, 1=lead, 2=bass
            check(s.audioClip(c).bus == 2, "setAudioClipBus routes the clip");
            s.audioClipSampler(c).setSampleMono(makeSample(), 48000);
            s.setSongMode(true);
            s.setSongUsesClips(true);
            s.play();
            const int fr = sampleRate / 8;
            std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
            s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
            check(rms(b) > 0.0 && rms(l) == 0.0,
                  "an audio clip routed to the bass bus lands in the bass stem");
        }

        // Per-clip pitch: a clip pitched up an octave (+12) resamples at 2× rate, so its one-shot voice
        // finishes in half the frames. After rendering ~0.075 s, a natural clip is still sounding but the
        // pitched-up clip has already ended.
        {
            auto voicesAfter = [&](int semis) {
                audio::Sequencer s;
                const int c = s.addAudioClip(0, 0);
                s.setAudioClipPitch(c, semis);
                s.audioClipSampler(c).setSampleMono(makeSample(), 48000); // 0.1 s sample
                s.setSongMode(true);
                s.setSongUsesClips(true);
                s.play();
                const int fr = 3600; // 0.075 s @ 48k — between the +12 end (~0.05 s) and natural end (0.1 s)
                std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
                s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
                return s.audioClipSampler(c).activeVoices();
            };
            check(voicesAfter(0) == 1, "a natural-pitch audio clip is still sounding at 0.075 s");
            check(voicesAfter(12) == 0,
                  "a +12-semitone audio clip resamples at 2x rate and finishes sooner");
        }

        // Per-clip mute: a muted audio clip is skipped in playback (no voice, silent stem), mirroring
        // pattern-clip mute.
        {
            auto leadRms = [&](bool mute) {
                audio::Sequencer s;
                const int c = s.addAudioClip(0, 0);
                s.audioClip(c).muted = mute;
                s.audioClipSampler(c).setSampleMono(makeSample(), 48000);
                s.setSongMode(true);
                s.setSongUsesClips(true);
                s.play();
                const int fr = sampleRate / 8;
                std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
                s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
                return rms(l);
            };
            check(leadRms(false) > 0.0, "an unmuted audio clip sounds");
            check(leadRms(true) == 0.0, "a muted audio clip is silent");
        }

        // Per-clip reverse: a ramp sample (rising 0→1) plays back with rising energy forward, but a
        // reversed clip reads it from the end so its energy falls — the first half is louder than the
        // second. Proves the sample is actually played backward.
        {
            auto halfRms = [&](bool rev) {
                std::vector<float> ramp(4800, 0.0f);
                for (size_t i = 0; i < ramp.size(); ++i) {
                    ramp[i] = static_cast<float>(i) / static_cast<float>(ramp.size()); // 0 → ~1
                }
                audio::Sequencer s;
                const int c = s.addAudioClip(0, 0);
                s.audioClip(c).reverse = rev;
                s.audioClipSampler(c).setSampleMono(ramp, 48000);
                s.setSongMode(true);
                s.setSongUsesClips(true);
                s.play();
                const int fr = 4800; // the whole sample at natural rate
                std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
                std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
                s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
                std::vector<float> firstHalf(l.begin(), l.begin() + static_cast<long>(l.size()) / 2);
                std::vector<float> secondHalf(l.begin() + static_cast<long>(l.size()) / 2, l.end());
                return std::pair<double, double>{rms(firstHalf), rms(secondHalf)};
            };
            const auto fwd = halfRms(false);
            const auto rev = halfRms(true);
            check(fwd.second > fwd.first, "a forward audio clip rises in energy (ramp 0→1)");
            check(rev.first > rev.second, "a reversed audio clip plays the sample backward (energy falls)");
        }

        // removeAudioClip / clearAudioClips manage the collection.
        {
            audio::Sequencer s;
            s.addAudioClip(0, 0);
            s.addAudioClip(1, 0);
            check(s.audioClipCount() == 2, "two audio clips added");
            s.removeAudioClip(0);
            check(s.audioClipCount() == 1 && s.audioClip(0).startBar == 1,
                  "removeAudioClip drops the indexed clip");
            s.clearAudioClips();
            check(s.audioClipCount() == 0, "clearAudioClips empties the collection");
        }
    }

    // --- Arrangement-track mute / solo (2-D playlist rows) -------------------
    {
        // Two audio clips on the same bar, different track rows. Muting a row silences its clip; soloing
        // a row silences every other row. Distinct from per-clip mute (this gates a whole timeline row).
        auto makeSample = []() {
            std::vector<float> buf(4800, 0.0f);
            for (size_t i = 0; i < buf.size(); ++i) {
                buf[i] = 0.5f * std::sin(2.0f * 3.14159265f * 220.0f *
                                         static_cast<float>(i) / 48000.0f);
            }
            return buf;
        };
        // Render helper: two clips (track 0 → lead bus, track 1 → bass bus). Returns {leadRms, bassRms}.
        auto render = [&](int muteTrack, int soloTrack) {
            audio::Sequencer s;
            const int c0 = s.addAudioClip(0, 0); // track 0
            s.setAudioClipBus(c0, 1);            // lead
            s.audioClipSampler(c0).setSampleMono(makeSample(), 48000);
            const int c1 = s.addAudioClip(0, 1); // track 1
            s.setAudioClipBus(c1, 2);            // bass
            s.audioClipSampler(c1).setSampleMono(makeSample(), 48000);
            if (muteTrack >= 0) {
                s.setTrackMuted(muteTrack, true);
            }
            if (soloTrack >= 0) {
                s.setTrackSoloed(soloTrack, true);
            }
            s.setSongMode(true);
            s.setSongUsesClips(true);
            s.play();
            const int fr = sampleRate / 8;
            std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
            std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
            s.renderStems(d.data(), l.data(), b.data(), fr, sampleRate);
            return std::pair<double, double>{rms(l), rms(b)};
        };
        const auto none = render(-1, -1);
        check(none.first > 0.0 && none.second > 0.0, "both track rows sound when nothing is muted/soloed");
        const auto muteT0 = render(0, -1);
        check(muteT0.first == 0.0 && muteT0.second > 0.0,
              "muting track 0 silences its clip while track 1 still plays");
        const auto soloT1 = render(-1, 1);
        check(soloT1.first == 0.0 && soloT1.second > 0.0,
              "soloing track 1 silences every other row");
        // trackAudible reflects the flags directly.
        audio::Sequencer f;
        check(f.trackAudible(0) && !f.anyTrackSoloed(), "a fresh track is audible with no solo active");
        f.setTrackSoloed(2, true);
        check(f.anyTrackSoloed() && f.trackAudible(2) && !f.trackAudible(0),
              "with a solo active only the soloed row is audible");
        f.setTrackMuted(2, true);
        check(!f.trackAudible(2), "a muted row is inaudible even when soloed");
    }

    // --- Clip-song loop region (bar range) -----------------------------------
    {
        // Four one-bar clips (bars 0..3) → clipBarCount 4. A loop region [1,3) must start playback at
        // bar 1 and cycle 1→2→1→2, never reaching bar 0 or bar 3.
        audio::Sequencer s;
        s.addClip(0, 0, 0);
        s.addClip(0, 1, 0);
        s.addClip(0, 2, 0);
        s.addClip(0, 3, 0);
        check(s.clipBarCount() == 4, "four one-bar clips span four bars");
        s.setSongMode(true);
        s.setSongUsesClips(true);
        s.setSongLoop(true);
        s.setClipLoopRange(1, 3); // loop bars 1..2 (end exclusive)
        s.play();
        check(s.songBar() == 1, "clip-song play starts at the loop-region start bar");
        const int barFrames = 96000; // 16 steps * 6000 frames/step @120 BPM, 48k
        bool saw0 = false, saw1 = false, saw2 = false, saw3 = false;
        auto noteBar = [&](int b) {
            saw0 = saw0 || b == 0;
            saw1 = saw1 || b == 1;
            saw2 = saw2 || b == 2;
            saw3 = saw3 || b == 3;
        };
        noteBar(s.songBar());
        for (int i = 0; i < 6; ++i) { // advance ~6 bars
            std::vector<float> buf(static_cast<size_t>(barFrames) * 2, 0.0f);
            s.render(buf.data(), barFrames, sampleRate);
            noteBar(s.songBar());
        }
        check(saw1 && saw2 && !saw0 && !saw3,
              "clip-song transport loops only within [1,3), never reaching bar 0 or 3");

        // With no region (end <= start) the transport uses the whole timeline and reaches bar 0 again.
        audio::Sequencer w;
        w.addClip(0, 0, 0);
        w.addClip(0, 1, 0);
        w.setSongMode(true);
        w.setSongUsesClips(true);
        w.setSongLoop(true);
        w.play();
        check(w.songBar() == 0, "with no clip loop region playback starts at bar 0");
    }

    // --- songPositionBars: fractional, monotonic, integer at bar boundaries --
    {
        audio::Sequencer s;
        s.addClip(0, 0, 0);
        s.addClip(0, 1, 0);
        s.addClip(0, 2, 0); // three bars
        s.setSongMode(true);
        s.setSongUsesClips(true);
        s.setSongLoop(true);
        s.play();
        check(std::fabs(s.songPositionBars()) < 1e-6, "songPositionBars starts at 0");
        const int barFrames = 96000; // one bar @120 BPM, 48k
        // Render most of a bar (not the whole bar) → position advances within bar 0, still < 1.
        std::vector<float> buf(static_cast<size_t>(barFrames) * 2, 0.0f);
        s.render(buf.data(), barFrames / 2, sampleRate);
        const double mid = s.songPositionBars();
        check(mid > 0.4 && mid < 0.6, "songPositionBars reads ~0.5 halfway through the first bar");
        // Finish the bar → lands on integer bar 1.
        s.render(buf.data(), barFrames / 2, sampleRate);
        check(std::fabs(s.songPositionBars() - 1.0) < 0.02,
              "songPositionBars hits ~1.0 at the first bar boundary");
    }

    // --- Master tuning -------------------------------------------------------
    {
        audio::Sequencer s;
        check(s.masterTune() == 0.0f, "master tune defaults to 0 (A440)");
        s.setMasterTune(-31.8f);
        check(std::fabs(s.masterTune() - (-31.8f)) < 1e-4f, "master tune stores the concert-pitch offset");
        s.setMasterTune(500.0f);
        check(s.masterTune() == 100.0f, "master tune clamps to +100 cents");
        s.setMasterTune(-500.0f);
        check(s.masterTune() == -100.0f, "master tune clamps to -100 cents");
    }

    // --- Arrangement markers -------------------------------------------------
    {
        audio::Sequencer s;
        check(s.markerCount() == 0, "no markers by default");
        const int m0 = s.addMarker(0, "Intro");
        const int m1 = s.addMarker(4, "Verse 1"); // a name with a space
        s.addMarker(12, "Drop");
        check(m0 == 0 && m1 == 1 && s.markerCount() == 3, "addMarker appends markers");
        check(s.marker(1).bar == 4 && s.marker(1).name == "Verse 1", "marker stores bar + spaced name");
        s.removeMarker(0);
        check(s.markerCount() == 2 && s.marker(0).name == "Verse 1", "removeMarker drops the indexed marker");
        s.clearMarkers();
        check(s.markerCount() == 0, "clearMarkers empties the list");
        // A negative bar clamps to 0.
        s.addMarker(-3, "Neg");
        check(s.marker(0).bar == 0, "a negative marker bar clamps to 0");
    }

    // --- Sequencer grid ------------------------------------------------------
    audio::Sequencer seq;
    check(seq.numSteps() == 16, "default pattern is 16 steps");
    check(seq.numChannels() >= 4, "kit has at least four channels");

    seq.setStep(0, 0, true);
    check(seq.step(0, 0), "setStep switches a step on");
    seq.toggle(0, 0);
    check(!seq.step(0, 0), "toggle switches it back off");
    seq.setStep(0, 3, true);
    seq.clear();
    check(!seq.step(0, 3), "clear switches every step off");

    // Per-step velocity: a soft accent step is quieter than a full-velocity step.
    {
        audio::Sequencer full;
        full.setBpm(120.0);
        full.setStep(0, 0, true); // velocity 1.0
        full.play();
        const double loud = rms(renderMono(full, 6000, sampleRate));

        audio::Sequencer soft;
        soft.setBpm(120.0);
        soft.setStepVelocity(0, 0, 0.3f);
        soft.play();
        const double softRms = rms(renderMono(soft, 6000, sampleRate));
        check(softRms > 0.0 && softRms < loud, "a lower step velocity plays a softer hit");
        check(soft.stepVelocity(0, 0) > 0.25f && soft.stepVelocity(0, 0) < 0.35f,
              "step velocity round-trips through the grid");
    }
    // Out-of-range access is safe and reads false.
    check(!seq.step(-1, 0) && !seq.step(0, 999), "out-of-range steps read false");

    // --- Transport timing (120 BPM, 16ths @ 48 kHz → exactly 6000 samples/step) -------------
    seq.setBpm(120.0);
    seq.play();
    check(seq.playing(), "play() starts the transport");
    check(seq.currentStep() == 0, "transport starts on step 0");

    (void)renderMono(seq, 6000, sampleRate); // one step
    check(seq.currentStep() == 1, "one step of frames advances to step 1");

    for (int i = 0; i < 15; ++i) {
        (void)renderMono(seq, 6000, sampleRate);
    }
    check(seq.currentStep() == 0, "playhead wraps back to step 0 after a full bar");

    // Block size must not change the timing: 6000 frames split as 1000×6 still advances one step.
    audio::Sequencer seq2;
    seq2.setBpm(120.0);
    seq2.play();
    for (int i = 0; i < 6; ++i) {
        (void)renderMono(seq2, 1000, sampleRate);
    }
    check(seq2.currentStep() == 1, "stepping is block-size independent");

    seq2.stop();
    check(!seq2.playing(), "stop() freezes the transport");

    // --- Sound vs silence ----------------------------------------------------
    audio::Sequencer beat;
    beat.setBpm(120.0);
    beat.setStep(0, 0, true); // kick on the downbeat
    beat.play();
    const std::vector<float> firstStep = renderMono(beat, 6000, sampleRate);
    check(rms(firstStep) > 0.0, "a step with a kick produces sound");

    audio::Sequencer silent;
    silent.play(); // empty pattern
    const std::vector<float> nothing = renderMono(silent, 6000, sampleRate);
    check(rms(nothing) == 0.0, "an empty pattern renders silence");

    // --- Swing / groove ------------------------------------------------------
    {
        // At 120 BPM @ 48 kHz a straight step is 6000 samples. Swing 0.5 makes step 0 last
        // 9000 (1.5×) and step 1 last 3000 (0.5×), pushing the off-beat later while the pair still
        // sums to 12000.
        audio::Sequencer sw;
        sw.setBpm(120.0);
        sw.setSwing(0.5f);
        sw.play();
        (void)renderMono(sw, 6000, sampleRate);
        check(sw.currentStep() == 0, "swing lengthens the on-beat step (still step 0 at 6000)");
        (void)renderMono(sw, 3000, sampleRate); // total 9000
        check(sw.currentStep() == 1, "off-beat starts late (step 1 at 9000)");
        (void)renderMono(sw, 3000, sampleRate); // total 12000 → the short off-beat completed
        check(sw.currentStep() == 2, "the off-beat step is short (step 2 by 12000)");

        // Straight (swing 0) advances every 6000 samples.
        audio::Sequencer straight;
        straight.setBpm(120.0);
        straight.play();
        (void)renderMono(straight, 6000, sampleRate);
        check(straight.currentStep() == 1, "straight timing advances every 6000 samples");

        // Swing is per-pattern: each pattern carries its own groove independently.
        audio::Sequencer pp;
        pp.setSwing(0.6f); // pattern 0
        const int p1 = pp.addPattern();
        pp.selectPattern(p1);
        check(pp.swing() == 0.0f, "a new pattern starts straight (independent of pattern 0)");
        pp.setSwing(0.3f); // pattern 1
        pp.selectPattern(0);
        check(std::fabs(pp.swing() - 0.6f) < 1e-6f, "pattern 0 keeps its own swing");
        pp.selectPattern(p1);
        check(std::fabs(pp.swing() - 0.3f) < 1e-6f, "pattern 1 keeps its own swing");
    }

    // --- Per-step micro-timing nudge ----------------------------------------
    {
        // Index of the first frame whose level crosses a threshold (a hit's onset), or -1.
        auto onset = [](const std::vector<float>& buf) {
            for (size_t i = 0; i + 1 < buf.size(); i += 2) {
                if (std::fabs(buf[i]) > 0.05f || std::fabs(buf[i + 1]) > 0.05f) {
                    return static_cast<int>(i / 2);
                }
            }
            return -1;
        };

        // Default: a step is on the grid — its kick fires essentially at step 0's boundary (t≈0).
        audio::Sequencer straight;
        straight.setBpm(120.0); // 48 kHz → a 16th step is 6000 frames
        straight.setStep(0, 0, true);
        check(straight.stepNudge(0, 0) == 0, "a step starts with no timing nudge");
        straight.play();
        const int t0 = onset(renderMono(straight, 8000, sampleRate));
        check(t0 >= 0 && t0 < 600, "an un-nudged step fires on the grid (near t=0)");

        // Nudge 50%: the same hit is pushed to ~half a step later (~3000 frames).
        audio::Sequencer late;
        late.setBpm(120.0);
        late.setStep(0, 0, true);
        late.setStepNudge(0, 0, 50);
        check(late.stepNudge(0, 0) == 50, "setStepNudge stores the nudge amount");
        late.play();
        const int t1 = onset(renderMono(late, 8000, sampleRate));
        check(t1 > 2200 && t1 < 3800, "a 50% nudge delays the hit to ~half a step later");
        check(t1 > t0 + 2000, "the nudged hit lands clearly later than the on-grid hit");

        // The nudge is clamped into [0, 95] and defaults per pattern.
        audio::Sequencer clamp;
        clamp.setStep(0, 0, true);
        clamp.setStepNudge(0, 0, 200);
        check(clamp.stepNudge(0, 0) == 95, "nudge is clamped to 95%");
        clamp.setStepNudge(0, 0, -30);
        check(clamp.stepNudge(0, 0) == 0, "negative nudge clamps to on-grid");
    }

    // --- Per-step pitch (channel-rack graph editor) --------------------------
    {
        // A tuned tom (a clear pitched membrane): +12 semitones on a step doubles its fundamental.
        auto tomCrossings = [&](int tune) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setChannelType(0, audio::Drum::Tom);
            s.setStep(0, 0, true);
            s.setStepTune(0, 0, tune);
            s.play();
            const std::vector<float> out = renderMono(s, 4000, sampleRate); // ~83 ms of the hit
            int cx = 0;
            float prev = 0.0f;
            for (int i = 0; i < 4000; ++i) {
                const float v = out[static_cast<size_t>(i) * 2];
                if (prev <= 0.0f && v > 0.0f) {
                    ++cx;
                }
                prev = v;
            }
            return cx;
        };
        check(tomCrossings(12) > tomCrossings(0) * 1.5,
              "per-step pitch tunes an individual hit up (higher fundamental)");
        audio::Sequencer sd;
        check(sd.stepTune(0, 0) == 0, "per-step pitch defaults to 0 (channel tuning)");
        sd.setStepTune(0, 0, 100); // clamps to ±24
        check(sd.stepTune(0, 0) == 24, "per-step pitch clamps to +24 semitones");
    }

    // --- Per-channel mixer: volume / mute / solo ----------------------------
    {
        audio::Sequencer mix;
        mix.setBpm(120.0);
        mix.setStep(0, 0, true); // kick
        mix.setStep(1, 0, true); // snare, same step
        mix.play();
        const double full = rms(renderMono(mix, 6000, sampleRate));
        check(full > 0.0, "two channels produce sound");

        // Mute the kick → quieter than both.
        audio::Sequencer m2;
        m2.setBpm(120.0);
        m2.setStep(0, 0, true);
        m2.setStep(1, 0, true);
        m2.setChannelMute(0, true);
        m2.play();
        const double muted = rms(renderMono(m2, 6000, sampleRate));
        check(muted > 0.0 && muted < full, "muting a channel reduces the mix");

        // Mute both → silence.
        m2.setChannelMute(1, true);
        m2.play();
        check(rms(renderMono(m2, 6000, sampleRate)) == 0.0, "muting all channels is silent");

        // Solo the snare (kick also active) → only the snare sounds.
        audio::Sequencer s2;
        s2.setBpm(120.0);
        s2.setStep(0, 0, true);
        s2.setStep(1, 0, true);
        s2.setChannelSolo(1, true);
        s2.play();
        const double soloed = rms(renderMono(s2, 6000, sampleRate));
        check(soloed > 0.0 && soloed < full, "solo isolates one channel");

        // Volume 0 on the only active channel → silence.
        audio::Sequencer v2;
        v2.setBpm(120.0);
        v2.setStep(0, 0, true);
        v2.setChannelVolume(0, 0.0f);
        v2.play();
        check(rms(renderMono(v2, 6000, sampleRate)) == 0.0, "channel volume 0 is silent");

        // Pan: a hard-left channel is much louder on the left than the right.
        audio::Sequencer pan;
        pan.setBpm(120.0);
        pan.setStep(0, 0, true);
        pan.setChannelPan(0, -1.0f);
        pan.play();
        const std::vector<float> panned = renderMono(pan, 6000, sampleRate);
        check(rmsChannel(panned, 0) > rmsChannel(panned, 1) * 4.0, "hard-left pan favors the left");

        // Center pan is balanced.
        audio::Sequencer cen;
        cen.setBpm(120.0);
        cen.setStep(0, 0, true);
        cen.play();
        const std::vector<float> centered = renderMono(cen, 6000, sampleRate);
        check(std::fabs(rmsChannel(centered, 0) - rmsChannel(centered, 1)) < 1e-4,
              "center pan is balanced L/R");
    }

    // --- Humanize ------------------------------------------------------------
    {
        // Four identical kicks on the beats: without humanize their per-beat peaks are equal; with
        // humanize they vary. Deterministic, so the same render is reproducible.
        auto beatPeaks = [&](float amount) {
            audio::Sequencer h;
            h.setBpm(120.0);
            for (int s : {0, 4, 8, 12}) {
                h.setStep(0, s, true);
            }
            h.setHumanize(amount);
            h.play();
            const std::vector<float> out = renderMono(h, 16 * 6000, sampleRate);
            std::vector<float> peaks;
            for (int b = 0; b < 4; ++b) {
                float p = 0.0f;
                for (int i = b * 4 * 6000; i < b * 4 * 6000 + 6000; ++i) {
                    p = std::max(p, std::fabs(out[static_cast<size_t>(i) * 2]));
                }
                peaks.push_back(p);
            }
            return peaks;
        };
        const std::vector<float> flat = beatPeaks(0.0f);
        check(std::fabs(flat[0] - flat[1]) < 1e-4f && std::fabs(flat[0] - flat[3]) < 1e-4f,
              "without humanize, identical hits are equal");
        const std::vector<float> human = beatPeaks(0.9f);
        const bool varies = std::fabs(human[0] - human[1]) > 1e-3f ||
                            std::fabs(human[1] - human[2]) > 1e-3f ||
                            std::fabs(human[2] - human[3]) > 1e-3f;
        check(varies, "humanize makes identical hits vary in level");
    }

    // --- Arpeggiator ---------------------------------------------------------
    {
        // A held C-major triad (C E G, MIDI 60/64/67) across the bar, arp mode "up", should cycle
        // 60 → 64 → 67 → 60 … one pitch per step.
        audio::Sequencer arp;
        arp.setBpm(120.0);
        arp.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        arp.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        arp.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        arp.setArp(true, 0); // up
        arp.play();          // strikes step 0
        check(arp.arpCurrentPitch() == 60, "arp step 0 plays the lowest held note");
        (void)renderMono(arp, 6000, sampleRate);
        check(arp.arpCurrentPitch() == 64, "arp advances up to the 2nd note");
        (void)renderMono(arp, 6000, sampleRate);
        check(arp.arpCurrentPitch() == 67, "arp advances up to the 3rd note");
        (void)renderMono(arp, 6000, sampleRate);
        check(arp.arpCurrentPitch() == 60, "arp wraps back to the lowest note");

        // Down mode reverses the order from the top.
        audio::Sequencer down;
        down.setBpm(120.0);
        down.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        down.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        down.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        down.setArp(true, 1);
        down.play();
        check(down.arpCurrentPitch() == 67, "arp-down starts from the highest note");

        // Down-Up mode (6): high → low → back up (the mirror of up-down).
        audio::Sequencer du;
        du.setBpm(120.0);
        du.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        du.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        du.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        du.setArp(true, 6);
        du.play();
        check(du.arpCurrentPitch() == 67, "down-up arp starts from the highest note");
        (void)renderMono(du, 6000, sampleRate);
        check(du.arpCurrentPitch() == 64, "down-up arp descends to the middle note");
        (void)renderMono(du, 6000, sampleRate);
        check(du.arpCurrentPitch() == 60, "down-up arp reaches the lowest note");
        (void)renderMono(du, 6000, sampleRate);
        check(du.arpCurrentPitch() == 64, "down-up arp turns back up to the middle");
        (void)renderMono(du, 6000, sampleRate);
        check(du.arpCurrentPitch() == 67, "down-up arp returns to the top");

        // Octave range: one held C4 with a 2-octave arp cycles C4 → C5 → C4.
        audio::Sequencer oct;
        oct.setBpm(120.0);
        oct.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        oct.setArp(true, 0);
        oct.setArpOctaves(2);
        check(oct.arpOctaves() == 2, "arp octave range is settable");
        oct.play();
        check(oct.arpCurrentPitch() == 60, "octave arp starts at the root");
        (void)renderMono(oct, 6000, sampleRate);
        check(oct.arpCurrentPitch() == 72, "octave arp climbs an octave (C4 → C5)");
        (void)renderMono(oct, 6000, sampleRate);
        check(oct.arpCurrentPitch() == 60, "octave arp wraps back to the root");

        audio::Sequencer def;
        check(def.arpOctaves() == 1, "arp octave range defaults to 1");

        // As-played (mode 4): notes are arpeggiated in the order they were entered, not by pitch.
        audio::Sequencer asp;
        asp.setBpm(120.0);
        asp.roll().addNote(audio::Note{0, 16, 67, 1.0f}); // entry order 67 → 60 → 64
        asp.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        asp.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        asp.setArp(true, 4);
        asp.play();
        check(asp.arpCurrentPitch() == 67, "as-played arp starts with the first-entered note");
        (void)renderMono(asp, 6000, sampleRate);
        check(asp.arpCurrentPitch() == 60, "as-played arp follows entry order (2nd note)");
        (void)renderMono(asp, 6000, sampleRate);
        check(asp.arpCurrentPitch() == 64, "as-played arp follows entry order (3rd note)");

        // Random (mode 3): deterministic, only plays held pitches, and differs from the strict up walk.
        auto arpSeq = [&](int mode) {
            audio::Sequencer r;
            r.setBpm(120.0);
            r.roll().addNote(audio::Note{0, 16, 60, 1.0f});
            r.roll().addNote(audio::Note{0, 16, 64, 1.0f});
            r.roll().addNote(audio::Note{0, 16, 67, 1.0f});
            r.setArp(true, mode);
            r.play();
            std::vector<int> ps;
            ps.push_back(r.arpCurrentPitch());
            for (int k = 0; k < 12; ++k) {
                (void)renderMono(r, 6000, sampleRate);
                ps.push_back(r.arpCurrentPitch());
            }
            return ps;
        };
        const std::vector<int> rnd1 = arpSeq(3);
        const std::vector<int> rnd2 = arpSeq(3);
        const std::vector<int> up = arpSeq(0);
        check(rnd1 == rnd2, "random arp is deterministic (same each transport)");
        bool allHeld = true, twoDistinct = false;
        for (int p : rnd1) {
            if (p != 60 && p != 64 && p != 67) allHeld = false;
            if (p != rnd1[0]) twoDistinct = true;
        }
        check(allHeld, "random arp only plays held pitches");
        check(twoDistinct, "random arp visits more than one pitch");
        check(rnd1 != up, "random arp differs from the strict up order");

        // Gate: a 0.5 gate releases the arp note halfway through the step, so the second half is
        // near-silent (staccato); a full gate (1.0) sustains the note through the whole step.
        auto halfEnergy = [&](float gate, bool secondHalf) {
            audio::Sequencer g;
            g.setBpm(120.0); // 6000 frames/step @ 48 kHz
            g.roll().addNote(audio::Note{0, 16, 60, 1.0f});
            g.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.005f); // fast release for a clean staccato
            g.setArp(true, 0);
            g.setArpGate(gate);
            g.play();
            const std::vector<float> out = renderMono(g, 6000, sampleRate);
            // The gate releases at frame 3000 (0.5·step); measure well after that (release ≈ 240 fr).
            const int a = secondHalf ? 3600 : 200;
            const int b = secondHalf ? 5900 : 2800;
            double s = 0.0;
            for (int i = a; i < b; ++i) {
                const double v = out[static_cast<size_t>(i) * 2];
                s += v * v;
            }
            return std::sqrt(s / (b - a));
        };
        const double gatedFirst = halfEnergy(0.5f, false);
        const double gatedSecond = halfEnergy(0.5f, true);
        const double legatoSecond = halfEnergy(1.0f, true);
        check(gatedFirst > 0.0, "gated arp sounds in the first half of the step");
        check(gatedSecond < gatedFirst * 0.25,
              "a 0.5 arp gate silences the note in the second half (staccato)");
        check(legatoSecond > gatedFirst * 0.5,
              "a full arp gate sustains the note through the step (legato)");

        audio::Sequencer dg;
        check(dg.arpGate() == 1.0f, "arp gate defaults to 1 (legato)");

        audio::Sequencer dm;
        check(dm.arpMode() == 0, "arp mode defaults to up");
        check(dm.arpRate() == 1, "arp rate defaults to 1 (one note per step)");

        // Chord mode (5): every held pitch sounds at once (a rhythmic stab), not one per step.
        audio::Sequencer chord;
        chord.setBpm(120.0);
        chord.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        chord.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        chord.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        chord.synth().setEnvelope(0.001f, 0.05f, 1.0f, 0.05f); // sustain so all voices stay up
        chord.setArp(true, 5);
        chord.play();                                // strikes step 0
        (void)renderMono(chord, 512, sampleRate);    // let the attacks ramp up
        check(chord.synth().activeVoices() == 3, "chord arp strikes all three held notes at once");

        // A single-note (up) arp sounds only one voice at a time, for contrast.
        audio::Sequencer single;
        single.setBpm(120.0);
        single.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        single.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        single.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        single.synth().setEnvelope(0.001f, 0.05f, 1.0f, 0.05f);
        single.setArp(true, 0);
        single.play();
        (void)renderMono(single, 512, sampleRate);
        check(single.synth().activeVoices() == 1, "an up arp sounds a single voice at a time");

        // Rate 2: the arp advances every 2 steps, holding the note across the skipped step.
        audio::Sequencer rate;
        rate.setBpm(120.0);
        rate.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        rate.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        rate.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        rate.setArp(true, 0); // up
        rate.setArpRate(2);
        check(rate.arpRate() == 2, "arp rate is settable");
        rate.play(); // step 0 → first note
        const int r0 = rate.arpCurrentPitch();
        (void)renderMono(rate, 6000, sampleRate); // step 1 → held (skipped)
        const int r1 = rate.arpCurrentPitch();
        (void)renderMono(rate, 6000, sampleRate); // step 2 → advance
        const int r2 = rate.arpCurrentPitch();
        check(r0 == 60 && r1 == 60, "arp rate 2 holds the note across the skipped step");
        check(r2 == 64, "arp advances to the next note after the rate interval");
    }

    // --- Per-note roll / ratchet --------------------------------------------
    {
        // Count amplitude onsets (rising crossings of a follower) over the note's first step.
        auto countOnsets = [&](int rollCount) {
            audio::Sequencer s;
            s.setBpm(120.0); // 6000 frames per 16th step @ 48 kHz
            s.synth().setEnvelope(0.001f, 0.02f, 0.0f, 0.005f); // percussive: each hit is a burst
            audio::Note n{0, 1, 60, 1.0f, 1.0f, 0.0f};
            n.roll = rollCount;
            s.roll().addNote(n);
            s.play();
            const std::vector<float> out = renderMono(s, 6000, sampleRate);
            std::vector<float> env(out.size() / 2, 0.0f);
            float e = 0.0f;
            float peak = 0.0f;
            for (size_t i = 0; i + 1 < out.size(); i += 2) {
                e += 0.02f * (std::fabs(out[i]) - e);
                env[i / 2] = e;
                peak = std::max(peak, e);
            }
            if (peak <= 0.0f) {
                return 0;
            }
            const float hi = 0.35f * peak, lo = 0.12f * peak;
            int onsets = 0;
            bool above = false;
            for (float v : env) {
                if (!above && v > hi) {
                    ++onsets;
                    above = true;
                } else if (above && v < lo) {
                    above = false;
                }
            }
            return onsets;
        };
        const int single = countOnsets(1);
        const int rolled = countOnsets(4);
        check(single == 1, "a normal note has a single onset within its step");
        check(rolled >= 3, "a roll=4 note retriggers several times within its step");
        check(rolled > single, "a rolled note adds retriggers over a plain note");

        audio::Sequencer dn;
        dn.roll().addNote(audio::Note{0, 4, 60, 1.0f});
        check(dn.roll().noteRoll(60, 0) == 1, "a note defaults to no roll");
        dn.roll().setNoteRoll(60, 0, 20);
        check(dn.roll().noteRoll(60, 0) == 8, "note roll clamps to 8");
    }

    // --- Sidechain ducking ---------------------------------------------------
    {
        // A kick on step 0 (muted so only the ducking is heard) ducks a sustained synth note; the
        // synth is quiet right after the kick and recovers over the release.
        audio::Sequencer sc;
        sc.setBpm(120.0);
        sc.setStep(0, 0, true);       // kick step (drives the sidechain)
        sc.setChannelMute(0, true);   // silence the kick itself
        sc.roll().addNote(audio::Note{0, 16, 60, 1.0f}); // sustained synth note across the bar
        sc.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        sc.setSidechain(true, 0.9f, 250.0f);
        sc.play();
        const std::vector<float> out = renderMono(sc, 16 * 6000, sampleRate);
        // Compare the synth level just after the kick vs. later in the bar.
        auto windowRms = [&](int fromFrame, int toFrame) {
            double s = 0.0;
            for (int i = fromFrame; i < toFrame; ++i) {
                const double v = static_cast<double>(out[static_cast<size_t>(i) * 2]);
                s += v * v;
            }
            return std::sqrt(s / static_cast<double>(toFrame - fromFrame));
        };
        const double early = windowRms(200, 1500);   // ducked
        const double late = windowRms(20000, 25000);  // recovered
        check(early < late * 0.7, "sidechain ducks the synth right after the kick");
        check(late > 0.0, "synth recovers after the duck");

        // Routable source: with the source set to the snare (channel 1), a hit on channel 1 — and
        // NOT the kick — drives the duck.
        audio::Sequencer sc2;
        sc2.setBpm(120.0);
        sc2.setStep(1, 0, true);     // snare step on channel 1
        sc2.setChannelMute(1, true); // silence the snare itself
        sc2.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        sc2.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        sc2.setSidechain(true, 0.9f, 250.0f);
        sc2.setSidechainSource(1); // duck from the snare, not the kick
        check(sc2.sidechainSource() == 1, "sidechain source is settable");
        sc2.play();
        const std::vector<float> out2 = renderMono(sc2, 16 * 6000, sampleRate);
        auto win2 = [&](int a, int b) {
            double s = 0.0;
            for (int i = a; i < b; ++i) {
                const double v = static_cast<double>(out2[static_cast<size_t>(i) * 2]);
                s += v * v;
            }
            return std::sqrt(s / static_cast<double>(b - a));
        };
        check(win2(200, 1500) < win2(20000, 25000) * 0.7,
              "the routed (snare) channel drives the sidechain duck");

        // Melodic source: a note on the BASS lane (not a drum) triggers the duck. Measure the isolated
        // lead stem (the bass audio lands in the bass stem, so the lead stem cleanly shows the duck).
        audio::Sequencer scm;
        scm.setBpm(120.0);
        scm.roll().addNote(audio::Note{0, 16, 60, 1.0f});  // sustained lead note (the ducked content)
        scm.roll2().addNote(audio::Note{0, 1, 40, 1.0f});  // bass note at step 0 → the trigger
        scm.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        scm.setSidechain(true, 0.9f, 250.0f);
        scm.setSidechainMelodicSource(1); // bass lane drives the duck
        check(scm.sidechainMelodicSource() == 1, "melodic sidechain source is settable");
        scm.play();
        const int mfr = 16 * 6000;
        std::vector<float> md(static_cast<size_t>(mfr) * 2, 0.0f);
        std::vector<float> ml(static_cast<size_t>(mfr) * 2, 0.0f);
        std::vector<float> mb(static_cast<size_t>(mfr) * 2, 0.0f);
        scm.renderStems(md.data(), ml.data(), mb.data(), mfr, sampleRate);
        auto leadWin = [&](int a, int b) {
            double s = 0.0;
            for (int i = a; i < b; ++i) {
                const double v = static_cast<double>(ml[static_cast<size_t>(i) * 2]);
                s += v * v;
            }
            return std::sqrt(s / static_cast<double>(b - a));
        };
        check(leadWin(200, 1500) < leadWin(20000, 25000) * 0.7,
              "a melodic (bass-lane) note drives the sidechain duck on the lead bus");

        // Attack: with an attack time the duck ramps in, so the level *immediately* after the kick is
        // higher (less ducked) than with the instant (attack=0) snap — a softer, rounded pump.
        auto buildSc = [&](float attackMs) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(0, 0, true);
            s.setChannelMute(0, true);
            s.roll().addNote(audio::Note{0, 16, 60, 1.0f});
            s.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setSidechain(true, 0.9f, 250.0f, attackMs);
            s.play();
            return renderMono(s, 16 * 6000, sampleRate);
        };
        const std::vector<float> snap = buildSc(0.0f);   // instant duck
        const std::vector<float> ramp = buildSc(40.0f);  // 40 ms attack
        auto winOf = [&](const std::vector<float>& b, int a, int c) {
            double s = 0.0;
            for (int i = a; i < c; ++i) {
                const double v = static_cast<double>(b[static_cast<size_t>(i) * 2]);
                s += v * v;
            }
            return std::sqrt(s / static_cast<double>(c - a));
        };
        // Right after the kick (~2–15 ms) the ramped duck has not fully closed yet, so it is louder.
        check(winOf(ramp, 100, 700) > winOf(snap, 100, 700) * 1.3,
              "sidechain attack ramps the duck in (louder just after the kick than an instant snap)");
        // The ramped duck still pumps: near the trough (just after the ~40 ms attack completes) it is
        // much quieter than the fully-recovered tail.
        check(winOf(ramp, 2000, 2600) < winOf(ramp, 20000, 25000) * 0.7,
              "with attack the duck still closes to the floor then recovers (a rounded pump)");
        audio::Sequencer scDef;
        check(std::fabs(scDef.sidechainAttackMs()) < 1e-6f,
              "sidechain attack defaults to 0 (instant snap, back-compatible)");
    }

    // --- Arrangement: patterns + playlist -----------------------------------
    audio::Sequencer arr;
    check(arr.patternCount() == 1, "starts with one pattern");
    arr.setStep(0, 0, true); // kick on step 0 of pattern 0
    const int p1 = arr.addPattern();
    check(p1 == 1 && arr.patternCount() == 2, "addPattern appends");
    arr.selectPattern(p1);
    check(!arr.step(0, 0), "a fresh pattern is empty (independent grid)");
    arr.setStep(1, 0, true); // snare on step 0 of pattern 1
    arr.selectPattern(0);
    check(arr.step(0, 0) && !arr.step(1, 0), "patterns keep independent grids");

    // Song mode: playlist [0,1] should switch the current pattern at each bar boundary.
    arr.setBpm(120.0); // 6000 samples/step → 96000 samples/bar (16 steps)
    arr.setPlaylist({0, 1});
    arr.setSongMode(true);
    arr.play();
    check(arr.currentPattern() == 0, "song mode starts on the first playlist pattern");
    // Render one full bar (16 steps × 6000).
    (void)renderMono(arr, 16 * 6000, sampleRate);
    check(arr.currentPattern() == 1, "advances to the next playlist pattern after a bar");
    (void)renderMono(arr, 16 * 6000, sampleRate);
    check(arr.currentPattern() == 0, "playlist wraps back to the start");

    // Play-once: with song loop off, the transport stops at the end of the playlist.
    {
        audio::Sequencer once;
        once.setBpm(120.0);
        once.setPlaylist({0, 1});
        once.setSongMode(true);
        once.setSongLoop(false);
        check(!once.songLoop(), "song loop is settable off");
        once.play();
        (void)renderMono(once, 16 * 6000, sampleRate); // bar 1 → entry 1
        check(once.playing(), "play-once still playing during the arrangement");
        (void)renderMono(once, 16 * 6000, sampleRate); // bar 2 ends → would wrap → stop
        check(!once.playing(), "play-once stops at the end of the playlist");

        // With loop on (default), it keeps playing past the end.
        audio::Sequencer looped;
        looped.setBpm(120.0);
        looped.setPlaylist({0, 1});
        looped.setSongMode(true);
        looped.play();
        (void)renderMono(looped, 3 * 16 * 6000, sampleRate); // three bars
        check(looped.playing(), "a looping song keeps playing past the playlist end");
        check(looped.songLoop(), "song loop defaults to on");
    }

    // Song loop region: restrict playback to a sub-range of the playlist [start, end).
    {
        audio::Sequencer r;
        r.setBpm(120.0);
        r.addPattern();
        r.addPattern();
        r.addPattern(); // patterns 0..3 exist
        r.setPlaylist({0, 1, 2, 3});
        r.setSongMode(true);
        r.setSongLoopRange(1, 3); // loop patterns at playlist indices 1,2
        r.play();
        check(r.currentPattern() == 1, "song loop region starts at the region start");
        (void)renderMono(r, 16 * 6000, sampleRate);
        check(r.currentPattern() == 2, "region advances to the next entry");
        (void)renderMono(r, 16 * 6000, sampleRate);
        check(r.currentPattern() == 1, "region wraps back to the start (never reaches index 3)");
        (void)renderMono(r, 16 * 6000, sampleRate);
        check(r.currentPattern() == 2, "region keeps cycling within [1,3)");

        audio::Sequencer d;
        check(d.songLoopEnd() <= d.songLoopStart(), "no loop region by default (whole playlist)");
        d.setSongLoopRange(2, 1); // degenerate → cleared
        check(d.songLoopEnd() <= d.songLoopStart(), "a degenerate region is treated as cleared");
    }

    // --- Metronome: accented clicks on each beat ------------------------------
    {
        // Empty pattern → silent except for the metronome. At 120 BPM a beat is 0.5 s = 24000
        // frames; over 2 s we expect 4 beats (4 clicks).
        audio::Sequencer metro;
        metro.setBpm(120.0);
        metro.clear();
        metro.roll().clear();
        metro.setMetronome(true);
        metro.play();
        const int frames = sampleRate * 2; // 2 seconds
        const std::vector<float> out = renderMono(metro, frames, sampleRate);
        check(rms(out) > 0.0, "metronome produces sound on an empty pattern");

        // Count click onsets: windows (per 24000-frame beat) that contain energy.
        int clicks = 0;
        const int beatFrames = 24000;
        for (int b = 0; b < 4; ++b) {
            double e = 0.0;
            // The click sits at the start of each beat window (~first 40 ms).
            for (int i = b * beatFrames; i < b * beatFrames + 3000 && i < frames; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            if (e > 1e-4) {
                ++clicks;
            }
        }
        check(clicks == 4, "metronome clicks once per beat (4 beats in 2 s @120 BPM)");

        // Between clicks (e.g. mid-beat) it is silent.
        double midEnergy = 0.0;
        for (int i = 12000; i < 20000; ++i) {
            midEnergy += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
        }
        check(midEnergy < 1e-6, "metronome is silent between clicks");

        // Disabled → silence on an empty pattern.
        audio::Sequencer off;
        off.setMetronome(false);
        off.play();
        const std::vector<float> q = renderMono(off, sampleRate, sampleRate);
        check(rms(q) == 0.0, "metronome off leaves an empty pattern silent");

        // Level scales the click loudness.
        audio::Sequencer lvl;
        check(std::fabs(lvl.metronomeLevel() - 0.5f) < 1e-6f, "metronome level defaults to 0.5");
        auto clickRms = [&](float level) {
            audio::Sequencer m;
            m.setBpm(120.0);
            m.setMetronome(true);
            m.setMetronomeLevel(level);
            m.play();
            return rms(renderMono(m, sampleRate / 2, sampleRate)); // half a second (a couple clicks)
        };
        const double soft = clickRms(0.25f);
        const double loud = clickRms(1.0f);
        check(soft > 0.0 && loud > soft * 1.5, "a higher metronome level makes a louder click");
        check(clickRms(0.0f) == 0.0, "metronome level 0 silences the click");
    }

    // --- Per-note probability -------------------------------------------------
    {
        // Count how many of N loops a lead note actually sounds at a given probability.
        auto soundedLoops = [&](float prob, int loops) {
            audio::Sequencer s;
            s.setBpm(120.0);
            audio::Note n{0, 2, 60, 1.0f, prob};
            s.roll().addNote(n);
            s.synth().setEnvelope(0.001f, 0.01f, 0.9f, 0.02f);
            s.play();
            int sounded = 0;
            const int loopFrames = 16 * 6000; // one 16-step bar @120 BPM
            for (int l = 0; l < loops; ++l) {
                const std::vector<float> out = renderMono(s, loopFrames, sampleRate);
                double e = 0.0;
                for (int i = 0; i < 3000; ++i) {
                    e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
                }
                if (e > 1e-4) {
                    ++sounded;
                }
            }
            return sounded;
        };

        check(soundedLoops(1.0f, 8) == 8, "a note at probability 1.0 always sounds");
        check(soundedLoops(0.0f, 8) == 0, "a note at probability 0.0 never sounds");
        const int half = soundedLoops(0.5f, 32);
        check(half > 4 && half < 28, "a note at probability 0.5 sounds some loops but not all");

        audio::Note def;
        check(def.probability == 1.0f, "notes default to probability 1.0");
    }

    // --- Global transpose ----------------------------------------------------
    {
        // A held A3 (220 Hz) note; transposing up an octave should render ~440 Hz.
        auto pitchHz = [&](int transpose) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.roll().addNote(audio::Note{0, 16, 57, 1.0f}); // A3 = 220 Hz
            s.synth().setWaveform(audio::Waveform::Saw);
            s.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.synth().setFilter(20000.0f, 0.7f, 0.0f);
            s.setTranspose(transpose);
            s.play();
            const std::vector<float> out = renderMono(s, 6000, sampleRate);
            // Rising zero-crossings on the left channel → fundamental frequency.
            int crossings = 0;
            for (size_t i = 1; i < 6000; ++i) {
                if (out[(i - 1) * 2] <= 0.0f && out[i * 2] > 0.0f) {
                    ++crossings;
                }
            }
            return static_cast<double>(crossings) * sampleRate / 6000.0;
        };
        check(std::fabs(pitchHz(0) - 220.0) < 6.0, "no transpose plays the written pitch");
        check(std::fabs(pitchHz(12) - 440.0) < 12.0, "transpose +12 raises the note an octave");
        check(std::fabs(pitchHz(-12) - 110.0) < 6.0, "transpose -12 lowers the note an octave");

        audio::Sequencer def;
        check(def.transpose() == 0, "transpose defaults to 0");
    }

    // --- Clone pattern: an independent copy ----------------------------------
    {
        audio::Sequencer s;
        s.setStep(0, 0, true);
        s.setStep(1, 4, true);
        s.setStepProbability(1, 4, 0.5f);
        s.setStepRatchet(0, 0, 3);
        s.roll().addNote(audio::Note{2, 4, 64, 0.7f});
        s.setPatternName(0, "Verse");

        const int c = s.clonePattern(0);
        check(s.patternCount() == 2 && c == 1, "clone appends a new pattern");
        check(s.patternName(1) == "Verse copy", "clone names itself '<src> copy'");

        s.selectPattern(1);
        check(s.step(0, 0) && s.step(1, 4), "clone copies the drum steps");
        check(std::fabs(s.stepProbability(1, 4) - 0.5f) < 0.01f && s.stepRatchet(0, 0) == 3,
              "clone copies per-step probability and ratchet");
        check(s.roll().notes().size() == 1 && s.roll().notes()[0].pitch == 64,
              "clone copies the piano-roll notes");

        // Editing the clone does not touch the original.
        s.setStep(3, 8, true);
        s.selectPattern(0);
        check(!s.step(3, 8), "editing the clone leaves the original untouched");
    }

    // --- Melodic bus pan: place the lead in the stereo field -----------------
    {
        auto leadLR = [&](float pan) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.roll().addNote(audio::Note{0, 16, 60, 1.0f}); // a sustained lead note
            s.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setLeadPan(pan);
            s.play();
            const std::vector<float> out = renderMono(s, 6000, sampleRate);
            return std::make_pair(rmsChannel(out, 0), rmsChannel(out, 1));
        };

        const auto center = leadLR(0.0f);
        check(std::fabs(center.first - center.second) < 1e-4, "lead pan 0 is centered (L == R)");

        const auto left = leadLR(-1.0f);
        check(left.first > left.second * 5.0, "lead pan -1 sends the lead to the left channel");

        const auto right = leadLR(1.0f);
        check(right.second > right.first * 5.0, "lead pan +1 sends the lead to the right channel");

        // Bass pan is independent and defaults to center.
        audio::Sequencer d;
        check(d.leadPan() == 0.0f && d.bassPan() == 0.0f, "melodic buses default to center");
    }

    // --- Steps per beat: grid subdivision ------------------------------------
    {
        // At 120 BPM a beat is 0.5 s. With 4 steps/beat a step is 6000 frames; with 8 steps/beat a
        // step is 3000 frames. Advancing one step's worth of frames should land on step 1 either way.
        audio::Sequencer a;
        a.setBpm(120.0);
        check(a.stepsPerBeat() == 4, "default is 4 steps per beat");
        a.play();
        (void)renderMono(a, 6000, sampleRate);
        check(a.currentStep() == 1, "at 4 steps/beat one step is 6000 frames @120 BPM");

        audio::Sequencer b;
        b.setBpm(120.0);
        b.setStepsPerBeat(8);
        check(b.stepsPerBeat() == 8, "steps-per-beat is settable");
        b.play();
        (void)renderMono(b, 3000, sampleRate);
        check(b.currentStep() == 1, "at 8 steps/beat one step is 3000 frames @120 BPM");
        (void)renderMono(b, 3000, sampleRate);
        check(b.currentStep() == 2, "the finer grid keeps advancing evenly");

        // Clamped to 1–8.
        audio::Sequencer c;
        c.setStepsPerBeat(99);
        check(c.stepsPerBeat() == 8, "steps-per-beat clamps to 8");
        c.setStepsPerBeat(0);
        check(c.stepsPerBeat() == 1, "steps-per-beat clamps to 1");
    }

    // --- Pattern length: resizable step count --------------------------------
    {
        audio::Sequencer s;
        check(s.numSteps() == 16, "default pattern length is 16");
        s.setStep(0, 4, true);
        s.setStep(1, 12, true);
        s.setStepVelocity(2, 3, 0.5f);

        s.setNumSteps(32);
        check(s.numSteps() == 32, "pattern grows to 32 steps");
        check(s.step(0, 4) && s.step(1, 12), "existing steps survive a grow");
        check(!s.step(0, 20), "new steps start empty");
        s.setStep(0, 24, true); // a step only reachable at the longer length
        check(s.step(0, 24), "steps in the extended range are usable");

        s.setNumSteps(8);
        check(s.numSteps() == 8, "pattern shrinks to 8 steps");
        check(s.step(0, 4), "steps within the new length survive a shrink");
        s.setNumSteps(16);
        check(!s.step(1, 12), "steps beyond a shrink are gone (not restored)");

        // A 32-step pattern loops over its full length: at 120 BPM a step is 6000 frames, so 32
        // steps advance the transport through step 31 and wrap to 0.
        audio::Sequencer t;
        t.setNumSteps(32);
        t.setBpm(120.0);
        t.play();
        (void)renderMono(t, 31 * 6000, sampleRate);
        check(t.currentStep() == 31, "32-step pattern reaches step 31");
        (void)renderMono(t, 6000, sampleRate);
        check(t.currentStep() == 0, "32-step pattern wraps after its full length");
    }

    // --- Drum decay: the tail length scales ----------------------------------
    {
        // Count how long a kick stays active for a short vs long decay multiplier.
        auto activeFrames = [&](float mul) {
            audio::DrumVoice k;
            k.setType(audio::Drum::Kick);
            k.setDecay(mul);
            k.trigger(1.0f);
            int frames = 0;
            while (k.active() && frames < sampleRate) {
                std::vector<float> b(64, 0.0f);
                k.render(b.data(), 64, sampleRate);
                frames += 64;
            }
            return frames;
        };
        const int shortT = activeFrames(0.5f);
        const int longT = activeFrames(3.0f);
        check(longT > shortT * 2, "a longer decay multiplier lengthens the drum tail");

        audio::Sequencer d;
        check(d.channelDecay(0) == 1.0f, "channel decay defaults to 1");
    }

    // --- Drum tuning: a tuned kick shifts pitch ------------------------------
    {
        // A kick's fundamental rises when tuned up. Estimate its pitch from the sustained tail.
        auto kickHz = [&](float semis) {
            audio::DrumVoice k;
            k.setType(audio::Drum::Kick);
            k.setTune(semis);
            k.trigger(1.0f);
            std::vector<float> buf(4800, 0.0f);
            k.render(buf.data(), 4800, sampleRate);
            // Zero-crossing rate over the body (skip the initial pitch sweep).
            int crossings = 0;
            for (int i = 1201; i < 4800; ++i) {
                if (buf[static_cast<size_t>(i - 1)] <= 0.0f && buf[static_cast<size_t>(i)] > 0.0f) {
                    ++crossings;
                }
            }
            return static_cast<double>(crossings) * sampleRate / (4800 - 1200);
        };
        const double base = kickHz(0.0f);
        const double up = kickHz(12.0f); // +1 octave
        check(base > 0.0, "kick has a measurable pitch");
        check(up > base * 1.6, "tuning the kick up an octave raises its pitch");

        audio::Sequencer d;
        check(d.channelTune(0) == 0.0f, "channels default to 0 tune");
    }

    // --- Drum drive: saturation adds harmonics -------------------------------
    {
        auto renderKick = [&](float drive) {
            audio::DrumVoice k;
            k.setType(audio::Drum::Kick);
            k.setDrive(drive);
            k.trigger(1.0f);
            std::vector<float> buf(4800, 0.0f);
            k.render(buf.data(), 4800, sampleRate);
            return buf;
        };
        // Level-independent brightness: HF (first-difference) energy over total energy.
        auto brightness = [](const std::vector<float>& b) {
            double hf = 0.0, en = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                hf += d * d;
                en += static_cast<double>(b[i]) * b[i];
            }
            return en > 0.0 ? hf / en : 0.0;
        };
        const std::vector<float> clean = renderKick(0.0f);
        const std::vector<float> driven = renderKick(0.9f);
        check(brightness(driven) > brightness(clean) * 1.2, "drum drive adds harmonics (brighter)");

        audio::DrumVoice dv;
        check(dv.drive() == 0.0f, "drum drive defaults to 0 (clean)");
        audio::Sequencer d;
        check(d.channelDrive(0) == 0.0f, "channel drive defaults to 0");
    }

    // --- Flam: a grace hit followed by the main hit --------------------------
    {
        auto renderFlam = [&](float flamMs) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(2, 0, true); // closed hat — short, so a second onset is easy to see
            s.setChannelFlam(2, flamMs);
            s.play();
            return renderMono(s, 6000, sampleRate);
        };
        auto energyAt = [](const std::vector<float>& out, int start, int len) {
            double e = 0.0;
            for (int i = start; i < start + len && i < 6000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };
        const std::vector<float> noFlam = renderFlam(0.0f);
        const std::vector<float> flam = renderFlam(30.0f); // 30 ms → main hit ~1440 frames in
        const double ref = energyAt(noFlam, 0, 500);
        check(ref > 1e-4, "the flam grace/first hit sounds at the step start");
        // Without flam the hat has decayed by ~1440 frames; with flam the full hit lands there.
        check(energyAt(noFlam, 1440, 500) < ref * 0.2, "no flam → single hit, decayed by 30 ms");
        check(energyAt(flam, 1440, 500) > ref * 0.3, "flam fires the main hit after the grace");

        audio::Sequencer dd;
        check(dd.channelFlam(0) == 0.0f, "channel flam defaults to 0 (off)");
    }

    // --- Tom voice + per-channel drum type -----------------------------------
    {
        audio::DrumVoice tom;
        tom.setType(audio::Drum::Tom);
        tom.trigger(1.0f);
        std::vector<float> buf(4800, 0.0f);
        tom.render(buf.data(), 4800, sampleRate);
        check(rms(buf) > 0.0, "tom produces sound");
        // Tom is tonal — count zero crossings over the sustained body (skip the initial sweep).
        int crossings = 0;
        for (int i = 1201; i < 4800; ++i) {
            if (buf[static_cast<size_t>(i - 1)] <= 0.0f && buf[static_cast<size_t>(i)] > 0.0f) {
                ++crossings;
            }
        }
        check(crossings > 0, "tom has a measurable pitch");

        // Per-channel type selection: any row can be reassigned to any drum voice.
        audio::Sequencer s;
        s.setChannelType(0, audio::Drum::Tom);
        check(s.channelType(0) == audio::Drum::Tom, "a channel's drum type can be reassigned");
        audio::Sequencer d;
        check(d.channelType(0) == audio::Drum::Kick, "channel 0 defaults to the kick");

        // Cowbell: two detuned square tones → an audible, bright metallic hit.
        audio::DrumVoice cow;
        cow.setType(audio::Drum::Cowbell);
        cow.trigger(1.0f);
        std::vector<float> cb(4800, 0.0f);
        cow.render(cb.data(), 4800, sampleRate);
        check(rms(cb) > 0.0, "cowbell produces sound");
        int cowCross = 0;
        for (int i = 1; i < 2400; ++i) {
            if (cb[static_cast<size_t>(i - 1)] <= 0.0f && cb[static_cast<size_t>(i)] > 0.0f) {
                ++cowCross;
            }
        }
        check(cowCross > 20, "cowbell rings at its metallic (few-hundred-Hz) tones");

        // Rimshot: a short, bright crack that decays very fast.
        audio::DrumVoice rim;
        rim.setType(audio::Drum::Rimshot);
        rim.trigger(1.0f);
        std::vector<float> rb(4800, 0.0f);
        rim.render(rb.data(), 4800, sampleRate);
        check(rms(rb) > 0.0, "rimshot produces sound");
        // With tau ~0.05 s the voice is silent (~6 tau) well before 0.5 s.
        std::vector<float> rb2(24000, 0.0f);
        rim.render(rb2.data(), 24000, sampleRate);
        check(!rim.active(), "rimshot decays fast (inactive within 0.6 s)");

        // Crash: a long, bright, noisy cymbal wash — many zero crossings and a long tail.
        audio::DrumVoice crash;
        crash.setType(audio::Drum::Crash);
        crash.trigger(1.0f);
        std::vector<float> cr(24000, 0.0f); // 0.5 s
        crash.render(cr.data(), 24000, sampleRate);
        check(rms(cr) > 0.0, "crash produces sound");
        check(crash.active(), "crash still rings at 0.5 s (long decay)");
        int crashCross = 0;
        for (int i = 1; i < 24000; ++i) {
            if (cr[static_cast<size_t>(i - 1)] <= 0.0f && cr[static_cast<size_t>(i)] > 0.0f) {
                ++crashCross;
            }
        }
        // A noisy cymbal crosses zero far more often than a tonal drum.
        check(crashCross > 2000, "crash is bright/noisy (many zero crossings)");

        // Ride: a pingy, sustained cymbal — rings at 0.5 s but is more tonal (fewer crossings) than
        // the noise-heavy crash.
        audio::DrumVoice ride;
        ride.setType(audio::Drum::Ride);
        ride.trigger(1.0f);
        std::vector<float> rd(24000, 0.0f); // 0.5 s
        ride.render(rd.data(), 24000, sampleRate);
        check(rms(rd) > 0.0, "ride produces sound");
        check(ride.active(), "ride sustains at 0.5 s (medium-long decay)");
        int rideCross = 0;
        for (int i = 1; i < 24000; ++i) {
            if (rd[static_cast<size_t>(i - 1)] <= 0.0f && rd[static_cast<size_t>(i)] > 0.0f) {
                ++rideCross;
            }
        }
        check(rideCross < crashCross,
              "ride is more tonal/defined than the crash (fewer zero crossings)");
    }

    // --- Channel rotate: shift a step row around the bar ---------------------
    {
        audio::Sequencer s;
        s.setStep(0, 0, true);
        s.setStep(0, 1, true);
        s.setStepRatchet(0, 0, 3); // travels with step 0
        s.rotateChannel(0, 2);     // shift later by 2
        check(!s.step(0, 0) && !s.step(0, 1) && s.step(0, 2) && s.step(0, 3),
              "rotate shifts the step row later");
        check(s.stepRatchet(0, 2) == 3, "rotate carries the per-step ratchet with its step");

        // Wraparound: the last step rolls to the front.
        audio::Sequencer w;
        const int last = w.numSteps() - 1;
        w.setStep(0, last, true);
        w.rotateChannel(0, 1);
        check(w.step(0, 0) && !w.step(0, last), "rotate wraps around the pattern length");

        // Negative offset shifts earlier.
        audio::Sequencer nb;
        nb.setStep(0, 2, true);
        nb.rotateChannel(0, -2);
        check(nb.step(0, 0) && !nb.step(0, 2), "a negative rotate shifts the row earlier");

        // A full-length rotate is a no-op.
        audio::Sequencer fr;
        fr.setStep(0, 5, true);
        fr.rotateChannel(0, fr.numSteps());
        check(fr.step(0, 5), "rotating by the full length leaves the row unchanged");
    }

    // --- Euclidean fill: evenly distributed pulses ---------------------------
    {
        audio::Sequencer s; // default 16 steps
        // 4 pulses over 16 steps → a hit every 4 steps (0, 4, 8, 12).
        const int placed = s.euclidFill(0, 4);
        check(placed == 4, "euclid places exactly the requested pulse count");
        check(s.step(0, 0) && s.step(0, 4) && s.step(0, 8) && s.step(0, 12),
              "4 pulses over 16 steps land on the quarter beats");
        check(!s.step(0, 1) && !s.step(0, 5), "off-grid steps stay empty");

        // The hit count always matches the requested pulses (even for uneven divisions).
        bool countsMatch = true;
        for (int k = 0; k <= 16; ++k) {
            audio::Sequencer e;
            if (e.euclidFill(0, k) != k) {
                countsMatch = false;
            }
        }
        check(countsMatch, "euclid hit count matches the requested pulses for every k in 0..16");

        // A previous pattern is replaced, and 0 pulses clears the row.
        audio::Sequencer c;
        c.setStep(0, 3, true);
        c.euclidFill(0, 0);
        check(!c.step(0, 3), "a zero-pulse euclid fill clears the row");
    }

    // --- Choke: a voice can be silenced mid-ring -----------------------------
    {
        audio::DrumVoice oh;
        oh.setType(audio::Drum::OpenHat); // long tail (~0.28 s)
        oh.trigger(1.0f);
        std::vector<float> a(480, 0.0f);
        oh.render(a.data(), 480, sampleRate); // 10 ms
        check(oh.active() && rms(a) > 0.0, "open hat rings after trigger");
        oh.choke();
        std::vector<float> b(960, 0.0f);
        oh.render(b.data(), 960, sampleRate); // 20 ms — the ~4 ms choke fade completes
        check(!oh.active(), "a choked voice goes silent");
    }

    // --- Choke groups: closed hat cuts off open hat --------------------------
    {
        auto tailEnergy = [&](bool chokeOn) {
            audio::Sequencer s;
            s.setBpm(120.0);           // step = 6000 frames
            s.setStep(3, 0, true);     // open hat on step 0 (long tail)
            s.setStep(2, 1, true);     // closed hat on step 1
            s.setChannelChokeGroup(3, chokeOn ? 1 : 0);
            s.setChannelChokeGroup(2, chokeOn ? 1 : 0);
            s.play();
            const std::vector<float> out = renderMono(s, 12000, sampleRate);
            // Energy in the open-hat tail AFTER the closed hat hits (well past step 1's onset).
            double e = 0.0;
            for (int i = 7200; i < 12000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };
        const double choked = tailEnergy(true);
        const double open = tailEnergy(false);
        check(choked < open * 0.5, "a choke group cuts the open-hat tail when the closed hat hits");

        // Default kit already puts the two hats in a choke group.
        audio::Sequencer d;
        check(d.channelChokeGroup(2) == d.channelChokeGroup(3) && d.channelChokeGroup(2) != 0,
              "default kit chokes the closed and open hats together");
    }

    // --- Per-step ratchet -----------------------------------------------------
    {
        // A step slot is 6000 frames @120 BPM. A ratchet of R places hits at 0, 6000/R, 2·6000/R…
        // Render one slot and measure energy in a short window at each expected hit position.
        auto render1 = [&](int ratchet) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(2, 0, true); // closed hat — short, decays fast between ratchet hits
            s.setStepRatchet(2, 0, ratchet);
            s.play();
            return renderMono(s, 6000, sampleRate);
        };
        auto energyAt = [](const std::vector<float>& out, int start, int len) {
            double e = 0.0;
            for (int i = start; i < start + len && i < 6000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };

        const std::vector<float> one = render1(1);
        const std::vector<float> four = render1(4);
        const std::vector<float> two = render1(2);
        const double ref = energyAt(one, 0, 500); // a single hat hit's onset energy

        check(ref > 1e-4, "non-ratcheted step hits at the start of the slot");
        // The single hat has decayed away well before the quarter points.
        check(energyAt(one, 1500, 500) < ref * 0.2 && energyAt(one, 3000, 500) < ref * 0.2,
              "a single hat has no extra hits mid-slot");

        // 4× ratchet: a fresh hat at each quarter (0, 1500, 3000, 4500).
        check(energyAt(four, 1500, 500) > ref * 0.3 && energyAt(four, 3000, 500) > ref * 0.3 &&
                  energyAt(four, 4500, 500) > ref * 0.3,
              "4x ratchet fires fresh hits at each quarter of the step");

        // 2× ratchet: a fresh hat at the half-way point (3000), but not at the 1500 quarter.
        check(energyAt(two, 3000, 500) > ref * 0.3 && energyAt(two, 1500, 500) < ref * 0.2,
              "2x ratchet fires at the half, not the quarter, of the step");

        audio::Sequencer d;
        check(d.stepRatchet(0, 0) == 1, "steps default to ratchet 1");
    }

    // --- Per-step probability -------------------------------------------------
    {
        // Count how many times a kick on step 0 fires over N bars at a given probability.
        auto countHits = [&](float prob, int bars) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(0, 0, true);
            s.setStepProbability(0, 0, prob);
            s.play();
            int hits = 0;
            const int barFrames = 16 * 6000; // 16 steps × 6000 frames @120 BPM
            for (int b = 0; b < bars; ++b) {
                const std::vector<float> out = renderMono(s, barFrames, sampleRate);
                // The kick sits at the start of the bar; energy in the first 3000 frames = a hit.
                double e = 0.0;
                for (int i = 0; i < 3000; ++i) {
                    e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
                }
                if (e > 1e-4) {
                    ++hits;
                }
            }
            return hits;
        };

        check(countHits(1.0f, 8) == 8, "probability 1.0 fires every bar");
        check(countHits(0.0f, 8) == 0, "probability 0.0 never fires");
        const int half = countHits(0.5f, 32);
        check(half > 4 && half < 28, "probability 0.5 fires some bars but not all");

        // Default probability is 1.0 (always).
        audio::Sequencer d;
        check(d.stepProbability(0, 0) == 1.0f, "steps default to probability 1.0");
    }

    // --- Per-step trig condition (stride): fire only every Nth loop -----------
    {
        // Which of the first 4 loops does a kick on step 0 fire in, at a given stride? (Deterministic,
        // unlike probability — a stride-N step fires on loops 0, N, 2N, …)
        auto barsFired = [&](int stride) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(0, 0, true);
            s.setStepStride(0, 0, stride);
            s.play();
            std::vector<bool> fired;
            const int barFrames = 16 * 6000; // one 16-step bar @120 BPM
            for (int b = 0; b < 4; ++b) {
                const std::vector<float> out = renderMono(s, barFrames, sampleRate);
                double e = 0.0;
                for (int i = 0; i < 3000; ++i) {
                    e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
                }
                fired.push_back(e > 1e-4);
            }
            return fired;
        };
        const std::vector<bool> every = barsFired(1);
        check(every[0] && every[1] && every[2] && every[3], "stride 1 fires on every loop");
        const std::vector<bool> alt = barsFired(2);
        check(alt[0] && !alt[1] && alt[2] && !alt[3],
              "stride 2 fires on loops 0 and 2 but not 1 and 3");
        const std::vector<bool> quarter = barsFired(4);
        check(quarter[0] && !quarter[1] && !quarter[2] && !quarter[3],
              "stride 4 fires only on the first loop of every four");

        audio::Sequencer d;
        check(d.stepStride(0, 0) == 1, "steps default to stride 1 (fire every loop)");
        d.setStepStride(0, 0, 3);
        check(d.stepStride(0, 0) == 3, "step stride setter/getter round-trips");
        d.setStepStride(0, 0, 99);
        check(d.stepStride(0, 0) == 8, "step stride clamps to a max of 8");
    }

    // --- Per-note trig condition (melodic stride) ----------------------------
    {
        // A lead note on step 0 with a stride must fire only every Nth loop, just like drum steps.
        auto melodyBarsFired = [&](int stride) {
            audio::Sequencer s;
            s.setBpm(120.0);
            audio::Note note;
            note.startStep = 0;
            note.lengthSteps = 2;
            note.pitch = 60;
            note.stride = stride;
            s.roll().addNote(note);
            s.play();
            std::vector<bool> fired;
            const int barFrames = 16 * 6000;
            for (int b = 0; b < 4; ++b) {
                const std::vector<float> out = renderMono(s, barFrames, sampleRate);
                double e = 0.0;
                for (int i = 0; i < 6000; ++i) { // the note's first step (its onset)
                    e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
                }
                fired.push_back(e > 1e-4);
            }
            return fired;
        };
        const std::vector<bool> every = melodyBarsFired(1);
        check(every[0] && every[1] && every[2] && every[3], "a stride-1 note fires on every loop");
        const std::vector<bool> alt = melodyBarsFired(2);
        check(alt[0] && !alt[1] && alt[2] && !alt[3],
              "a stride-2 note fires on loops 0 and 2 but not 1 and 3");

        // PianoRoll accessor round-trip + clamp.
        audio::PianoRoll pr;
        pr.addNote(audio::Note{0, 1, 60, 0.9f});
        check(pr.noteStride(60, 0) == 1, "a note defaults to stride 1");
        check(pr.setNoteStride(60, 0, 4) == 4 && pr.noteStride(60, 0) == 4,
              "note stride setter/getter round-trips");
        check(pr.setNoteStride(60, 0, 99) == 8, "note stride clamps to a max of 8");
    }

    // --- Per-pattern transpose (key change) ----------------------------------
    {
        // A held A4 note played through a clean sine lead; the pattern transpose shifts its pitch.
        auto noteHz = [&](int patTranspose) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.synth().setWaveform(audio::Waveform::Sine);
            s.synth().setOscillators(0.0f, 0.0f, 0.0f, 0.0f); // pure tone for a clean pitch estimate
            s.setPatternTranspose(patTranspose);
            audio::Note note;
            note.startStep = 0;
            note.lengthSteps = 16;
            note.pitch = 69; // A4 = 440 Hz
            s.roll().addNote(note);
            s.play();
            const std::vector<float> out = renderMono(s, sampleRate / 4, sampleRate); // 0.25 s
            int crossings = 0;
            float prev = 0.0f;
            for (size_t i = 0; i < out.size(); i += 2) { // left channel
                const float v = out[i];
                if (prev <= 0.0f && v > 0.0f) {
                    ++crossings;
                }
                prev = v;
            }
            const double frames = static_cast<double>(out.size() / 2);
            return static_cast<double>(crossings) * sampleRate / frames;
        };
        const double base = noteHz(0);
        const double up = noteHz(12);
        check(std::fabs(base - 440.0) < 25.0, "pattern transpose 0 plays the note at its pitch (~A4)");
        check(up > base * 1.8, "pattern transpose +12 raises the note about an octave");

        audio::Sequencer d;
        check(d.patternTranspose() == 0, "pattern transpose defaults to 0");
        d.setPatternTranspose(100);
        check(d.patternTranspose() == 48, "pattern transpose clamps to +48");
        d.setPatternTranspose(-100);
        check(d.patternTranspose() == -48, "pattern transpose clamps to -48");
    }

    // --- Trig conditions in song mode: count global loops across the playlist ----
    {
        // Pattern 0 has a stride-2 kick; play it four times via the playlist. The stride counts global
        // pattern loops, so the kick fires on bars 0 and 2 but not 1 and 3 (as in pattern-loop mode).
        audio::Sequencer s;
        s.setBpm(120.0);
        s.setStep(0, 0, true);
        s.setStepStride(0, 0, 2);
        s.setPlaylist({0, 0, 0, 0});
        s.setSongMode(true);
        s.play();
        std::vector<bool> fired;
        const int barFrames = 16 * 6000;
        for (int b = 0; b < 4; ++b) {
            const std::vector<float> out = renderMono(s, barFrames, sampleRate);
            double e = 0.0;
            for (int i = 0; i < 3000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            fired.push_back(e > 1e-4);
        }
        check(fired[0] && !fired[1] && fired[2] && !fired[3],
              "trig conditions count global loops in song mode (stride 2 fires bars 0 and 2)");
    }

    // --- Song-mode pattern change releases held melodic notes (no hang) ----------
    {
        // Pattern 0 holds a full-bar lead note (rings to the bar line); pattern 1 is empty with a
        // different transpose (the guaranteed-hang case). Advancing 0→1 must release the note.
        audio::Sequencer s;
        s.setBpm(120.0);
        audio::Note note;
        note.startStep = 0;
        note.lengthSteps = 16; // ends exactly on the bar line (endStep 0)
        note.pitch = 60;
        s.roll().addNote(note);
        const int emptyPat = s.addPattern();
        s.selectPattern(emptyPat);
        s.setPatternTranspose(2); // different key → a note-off here would be a different pitch
        s.selectPattern(0);
        s.setPlaylist({0, 1});
        s.setSongMode(true);
        s.play();
        const int barFrames = 16 * 6000;
        (void)renderMono(s, barFrames, sampleRate);                          // bar A: the note plays
        const std::vector<float> barB = renderMono(s, barFrames, sampleRate); // bar B: empty pattern
        double tailE = 0.0;
        for (int i = barFrames * 3 / 4; i < barFrames; ++i) { // last quarter of bar B
            tailE += static_cast<double>(barB[static_cast<size_t>(i) * 2]) *
                    static_cast<double>(barB[static_cast<size_t>(i) * 2]);
        }
        check(tailE < 1e-3,
              "advancing to a different pattern releases held notes (no hung/droning note)");
    }

    // --- releaseAllNotes: panic / all-notes-off ------------------------------
    {
        audio::Sequencer s;
        s.synth().noteOn(60, 1.0f);
        s.synth2().noteOn(48, 1.0f);
        check(s.synth().active() && s.synth2().active(), "voices are active after noteOn");
        s.releaseAllNotes();
        (void)renderMono(s, sampleRate, sampleRate); // 1 s >> the release tail
        check(!s.synth().active() && !s.synth2().active(),
              "releaseAllNotes releases every held voice (they decay to inactive)");
    }

    // --- Per-note micro-timing nudge (melodic) -------------------------------
    {
        // A lead note on step 0: with no nudge it sounds immediately; with a 50% nudge its onset is
        // delayed ~half a step, so there is little energy at the very start of the step.
        auto earlyEnergy = [&](int nudge) {
            audio::Sequencer s;
            s.setBpm(120.0);
            audio::Note note;
            note.startStep = 0;
            note.lengthSteps = 4;
            note.pitch = 60;
            note.nudge = nudge;
            s.roll().addNote(note);
            s.play();
            const std::vector<float> out = renderMono(s, 6000, sampleRate); // one 16th step @120 BPM
            double e = 0.0;
            for (int i = 0; i < 1500; ++i) { // first quarter of the step
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };
        const double onGrid = earlyEnergy(0);
        const double nudged = earlyEnergy(50);
        check(onGrid > 1e-4, "an on-grid note sounds at the step start");
        check(nudged < onGrid * 0.1, "a nudged note's onset is delayed (quiet at the step start)");

        // Accessor round-trip + clamp.
        audio::PianoRoll pr;
        pr.addNote(audio::Note{0, 1, 60, 0.9f});
        check(pr.noteNudge(60, 0) == 0, "a note defaults to nudge 0");
        check(pr.setNoteNudge(60, 0, 40) == 40 && pr.noteNudge(60, 0) == 40,
              "note nudge setter/getter round-trips");
        check(pr.setNoteNudge(60, 0, 999) == 95, "note nudge clamps to 95%");
    }

    // --- Groove templates: stamp a per-step micro-timing feel --------------------
    {
        audio::Sequencer s;
        s.setNumSteps(16);
        // Swing 16th: every odd 16th step is nudged late, even steps stay on the grid.
        s.applyGroove(1);
        check(s.stepNudge(0, 0) == 0 && s.stepNudge(0, 2) == 0 && s.stepNudge(0, 4) == 0,
              "swing-16th groove leaves the on-beat 16ths on the grid");
        check(s.stepNudge(0, 1) > 0 && s.stepNudge(0, 3) > 0,
              "swing-16th groove delays the off-beat 16ths");
        // The groove applies to every channel, not just one.
        check(s.stepNudge(2, 1) == s.stepNudge(0, 1) && s.stepNudge(2, 1) > 0,
              "the groove is applied across all channels");
        // Swing 8th nudges the "and" of each beat (step 2, 6, …) but not the odd 16ths.
        s.applyGroove(2);
        check(s.stepNudge(0, 2) > 0 && s.stepNudge(0, 1) == 0,
              "swing-8th groove delays the 8th-note offbeats, not every 16th");
        // Straight clears every nudge back to the grid.
        s.applyGroove(0);
        bool allOnGrid = true;
        for (int c = 0; c < s.numChannels(); ++c) {
            for (int st = 0; st < s.numSteps(); ++st) {
                if (s.stepNudge(c, st) != 0) {
                    allOnGrid = false;
                }
            }
        }
        check(allOnGrid, "the straight groove clears all nudge back to the grid");
        check(std::string(audio::Sequencer::grooveName(1)) == "Swing 16th",
              "groove presets have UI names");

        // Drum micro-timing humanize: random per-step nudges within the cap, deterministic per seed.
        audio::Sequencer h1;
        audio::Sequencer h2;
        h1.humanizeStepTiming(30, 2024u);
        h2.humanizeStepTiming(30, 2024u);
        bool det = true, cap = true, anyNudge = false;
        for (int st = 0; st < h1.numSteps(); ++st) {
            if (h1.stepNudge(0, st) != h2.stepNudge(0, st)) det = false;
            if (h1.stepNudge(0, st) < 0 || h1.stepNudge(0, st) > 30) cap = false;
            if (h1.stepNudge(0, st) > 0) anyNudge = true;
        }
        check(det, "drum timing humanize is deterministic for a given seed");
        check(cap, "drum timing humanize stays within the cap");
        check(anyNudge, "drum timing humanize applies nudges");
        h1.humanizeStepTiming(0, 1u); // cap 0 clears all nudge
        bool cleared = true;
        for (int st = 0; st < h1.numSteps(); ++st) {
            if (h1.stepNudge(0, st) != 0) cleared = false;
        }
        check(cleared, "drum timing humanize with cap 0 clears all nudge");

        // A groove also swings the melodic lanes, not just the drum grid: a lead note on an odd 16th
        // gets the swing nudge, one on an even step stays on the grid.
        audio::Sequencer g;
        g.setNumSteps(16);
        g.roll().addNote(audio::Note{1, 1, 60, 0.9f}); // odd step
        g.roll().addNote(audio::Note{2, 1, 64, 0.9f}); // even step
        g.roll2().addNote(audio::Note{3, 1, 40, 0.9f}); // bass, odd step
        g.applyGroove(1); // Swing 16th
        check(g.roll().noteNudge(60, 1) > 0 && g.roll().noteNudge(64, 2) == 0,
              "a groove swings the lead notes on off-beats but not on-beats");
        check(g.roll2().noteNudge(40, 3) > 0, "a groove swings the bass lane too");
        g.applyGroove(0); // Straight clears melody nudge as well
        check(g.roll().noteNudge(60, 1) == 0 && g.roll2().noteNudge(40, 3) == 0,
              "the straight groove clears the melodic nudge too");
    }

    // --- Count-in: a bar of clicks before the pattern starts ------------------
    {
        // A loud kick on every step; with a 1-bar count-in, the pattern must stay silent (clicks
        // only) for the first bar, then the kick sounds. At 120 BPM a 16-step bar is 96000 frames.
        audio::Sequencer ci;
        ci.setBpm(120.0);
        for (int s = 0; s < ci.numSteps(); ++s) {
            ci.setStep(0, s, true); // kick on every step
        }
        ci.setCountInBars(1);
        ci.play();
        check(ci.countingIn(), "count-in is active right after play()");

        const int bar = 96000;
        const std::vector<float> firstBar = renderMono(ci, bar, sampleRate);
        check(!ci.countingIn(), "count-in ends after one bar");
        const std::vector<float> secondBar = renderMono(ci, bar, sampleRate);

        // Both bars have sound, but bar 2 (kicks + clicks) is far louder than bar 1 (clicks only).
        const double e1 = rms(firstBar);
        const double e2 = rms(secondBar);
        check(e1 > 0.0, "count-in bar plays clicks");
        check(e2 > e1 * 3.0, "the pattern (kicks) only sounds after the count-in");
    }

    // --- Per-bus stems sum back to the mixed render (behaviour preservation) ---
    {
        // One sequencer renders the mixed output; an identically-programmed one renders the three
        // stems, and tanh(drums + lead + bass) must match the mixed render sample-for-sample.
        auto program = [&](audio::Sequencer& s) {
            s.setBpm(128.0);
            s.setStep(0, 0, true);
            s.setStep(2, 2, true);
            s.roll().addNote(audio::Note{0, 4, 60, 0.9f});
            s.roll2().addNote(audio::Note{0, 8, 40, 0.8f});
            s.synth().setEnvelope(0.002f, 0.05f, 0.7f, 0.1f);
            s.synth2().setEnvelope(0.002f, 0.05f, 0.7f, 0.1f);
            s.play();
        };
        const int frames = 12000;
        audio::Sequencer mixed;
        program(mixed);
        std::vector<float> mixOut(static_cast<size_t>(frames) * 2, 0.0f);
        mixed.render(mixOut.data(), frames, sampleRate);

        audio::Sequencer stems;
        program(stems);
        std::vector<float> d(static_cast<size_t>(frames) * 2, 0.0f);
        std::vector<float> l(static_cast<size_t>(frames) * 2, 0.0f);
        std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
        stems.renderStems(d.data(), l.data(), b.data(), frames, sampleRate);

        double maxDiff = 0.0;
        for (size_t i = 0; i < mixOut.size(); ++i) {
            const float summed =
                static_cast<float>(std::tanh(static_cast<double>(d[i]) + static_cast<double>(l[i]) +
                                             static_cast<double>(b[i])));
            maxDiff = std::max(maxDiff, std::fabs(static_cast<double>(mixOut[i] - summed)));
        }
        check(maxDiff < 1e-6, "tanh(drums+lead+bass) stems equal the mixed render");

        // The stems are genuinely separate: drums carry energy, lead carries energy.
        check(rms(d) > 0.0 && rms(l) > 0.0, "drums and lead stems each carry sound");
    }

    // Live MIDI input: a note pushed into the input queue (as a device backend would) plays on the lead
    // instrument and shows up in the lead stem, with no sequence running. A note-off then releases it so
    // the sound decays to silence — proving the live-input routing and gating work end to end.
    {
        const int sr = 48000;
        audio::Sequencer lseq;
        lseq.synth().setEnvelope(0.002f, 0.02f, 0.7f, 0.05f); // fast release so it silences quickly
        const int fr = sr / 10;                                // 0.1 s blocks

        // No play() — nothing is sequenced. Push a live note-on and render: the lead stem must sound.
        lseq.midiInput().pushNoteOn(69, 1.0f); // A4
        std::vector<float> d1(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> l1(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> b1(static_cast<size_t>(fr) * 2, 0.0f);
        lseq.renderStems(d1.data(), l1.data(), b1.data(), fr, sr);
        check(rms(l1) > 0.0, "a live MIDI note-on plays on the lead stem (no sequence running)");
        check(rms(d1) == 0.0, "and does not leak into the drum stem");

        // Note-off, then render long enough for the release to finish: the lead falls silent.
        lseq.midiInput().pushNoteOff(69);
        std::vector<float> d2(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> l2(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> b2(static_cast<size_t>(fr) * 2, 0.0f);
        for (int k = 0; k < 5; ++k) { // 0.5 s of release
            std::fill(l2.begin(), l2.end(), 0.0f);
            std::fill(d2.begin(), d2.end(), 0.0f);
            std::fill(b2.begin(), b2.end(), 0.0f);
            lseq.renderStems(d2.data(), l2.data(), b2.data(), fr, sr);
        }
        check(rms(l2) == 0.0, "a live MIDI note-off releases the note (lead falls silent)");

        // Live input can target an extra channel instead of the lead: route a new channel to the bass
        // bus, aim the live queue at it, and a pushed note now sounds in the bass stem, not the lead.
        const int ch = lseq.addInstrumentChannel();
        lseq.instrumentSynth(ch).setEnvelope(0.002f, 0.02f, 0.7f, 0.05f);
        lseq.setInstrumentBus(ch, 2); // 2 = bass bus
        lseq.setLiveTarget(ch);
        check(lseq.liveTarget() == ch, "the live-input target is the chosen extra channel");
        lseq.midiInput().pushNoteOn(72, 1.0f); // C5
        std::vector<float> d3(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> l3(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> b3(static_cast<size_t>(fr) * 2, 0.0f);
        lseq.renderStems(d3.data(), l3.data(), b3.data(), fr, sr);
        check(rms(b3) > 0.0, "live input on a bass-routed channel sounds in the bass stem");
        check(rms(l3) == 0.0, "and not on the lead stem");
    }

    // Live recording: with the transport running and record armed, a held live note is captured into the
    // target lane's roll (step-quantized), so playing over a loop writes the performance into the pattern.
    {
        const int sr = 48000;
        const int half = sr / 2;
        std::vector<float> rd(static_cast<size_t>(half) * 2, 0.0f);
        std::vector<float> rl(static_cast<size_t>(half) * 2, 0.0f);
        std::vector<float> rb(static_cast<size_t>(half) * 2, 0.0f);

        audio::Sequencer rseq;
        rseq.setBpm(120.0);
        const size_t before = rseq.roll().notes().size();
        rseq.setLiveRecording(true);
        check(rseq.liveRecording(), "live recording arms");
        rseq.play();
        rseq.midiInput().pushNoteOn(64, 0.9f);                 // E4
        rseq.renderStems(rd.data(), rl.data(), rb.data(), half, sr); // hold across several steps
        rseq.midiInput().pushNoteOff(64);
        rseq.renderStems(rd.data(), rl.data(), rb.data(), half, sr);
        check(rseq.roll().notes().size() == before + 1, "live recording writes a note into the roll");
        if (!rseq.roll().notes().empty()) {
            const audio::Note& rec = rseq.roll().notes().back();
            check(rec.pitch == 64, "the recorded note carries the played pitch");
            check(rec.lengthSteps >= 1, "the recorded note has a positive length");
        }

        // With recording off, a live note plays but is NOT written to the roll.
        audio::Sequencer nseq;
        nseq.play();
        const size_t n0 = nseq.roll().notes().size();
        nseq.midiInput().pushNoteOn(64, 0.9f);
        nseq.renderStems(rd.data(), rl.data(), rb.data(), half, sr);
        nseq.midiInput().pushNoteOff(64);
        nseq.renderStems(rd.data(), rl.data(), rb.data(), half, sr);
        check(nseq.roll().notes().size() == n0,
              "with recording off, live notes are not written to the roll");
    }

    // MIDI-learn: a mapped MIDI CC drives its bound parameter live. CC 7 → lead gain: with the gain up
    // a held live note is audible; pushing CC 7 = 0 zeroes the gain and the lead falls silent. A CC
    // bound to the metronome level updates it once drained.
    {
        const int sr = 48000;
        const int fr = sr / 10;
        audio::Sequencer cseq;
        cseq.mapMidiCc(7, audio::Sequencer::CcTarget::LeadGain);
        check(cseq.midiCcTarget(7) == audio::Sequencer::CcTarget::LeadGain,
              "a MIDI CC binds to the lead-gain target");

        cseq.midiInput().pushControlChange(7, 1.0f); // gain full
        cseq.midiInput().pushNoteOn(69, 1.0f);
        std::vector<float> cd(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> cl(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> cb(static_cast<size_t>(fr) * 2, 0.0f);
        cseq.renderStems(cd.data(), cl.data(), cb.data(), fr, sr);
        check(rms(cl) > 0.0, "with a CC-mapped gain up, the live note is audible");

        cseq.midiInput().pushControlChange(7, 0.0f); // gain to zero via CC
        std::fill(cl.begin(), cl.end(), 0.0f);
        std::fill(cd.begin(), cd.end(), 0.0f);
        std::fill(cb.begin(), cb.end(), 0.0f);
        cseq.renderStems(cd.data(), cl.data(), cb.data(), fr, sr);
        check(rms(cl) == 0.0, "a CC that drives the gain to 0 silences the lead live");

        cseq.mapMidiCc(1, audio::Sequencer::CcTarget::MetronomeLevel);
        cseq.midiInput().pushControlChange(1, 0.25f);
        cseq.renderStems(cd.data(), cl.data(), cb.data(), fr, sr); // drains the CC
        check(std::fabs(cseq.metronomeLevel() - 0.25f) < 1e-6f,
              "a CC bound to the metronome level updates it");

        cseq.mapMidiCc(7, audio::Sequencer::CcTarget::None);
        check(cseq.midiCcTarget(7) == audio::Sequencer::CcTarget::None, "a CC can be unbound");

        // CC → lead filter cutoff: the classic live filter sweep. A low CC value darkens the filter
        // (low cutoff), a high value opens it, mapped exponentially — so the cutoff rises with the CC.
        cseq.mapMidiCc(74, audio::Sequencer::CcTarget::LeadCutoff);
        cseq.midiInput().pushControlChange(74, 0.1f);
        cseq.renderStems(cd.data(), cl.data(), cb.data(), fr, sr);
        const float lowCut = cseq.synth().filterCutoff();
        cseq.midiInput().pushControlChange(74, 0.9f);
        cseq.renderStems(cd.data(), cl.data(), cb.data(), fr, sr);
        const float highCut = cseq.synth().filterCutoff();
        check(highCut > lowCut * 4.0f, "a CC mapped to lead cutoff sweeps the filter (up opens it)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
