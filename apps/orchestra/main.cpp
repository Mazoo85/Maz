// Maz Engine — "ORCHESTRA" (audio::scaleNotes / chordNotes, Oscillator, ADSR, computeSpatialMix,
// BusGraph, stereoMid / stereoSide / fromMidSide, StreamRandomizer — making sound, where apps/earshot
// listened to it)
// apps/earshot pointed the engine's analysers at signals and asked what they heard. This is the other
// half: the engine building the signal in the first place, and then — because both halves exist — handing
// it straight back to the analyser to see whether it comes out as what went in. LEFT: the notes of a
// scale and a chord, which are right or wrong against music itself; then an oscillator asked for an A
// and a pitch detector asked what it just made. Beside it, the shape of a note as a key is pressed and
// let go. MIDDLE: where a sound is in the world — the pan, the fall-off and the doppler shift, each with
// an answer you can derive. RIGHT: a mixing desk in decibels, a stereo field taken apart into its middle
// and its sides and put back, and the rule that stops a footstep sound playing twice in a row.
// Fixed seeds, no audio device, no soundcard. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 698 headers.
#include "maz/audio/BusGraph.hpp"
#include "maz/audio/Envelope.hpp"
#include "maz/audio/MusicScales.hpp"
#include "maz/audio/MusicTheory.hpp"
#include "maz/audio/Oscillator.hpp"
#include "maz/audio/PitchDetect.hpp"
#include "maz/audio/Randomizer.hpp"
#include "maz/audio/Spatial3D.hpp"
#include "maz/audio/Stereo.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kSampleRate = 48000;

std::string num(double v, int decimals = 3) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string signed_(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.*f", decimals, v);
    return buf;
}

std::string noteList(const std::vector<int>& midi) {
    std::string out;
    for (int n : midi) {
        out += (out.empty() ? "" : " ");
        out += audio::midiToNoteName(n);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ORCHESTRA starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Orchestra";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- notes, which are right or wrong against music itself ---------------------------------------
    struct NoteRow {
        std::string what;
        std::string notes;
    };
    std::vector<NoteRow> theory;
    theory.push_back({"C major", noteList(audio::scaleNotes(60, audio::Scale::Major))});
    theory.push_back({"C natural minor", noteList(audio::scaleNotes(60, audio::Scale::NaturalMinor))});
    theory.push_back({"C minor pentatonic",
                      noteList(audio::scaleNotes(60, audio::Scale::MinorPentatonic))});
    theory.push_back({"C major chord", noteList(audio::chordNotes(60, audio::Chord::Major))});
    theory.push_back({"C minor chord", noteList(audio::chordNotes(60, audio::Chord::Minor))});
    theory.push_back({"C dominant 7th", noteList(audio::chordNotes(60, audio::Chord::Dominant7))});
    theory.push_back({"C diminished", noteList(audio::chordNotes(60, audio::Chord::Diminished))});

    // ---- an oscillator, handed straight to a pitch detector ------------------------------------------
    const double askedFor = static_cast<double>(audio::noteNameToFrequency("A4"));
    double heard = 0.0;
    double centsOff = 0.0;
    {
        audio::Oscillator osc;
        osc.freq = static_cast<float>(askedFor);
        osc.sampleRate = static_cast<float>(kSampleRate);
        osc.waveform = audio::Waveform::Sine;
        std::vector<float> buffer(static_cast<std::size_t>(kSampleRate) / 2);
        for (float& s : buffer) {
            s = osc.next();
        }
        const audio::PitchResult p =
            audio::detectPitchYin(buffer, static_cast<float>(kSampleRate));
        heard = static_cast<double>(p.frequency);
        centsOff = 1200.0 * std::log2(heard / askedFor);
    }

    // Each waveform's root-mean-square has a value you can look up — for an IDEAL wave with infinitely
    // sharp edges. This engine's edges are deliberately not sharp, and the table shows where that costs.
    struct WaveRow {
        std::string name;
        double peak = 0.0;
        double rms = 0.0;
        double textbook = 0.0;
        bool bandLimited = false;
    };
    std::vector<WaveRow> waves;
    {
        struct Spec {
            audio::Waveform w;
            const char* name;
            double exact;
            bool blep;
        };
        const Spec specs[] = {{audio::Waveform::Sine, "sine", 0.70710678, false},
                              {audio::Waveform::Triangle, "triangle", 0.57735027, false},
                              {audio::Waveform::Square, "square", 1.0, true},
                              {audio::Waveform::Saw, "saw", 0.57735027, true}};
        for (const Spec& s : specs) {
            audio::Oscillator o;
            o.freq = 100.0f;
            o.sampleRate = static_cast<float>(kSampleRate);
            o.waveform = s.w;
            double acc = 0.0;
            float peak = 0.0f;
            for (int i = 0; i < kSampleRate; ++i) {
                const float v = o.next();
                acc += static_cast<double>(v) * v;
                peak = std::max(peak, std::fabs(v));
            }
            waves.push_back(WaveRow{s.name, static_cast<double>(peak),
                                    std::sqrt(acc / static_cast<double>(kSampleRate)), s.exact,
                                    s.blep});
        }
    }

    // ---- the shape of a note --------------------------------------------------------------------------
    struct EnvelopePoint {
        std::string when;
        double level = 0.0;
        std::string expected;
    };
    std::vector<EnvelopePoint> envelope;
    {
        audio::ADSR env;
        env.attack = 0.05f;
        env.decay = 0.10f;
        env.sustain = 0.60f;
        env.release = 0.20f;
        env.noteOn();
        const float dt = 1.0f / static_cast<float>(kSampleRate);
        auto run = [&](double seconds) {
            const int n = static_cast<int>(seconds * kSampleRate);
            float v = 0.0f;
            for (int i = 0; i < n; ++i) {
                v = env.process(dt);
            }
            return static_cast<double>(v);
        };
        envelope.push_back({"the attack ends", run(0.05), "1"});
        envelope.push_back({"the decay ends", run(0.10), "0.6"});
        envelope.push_back({"still held, 0.5s in", run(0.35), "0.6"});
        env.noteOff();
        envelope.push_back({"half through release", run(0.10), "0.3"});
        envelope.push_back({"released", run(0.11), "0"});
    }

    // ---- where the sound is ---------------------------------------------------------------------------
    struct Place {
        std::string where;
        audio::SpatialMix mix;
    };
    std::vector<Place> places;
    double dopplerPitch = 1.0;
    double dopplerExpected = 1.0;
    {
        audio::Listener3D listener;
        audio::SpatialConfig conf;
        conf.refDistance = 1.0f;
        conf.maxDistance = 100.0f;
        conf.doppler = false;
        struct Spot {
            const char* where;
            float x;
            float z;
        };
        const Spot spots[] = {{"1 m straight ahead", 0.0f, -1.0f},
                              {"1 m to the right", 1.0f, 0.0f},
                              {"1 m to the left", -1.0f, 0.0f},
                              {"10 m ahead", 0.0f, -10.0f},
                              {"100 m ahead", 0.0f, -100.0f}};
        for (const Spot& s : spots) {
            audio::Source3D src;
            src.pos = math::vec3(s.x, 0.0f, s.z);
            places.push_back(Place{s.where, audio::computeSpatialMix(listener, src, conf)});
        }
        audio::Source3D rushing;
        rushing.pos = math::vec3(0.0f, 0.0f, -10.0f);
        rushing.velocity = math::vec3(0.0f, 0.0f, 30.0f); // closing at 30 m/s
        audio::SpatialConfig withDoppler = conf;
        withDoppler.doppler = true;
        dopplerPitch =
            static_cast<double>(audio::computeSpatialMix(listener, rushing, withDoppler).pitch);
        dopplerExpected = static_cast<double>(withDoppler.speedOfSound) /
                          (static_cast<double>(withDoppler.speedOfSound) - 30.0);
    }

    // ---- the mixing desk ------------------------------------------------------------------------------
    struct DeskRow {
        std::string what;
        double master = 0.0;
    };
    std::vector<DeskRow> desk;
    const double minusSixDb = std::pow(10.0, -6.0 / 20.0);
    {
        audio::BusGraph graph;
        const int music = graph.addBus("Music", 0);
        const int sfx = graph.addBus("Sfx", 0);
        graph.setVolumeDb(music, -6.0f);
        auto push = [&](float m, float s) {
            graph.pushInput(music, m);
            graph.pushInput(sfx, s);
            return static_cast<double>(graph.process());
        };
        desk.push_back({"music alone", push(1.0f, 0.0f)});
        desk.push_back({"sfx alone", push(0.0f, 1.0f)});
        desk.push_back({"both together", push(1.0f, 1.0f)});
        graph.setMute(music, true);
        desk.push_back({"both, music muted", push(1.0f, 1.0f)});
        graph.setMute(music, false);
        graph.setSolo(sfx, true);
        desk.push_back({"both, sfx soloed", push(1.0f, 1.0f)});
    }

    // ---- taking the stereo field apart and putting it back --------------------------------------------
    double midSideWorst = 0.0;
    int midSidePairs = 0;
    {
        for (float l : {-1.0f, -0.3f, 0.0f, 0.5f, 1.0f}) {
            for (float r : {-1.0f, -0.3f, 0.0f, 0.5f, 1.0f}) {
                const audio::StereoFrame f{l, r};
                const audio::StereoFrame back =
                    audio::fromMidSide(audio::stereoMid(f), audio::stereoSide(f));
                midSideWorst = std::max(midSideWorst,
                                        static_cast<double>(std::max(std::fabs(back.left - l),
                                                                     std::fabs(back.right - r))));
                ++midSidePairs;
            }
        }
    }

    // ---- never the same footstep twice ----------------------------------------------------------------
    int repeatsWithRule = 0;
    int repeatsWithout = 0;
    std::string firstTwelve;
    const int kPicks = 2000;
    {
        audio::StreamRandomizer guarded;
        for (int i = 0; i < 4; ++i) {
            guarded.addStream();
        }
        guarded.mode = audio::RandomizerMode::RandomNoRepeat;
        guarded.setSeed(2026u);
        int last = -1;
        for (int i = 0; i < kPicks; ++i) {
            const audio::RandomPick p = guarded.next();
            if (i < 12) {
                firstTwelve += std::to_string(p.index);
            }
            if (p.index == last) {
                ++repeatsWithRule;
            }
            last = p.index;
        }
        audio::StreamRandomizer plain;
        for (int i = 0; i < 4; ++i) {
            plain.addStream();
        }
        plain.mode = audio::RandomizerMode::Random;
        plain.setSeed(2026u);
        last = -1;
        for (int i = 0; i < kPicks; ++i) {
            const audio::RandomPick p = plain.next();
            if (p.index == last) {
                ++repeatsWithout;
            }
            last = p.index;
        }
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kNo{1.0f, 0.48f, 0.42f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.28f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ORCHESTRA", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "making the sound, where earshot listened to it — and handing what it made "
                          "straight back to the listener",
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };

            // ---- column 1 ----
            float y = 100.0f;
            font.drawText(*renderer, 24.0f, y, "NOTES, WHICH ARE SIMPLY RIGHT OR WRONG", kHead, 0.32f);
            y += 28.0f;
            for (const NoteRow& t : theory) {
                cell(24.0f, y, t.what, kDim, sz);
                cell(210.0f, y, t.notes, kVal, sz);
                y += 22.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Nothing here is a matter of taste: a C major scale is C D E F G A B or it is "
                          "not, and a dominant seventh is the major chord with a flattened seventh on "
                          "top. It is the rare corner of an engine that can be checked against "
                          "something older than the engine.",
                          kDim, 0.25f);

            y += 70.0f;
            font.drawText(*renderer, 24.0f, y, "AN A, MADE AND THEN HEARD", kHead, 0.32f);
            y += 28.0f;
            cell(24.0f, y, "asked the oscillator for", kDim, sz);
            cell(250.0f, y, num(askedFor, 2) + " Hz", kVal, sz);
            y += 22.0f;
            cell(24.0f, y, "the detector reports", kDim, sz);
            cell(250.0f, y, num(heard, 2) + " Hz", kVal, sz);
            y += 22.0f;
            cell(24.0f, y, "difference", kDim, sz);
            cell(250.0f, y, signed_(centsOff) + " cents",
                 std::fabs(centsOff) < 1.0 ? kOk : kNo, sz);
            y += 26.0f;
            cell(24.0f, y, "waveform", kDim, 0.25f);
            cell(150.0f, y, "peak", kDim, 0.25f);
            cell(215.0f, y, "rms", kDim, 0.25f);
            cell(280.0f, y, "textbook", kDim, 0.25f);
            cell(370.0f, y, "off by", kDim, 0.25f);
            y += 21.0f;
            for (const WaveRow& w : waves) {
                const double err = 100.0 * std::fabs(w.rms - w.textbook) / w.textbook;
                cell(24.0f, y, w.name, kText, sz);
                cell(150.0f, y, num(w.peak, 3), kDim, sz);
                cell(215.0f, y, num(w.rms, 4), kVal, sz);
                cell(280.0f, y, num(w.textbook, 4), kDim, sz);
                cell(370.0f, y, num(err, 2) + "%", err < 0.01 ? kOk : kVal, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The sine and the triangle land on the textbook number. The square "
                          "and the saw do not, and that is the anti-aliasing working rather than "
                          "failing: those two are the waveforms with a vertical edge, and the engine "
                          "rounds every edge over two samples so it does not scream at frequencies the "
                          "sample rate cannot carry. A rounded edge holds slightly less energy, so the "
                          "measured level falls a fraction short of an ideal wave that no one can "
                          "actually play.",
                          kDim, 0.25f);

            // ---- column 2 ----
            y = 100.0f;
            font.drawText(*renderer, 500.0f, y, "PRESSING AND LETTING GO", kHead, 0.32f);
            y += 28.0f;
            cell(500.0f, y, "attack 0.05s, decay 0.10s, sustain 0.6, release 0.20s", kDim, 0.25f);
            y += 24.0f;
            for (const EnvelopePoint& e : envelope) {
                cell(500.0f, y, e.when, kText, sz);
                cell(710.0f, y, num(e.level, 4), kVal, sz);
                cell(800.0f, y, "should be " + e.expected, kDim, 0.24f);
                y += 22.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 500.0f, y,
                          "Held, it does not drift. Let go half way through the release, it is at half "
                          "the sustain level — the fall is linear, so the arithmetic is visible.",
                          kDim, 0.25f);

            y += 60.0f;
            font.drawText(*renderer, 500.0f, y, "WHERE THE SOUND IS", kHead, 0.32f);
            y += 28.0f;
            cell(500.0f, y, "position", kDim, 0.25f);
            cell(690.0f, y, "pan", kDim, 0.25f);
            cell(760.0f, y, "left", kDim, 0.25f);
            cell(830.0f, y, "right", kDim, 0.25f);
            y += 21.0f;
            for (const Place& p : places) {
                cell(500.0f, y, p.where, kText, sz);
                cell(690.0f, y, signed_(static_cast<double>(p.mix.pan)), kVal, sz);
                cell(760.0f, y, num(static_cast<double>(p.mix.left)), kVal, sz);
                cell(830.0f, y, num(static_cast<double>(p.mix.right)), kVal, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(500.0f, y,
                 "closing at 30 m/s: pitch x" + num(dopplerPitch, 4) + "   (343/(343-30) = " +
                     num(dopplerExpected, 4) + ")",
                 std::fabs(dopplerPitch - dopplerExpected) < 1e-3 ? kOk : kNo, 0.25f);
            y += 26.0f;
            font.drawText(*renderer, 500.0f, y,
                          "Dead ahead gives 0.707 to each side, not 0.5 — squared and added they make "
                          "one, so a sound does not get quieter as it crosses in front of you, which "
                          "it would with the obvious half-and-half. Ten metres is a tenth the gain and "
                          "a hundred is a hundredth: the inverse law, straight. And the doppler shift "
                          "is the schoolbook ratio with no fudge in it.",
                          kDim, 0.25f);

            // ---- column 3 ----
            y = 100.0f;
            font.drawText(*renderer, 950.0f, y, "THE MIXING DESK", kHead, 0.32f);
            y += 28.0f;
            cell(950.0f, y, "music bus at -6 dB, sfx at 0, master at 0", kDim, 0.25f);
            y += 24.0f;
            for (const DeskRow& d : desk) {
                cell(950.0f, y, d.what, kText, sz);
                cell(1180.0f, y, num(d.master, 4), kVal, sz);
                y += 22.0f;
            }
            y += 6.0f;
            cell(950.0f, y, "-6 dB really is " + num(minusSixDb, 4), kOk, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Decibels are not a slider label here: turning the music down six lands on "
                          "0.5012, which is what ten to the minus six twentieths is. Mute takes a bus "
                          "out; solo takes everything else out; both come back to the same number from "
                          "opposite directions.",
                          kDim, 0.25f);

            y += 76.0f;
            font.drawText(*renderer, 950.0f, y, "THE MIDDLE AND THE SIDES", kHead, 0.32f);
            y += 28.0f;
            cell(950.0f, y,
                 std::to_string(midSidePairs) + " stereo pairs, split and rejoined", kText, sz);
            y += 22.0f;
            cell(950.0f, y, "worst difference " + num(midSideWorst * 1e8, 1) + " x 10^-8",
                 midSideWorst < 1e-6 ? kOk : kNo, sz);
            y += 24.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Not bit-identical, and it should not be — halving and re-adding costs a "
                          "rounding step. It comes back within three hundred-millionths, far below "
                          "anything a sixteen-bit output could carry.",
                          kDim, 0.25f);

            y += 66.0f;
            font.drawText(*renderer, 950.0f, y, "NOT THE SAME FOOTSTEP TWICE", kHead, 0.32f);
            y += 28.0f;
            cell(950.0f, y, "4 sounds, " + std::to_string(kPicks) + " picks", kDim, 0.25f);
            y += 22.0f;
            cell(950.0f, y, "with the no-repeat rule", kText, sz);
            cell(1210.0f, y, std::to_string(repeatsWithRule) + " repeats",
                 repeatsWithRule == 0 ? kOk : kNo, sz);
            y += 22.0f;
            cell(950.0f, y, "without it", kText, sz);
            cell(1210.0f, y, std::to_string(repeatsWithout) + " repeats", kVal, sz);
            y += 22.0f;
            cell(950.0f, y, "first twelve: " + firstTwelve, kDim, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 950.0f, y,
                          ("Plain randomness repeats " +
                           num(100.0 * repeatsWithout / kPicks, 1) +
                           "% of the time with four sounds, which is one in four, which is exactly what "
                           "chance should do — and is also precisely what a player hears as a glitch. "
                           "The rule costs nothing and removes all of them.")
                              .c_str(),
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 702.0f,
                          "No device was opened and nothing was played. Every figure came out of a "
                          "float buffer, which is why a pitch, a fall-off curve and a mixer law can "
                          "all be regression-tested on a machine with no speakers.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ORCHESTRA shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
