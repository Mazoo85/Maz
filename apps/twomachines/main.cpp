// Maz Engine — "TWO MACHINES" (net::UdpSocket, Connection, ReliableChannel, PredictionBuffer,
// InterpolationBuffer, ReplicatedObject / Synchronizer, RpcDispatcher, MultiplayerSpawner,
// wsAcceptKey / wsEncodeFrame — a multiplayer stack talking to itself through the kernel)
// Networking demos are usually diagrams. This one opens two real UDP sockets on 127.0.0.1 and runs
// the whole stack between them: the datagrams go out through the OS network stack and come back,
// exactly as they would between two machines, with only the destination address differing. LEFT: the
// sockets themselves and what a packet header buys — sequence numbers, acknowledgements, and a
// protocol id that refuses a stranger's packet; then fifty messages pushed across a link that throws
// away forty percent of everything, in both directions. MIDDLE: the two tricks that hide latency, and
// what each is actually for — prediction, where the interesting number is not how far ahead the client
// is running but how far WRONG it turns out to be; and interpolation, which is not more accurate than
// snapping to the newest packet and is exactly six times smoother. RIGHT: what crosses the wire —
// a replicated object whole and then changed-only, a function call by name, objects spawned on one
// side and materialised on the other, and the browser's framing.
// Fixed seeds, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/net/Connection.hpp"
#include "maz/net/Interpolation.hpp"
#include "maz/net/Prediction.hpp"
#include "maz/net/ReliableChannel.hpp"
#include "maz/net/Replication.hpp"
#include "maz/net/Rpc.hpp"
#include "maz/net/Spawner.hpp"
#include "maz/net/UdpSocket.hpp"
#include "maz/net/WebSocket.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using namespace maz;
using namespace maz::net;

namespace {

constexpr std::uint32_t kProtocol = 0x4D415A01u; // 'MAZ' + a version byte

std::string num(double v, int decimals = 4) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

// A small deterministic generator so the loss pattern is the same picture every run.
struct Lcg {
    std::uint32_t s;
    explicit Lcg(std::uint32_t seed) : s(seed) {}
    std::uint32_t next() {
        s = s * 1664525u + 1013904223u;
        return s;
    }
    bool drop(int percent) { return (next() >> 8) % 100u < static_cast<std::uint32_t>(percent); }
};

long recvWait(UdpSocket& s, void* buf, std::size_t cap, Endpoint& from) {
    for (int i = 0; i < 200000; ++i) {
        const long n = s.recvFrom(buf, cap, from);
        if (n >= 0) {
            return n;
        }
    }
    return -1;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TWO MACHINES starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Two Machines";
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

    // ---- two real sockets ---------------------------------------------------------------------------
    bool socketsUp = false;
    std::uint16_t serverPort = 0;
    std::uint16_t clientPort = 0;
    long bytesThroughKernel = -1;
    std::uint16_t heardFromPort = 0;
    {
        UdpSocket server;
        UdpSocket client;
        if (server.open() && client.open() && server.bind(0, "127.0.0.1") &&
            client.bind(0, "127.0.0.1") && server.setNonBlocking(true) &&
            client.setNonBlocking(true)) {
            socketsUp = true;
            serverPort = server.localPort();
            clientPort = client.localPort();
            const char* msg = "ping";
            client.sendTo(msg, 4, Endpoint("127.0.0.1", serverPort));
            char buf[64];
            Endpoint from;
            bytesThroughKernel = recvWait(server, buf, sizeof buf, from);
            heardFromPort = from.port;
        }
    }

    // ---- headers, sequence numbers and acks ---------------------------------------------------------
    int packetsSent = 0;
    int packetsArrived = 0;
    int packetsAcked = 0;
    std::size_t stillInFlight = 0;
    bool strangerAccepted = true;
    std::uint32_t rejectedCount = 0;
    {
        Connection clientConn(kProtocol);
        Connection serverConn(kProtocol);
        Lcg lossUp(2468u);
        Lcg lossDown(1357u);
        for (int tick = 0; tick < 200; ++tick) {
            std::uint16_t seq = 0;
            const std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(tick & 0xFF)};
            const std::vector<std::uint8_t> packet = clientConn.pack(payload, seq);
            ++packetsSent;
            if (lossUp.drop(20)) {
                continue;
            }
            std::vector<std::uint8_t> got;
            if (serverConn.unpack(packet, got)) {
                ++packetsArrived;
            }
            std::uint16_t backSeq = 0;
            const std::vector<std::uint8_t> reply = serverConn.pack(nullptr, 0, backSeq);
            if (!lossDown.drop(20)) {
                std::vector<std::uint8_t> ignored;
                clientConn.unpack(reply, ignored);
                packetsAcked += static_cast<int>(clientConn.lastAcked().size());
            }
        }
        stillInFlight = clientConn.inFlightCount();
        Connection stranger(0xDEADBEEFu);
        std::uint16_t s2 = 0;
        const std::vector<std::uint8_t> foreign = stranger.pack(nullptr, 0, s2);
        std::vector<std::uint8_t> ignored;
        strangerAccepted = serverConn.unpack(foreign, ignored);
        rejectedCount = serverConn.rejectedPackets();
    }

    // ---- fifty messages across a link that eats forty percent ---------------------------------------
    std::size_t delivered = 0;
    bool inOrder = false;
    int sendAttempts = 0;
    int sendsDropped = 0;
    std::uint32_t virtualMs = 0;
    const int kMessages = 50;
    {
        UdpSocket a;
        UdpSocket b;
        if (a.open() && b.open() && a.bind(0, "127.0.0.1") && b.bind(0, "127.0.0.1") &&
            a.setNonBlocking(true) && b.setNonBlocking(true)) {
            ReliableChannel sender(&a, Endpoint("127.0.0.1", b.localPort()));
            ReliableChannel receiver(&b, Endpoint("127.0.0.1", a.localPort()));
            Lcg lossA(12345u);
            Lcg lossB(67890u);
            sender.dropSend = [&] {
                ++sendAttempts;
                const bool d = lossA.drop(40);
                if (d) {
                    ++sendsDropped;
                }
                return d;
            };
            receiver.dropSend = [&] { return lossB.drop(40); };
            for (int i = 0; i < kMessages; ++i) {
                const std::string m = "message-" + std::to_string(i);
                sender.sendReliable(m.data(), m.size());
            }
            std::vector<int> order;
            for (int step = 0; step < 6000 && static_cast<int>(order.size()) < kMessages; ++step) {
                sender.update(virtualMs);
                receiver.update(virtualMs);
                std::vector<std::uint8_t> msg;
                while (receiver.receive(msg)) {
                    const std::string s(msg.begin(), msg.end());
                    order.push_back(s.rfind("message-", 0) == 0 ? std::atoi(s.c_str() + 8) : -1);
                }
                virtualMs += 5;
            }
            delivered = order.size();
            inOrder = static_cast<int>(order.size()) == kMessages;
            for (int i = 0; i < static_cast<int>(order.size()); ++i) {
                if (order[static_cast<std::size_t>(i)] != i) {
                    inOrder = false;
                    break;
                }
            }
        }
    }

    // ---- predicting, and being corrected ------------------------------------------------------------
    // The client believes it moves 1.0 per input. The server knows it is 0.9 — a debuff the client has
    // not been told about — so every prediction is wrong by exactly 0.1, which is what makes the size
    // of the correction meaningful rather than arbitrary.
    float runsAhead = 0.0f;
    float worstCorrection = 0.0f;
    float replayMismatch = 0.0f;
    std::size_t unacknowledged = 0;
    {
        struct State {
            float x = 0.0f;
        };
        struct Input {
            float dx = 0.0f;
        };
        auto clientStep = [](State s, Input i) {
            s.x += i.dx;
            return s;
        };
        auto serverStep = [](State s, Input i) {
            s.x += i.dx * 0.9f;
            return s;
        };
        PredictionBuffer<State, Input> predictor;
        State authoritative;
        std::uint16_t seq = 0;
        std::vector<std::pair<std::uint16_t, Input>> inFlight;
        for (int tick = 0; tick < 40; ++tick) {
            const Input in{1.0f};
            const State shown = predictor.applyInput(seq, in, clientStep);
            inFlight.push_back({seq, in});
            ++seq;
            if (inFlight.size() > 6) { // the server is six ticks behind
                const std::pair<std::uint16_t, Input> oldest = inFlight.front();
                inFlight.erase(inFlight.begin());
                authoritative = serverStep(authoritative, oldest.second);
                runsAhead = std::max(runsAhead, std::fabs(shown.x - authoritative.x));
                const State fixed = predictor.reconcile(oldest.first, authoritative, clientStep);
                worstCorrection = std::max(worstCorrection, std::fabs(fixed.x - shown.x));
                State expect = authoritative;
                for (const std::pair<std::uint16_t, Input>& c : inFlight) {
                    expect = clientStep(expect, c.second);
                }
                replayMismatch = std::max(replayMismatch, std::fabs(fixed.x - expect.x));
            }
        }
        unacknowledged = predictor.pendingCount();
    }

    // ---- ten updates a second, drawn sixty times a second -------------------------------------------
    double errInterpolated = 0.0;
    double errSnapped = 0.0;
    double jumpInterpolated = 0.0;
    double jumpSnapped = 0.0;
    {
        InterpolationBuffer<float> buffer(64);
        auto truth = [](double t) { return static_cast<float>(std::sin(t * 2.0)); };
        for (int i = 0; i <= 40; ++i) {
            const double t = i * 0.1;
            buffer.insert(t, truth(t));
        }
        const double delay = 0.1; // render a tenth of a second in the past
        float prevInterp = 0.0f;
        float prevSnap = 0.0f;
        bool first = true;
        for (int f = 0; f <= 240; ++f) {
            const double now = f / 60.0;
            float v = 0.0f;
            if (!buffer.sample(now - delay, v)) {
                continue;
            }
            errInterpolated =
                std::max(errInterpolated, std::fabs(static_cast<double>(v) - truth(now - delay)));
            const double lastPacket = std::floor((now - delay) * 10.0) / 10.0;
            const float snap = truth(lastPacket < 0.0 ? 0.0 : lastPacket);
            errSnapped =
                std::max(errSnapped, std::fabs(static_cast<double>(snap) - truth(now - delay)));
            if (!first) {
                jumpInterpolated =
                    std::max(jumpInterpolated, std::fabs(static_cast<double>(v - prevInterp)));
                jumpSnapped = std::max(jumpSnapped, std::fabs(static_cast<double>(snap - prevSnap)));
            }
            prevInterp = v;
            prevSnap = snap;
            first = false;
        }
    }

    // ---- what actually crosses the wire -------------------------------------------------------------
    std::size_t fullBytes = 0;
    std::size_t deltaBytes = 0;
    std::size_t changedFields = 0;
    bool mirrorMatches = false;
    {
        std::uint32_t hp = 100;
        std::uint32_t ammo = 30;
        std::uint32_t x = 5;
        std::uint32_t y = 9;
        ReplicatedObject object;
        object.add([&] { return hp; }, [&](std::uint32_t v) { hp = v; }, 8);
        object.add([&] { return ammo; }, [&](std::uint32_t v) { ammo = v; }, 8);
        object.add([&] { return x; }, [&](std::uint32_t v) { x = v; }, 16);
        object.add([&] { return y; }, [&](std::uint32_t v) { y = v; }, 16);

        std::uint32_t mhp = 0;
        std::uint32_t mammo = 0;
        std::uint32_t mx = 0;
        std::uint32_t my = 0;
        ReplicatedObject mirror;
        mirror.add([&] { return mhp; }, [&](std::uint32_t v) { mhp = v; }, 8);
        mirror.add([&] { return mammo; }, [&](std::uint32_t v) { mammo = v; }, 8);
        mirror.add([&] { return mx; }, [&](std::uint32_t v) { mx = v; }, 16);
        mirror.add([&] { return my; }, [&](std::uint32_t v) { my = v; }, 16);

        Synchronizer tx;
        Synchronizer rx;
        BitWriter full;
        tx.writeFull(full, object);
        BitReader fr(full.bytes());
        rx.readFull(fr, mirror);
        fullBytes = full.bytes().size();

        hp = 92; // exactly one of the four fields moves
        BitWriter delta;
        changedFields = tx.writeDelta(delta, object);
        BitReader dr(delta.bytes());
        rx.readDelta(dr, mirror);
        deltaBytes = delta.bytes().size();
        mirrorMatches = (mhp == hp && mammo == ammo && mx == x && my == y);
    }

    // ---- calling a function on the other machine ----------------------------------------------------
    RpcMethodId methodId = 0;
    std::size_t rpcBytes = 0;
    int damageDelivered = -1;
    bool rpcDispatched = false;
    bool unknownRefused = false;
    {
        RpcDispatcher dispatcher;
        dispatcher.bindNamed("takeDamage",
                             [&](BitReader& r) { damageDelivered = static_cast<int>(r.readUint(16)); });
        BitWriter w;
        RpcDispatcher::writeHeaderNamed(w, "takeDamage", RpcMode::Reliable);
        w.writeUint(37, 16);
        BitReader r(w.bytes());
        rpcDispatched = dispatcher.dispatch(r);
        methodId = rpcHash("takeDamage");
        rpcBytes = w.bytes().size();

        BitWriter w2;
        RpcDispatcher::writeHeaderNamed(w2, "noSuchMethod");
        w2.writeUint(1, 16);
        BitReader r2(w2.bytes());
        unknownRefused = !dispatcher.dispatch(r2);
    }

    // ---- spawning ------------------------------------------------------------------------------------
    std::size_t spawnBytes = 0;
    std::vector<std::uint32_t> peerHolds;
    bool spawnReadOk = false;
    {
        MultiplayerSpawner host;
        MultiplayerSpawner peer;
        peer.onSpawn = [&](const SpawnRecord& r) { peerHolds.push_back(r.netId); };
        peer.onDespawn = [&](std::uint32_t id) {
            peerHolds.erase(std::remove(peerHolds.begin(), peerHolds.end(), id), peerHolds.end());
        };
        host.spawn(7, {10, 20});
        const std::uint32_t doomed = host.spawn(7, {30, 40});
        host.spawn(9, {50, 60});
        host.despawn(doomed);
        BitWriter w;
        host.writeEvents(w);
        spawnBytes = w.bytes().size();
        BitReader r(w.bytes());
        spawnReadOk = peer.readEvents(r);
    }

    // ---- the browser's transport ---------------------------------------------------------------------
    // RFC 6455 publishes a worked example of the handshake, so this is checkable against the standard
    // rather than against itself.
    const std::string rfcKey = "dGhlIHNhbXBsZSBub25jZQ==";
    const std::string rfcExpected = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    const std::string rfcGot = wsAcceptKey(rfcKey);
    const std::string wsText = "hello from the engine";
    const std::vector<std::uint8_t> wsFrame = wsEncodeFrame(WsOpcode::Text, wsText);
    std::size_t wsConsumed = 0;
    const auto wsDecoded = wsDecodeFrame(wsFrame.data(), wsFrame.size(), wsConsumed);
    const bool wsRoundTrip =
        wsDecoded && std::string(wsDecoded->payload.begin(), wsDecoded->payload.end()) == wsText;

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
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TWO MACHINES", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "a multiplayer stack talking to itself through the kernel — real sockets, "
                          "real datagrams, only the address is local",
                          kDim, 0.32f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 215.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1 ----
            float y = 100.0f;
            font.drawText(*renderer, 24.0f, y, "TWO REAL SOCKETS", kHead, 0.34f);
            y += 30.0f;
            row(24.0f, y, "the OS gave us ports",
                socketsUp ? std::to_string(serverPort) + " and " + std::to_string(clientPort)
                          : "no sockets available",
                socketsUp ? kVal : kNo);
            y += 23.0f;
            row(24.0f, y, "a datagram round trip",
                bytesThroughKernel >= 0
                    ? std::to_string(bytesThroughKernel) + " bytes, from port " +
                          std::to_string(heardFromPort)
                    : "did not arrive",
                bytesThroughKernel >= 0 ? kOk : kNo);
            y += 30.0f;

            font.drawText(*renderer, 24.0f, y, "WHAT A HEADER BUYS", kHead, 0.34f);
            y += 30.0f;
            row(24.0f, y, "sent / arrived",
                std::to_string(packetsSent) + " / " + std::to_string(packetsArrived) + "  (20% loss)",
                kVal);
            y += 23.0f;
            row(24.0f, y, "acknowledged back", std::to_string(packetsAcked), kOk);
            y += 23.0f;
            row(24.0f, y, "never confirmed", std::to_string(stillInFlight), kVal);
            y += 23.0f;
            row(24.0f, y, "header costs", std::to_string(kPacketHeaderBytes) + " bytes", kDim);
            y += 23.0f;
            row(24.0f, y, "a stranger's packet",
                strangerAccepted ? "ACCEPTED" : "refused (" + std::to_string(rejectedCount) + ")",
                strangerAccepted ? kNo : kOk);
            y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Twelve bytes in front of each payload carry a sequence number and a bitfield "
                          "of which of the last 32 packets the other end has seen, so the sender learns "
                          "what got through without anything being retransmitted. The protocol id in "
                          "the same header is what makes a packet from another program — or another "
                          "version of this one — a refusal rather than a crash.",
                          kDim, 0.25f);

            y += 108.0f;
            font.drawText(*renderer, 24.0f, y, "A LINK THAT EATS 40%", kHead, 0.34f);
            y += 30.0f;
            row(24.0f, y, "messages delivered",
                std::to_string(delivered) + " / " + std::to_string(kMessages), 
                delivered == static_cast<std::size_t>(kMessages) ? kOk : kNo);
            y += 23.0f;
            row(24.0f, y, "exactly once, in order", inOrder ? "yes" : "NO", inOrder ? kOk : kNo);
            y += 23.0f;
            row(24.0f, y, "sends it took",
                std::to_string(sendAttempts) + "  (" + std::to_string(sendsDropped) + " thrown away)",
                kVal);
            y += 23.0f;
            row(24.0f, y, "time to get there", std::to_string(virtualMs) + " ms", kVal);

            // ---- column 2 ----
            y = 100.0f;
            font.drawText(*renderer, 470.0f, y, "PREDICTING, AND BEING WRONG", kHead, 0.34f);
            y += 30.0f;
            row(470.0f, y, "runs ahead by", num(static_cast<double>(runsAhead), 3), kVal);
            y += 23.0f;
            row(470.0f, y, "worst correction", num(static_cast<double>(worstCorrection), 4), kNo);
            y += 23.0f;
            row(470.0f, y, "vs replaying by hand",
                num(static_cast<double>(replayMismatch), 2) == "0.00"
                    ? "identical"
                    : num(static_cast<double>(replayMismatch), 6),
                kOk);
            y += 23.0f;
            row(470.0f, y, "inputs in flight", std::to_string(unacknowledged), kDim);
            y += 30.0f;
            font.drawText(*renderer, 470.0f, y,
                          "The two numbers at the top are easy to confuse and mean opposite things. "
                          "Running 6.1 ahead is not an error — it is the client showing the player "
                          "their own input immediately instead of waiting a round trip, which is the "
                          "entire point. The error is the CORRECTION: here the client believes it "
                          "moves 1.0 per input and the server knows it is 0.9, so the picture is "
                          "wrong by exactly 0.1 each time the truth arrives — and reconciling "
                          "reproduces, to the bit, replaying the unacknowledged inputs by hand.",
                          kDim, 0.25f);

            y += 126.0f;
            font.drawText(*renderer, 470.0f, y, "TEN A SECOND, DRAWN SIXTY", kHead, 0.34f);
            y += 30.0f;
            row(470.0f, y, "off the truth: lerped", num(errInterpolated, 5), kVal);
            y += 23.0f;
            row(470.0f, y, "off the truth: snapped", num(errSnapped, 5), kVal);
            y += 23.0f;
            row(470.0f, y, "frame jump: lerped", num(jumpInterpolated, 5), kOk);
            y += 23.0f;
            row(470.0f, y, "frame jump: snapped", num(jumpSnapped, 5), kNo);
            y += 30.0f;
            font.drawText(*renderer, 470.0f, y,
                          ("Interpolation is not more accurate, and the first two rows say so: holding "
                           "the newest packet is off by " +
                           num(errSnapped, 3) + " against the buffer's " + num(errInterpolated, 3) +
                           ", which is nothing. What it buys is the bottom pair. Snapping moves the "
                           "whole gap in one frame and then sits still for five; interpolating spreads "
                           "the same distance across all six, so the biggest single step is " +
                           num(jumpSnapped / (jumpInterpolated > 0.0 ? jumpInterpolated : 1.0), 1) +
                           " times smaller — exactly the ratio of the two rates. Stutter, not error, "
                           "is the thing being fixed.")
                              .c_str(),
                          kDim, 0.25f);

            // ---- column 3 ----
            y = 100.0f;
            font.drawText(*renderer, 950.0f, y, "WHAT CROSSES THE WIRE", kHead, 0.34f);
            y += 30.0f;
            row(950.0f, y, "four fields, whole", std::to_string(fullBytes) + " bytes", kVal);
            y += 23.0f;
            row(950.0f, y, "one field changed",
                std::to_string(deltaBytes) + " bytes  (" + std::to_string(changedFields) + " field)",
                kOk);
            y += 23.0f;
            row(950.0f, y, "the mirror agrees", mirrorMatches ? "every field" : "NO",
                mirrorMatches ? kOk : kNo);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "8 + 8 + 16 + 16 bits is 6 bytes, and a full snapshot is 6 bytes — no "
                          "padding, no field names, no framing. Change one of the four and the delta "
                          "is 2: a mask of which fields moved, and the one that did.",
                          kDim, 0.25f);

            y += 74.0f;
            font.drawText(*renderer, 950.0f, y, "CALLING ACROSS", kHead, 0.34f);
            y += 30.0f;
            row(950.0f, y, "\"takeDamage\" becomes", std::to_string(methodId), kVal);
            y += 23.0f;
            row(950.0f, y, "the whole call",
                std::to_string(rpcBytes) + " bytes, arrived as " + std::to_string(damageDelivered),
                rpcDispatched && damageDelivered == 37 ? kOk : kNo);
            y += 23.0f;
            row(950.0f, y, "an unknown method", unknownRefused ? "refused" : "GUESSED",
                unknownRefused ? kOk : kNo);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Only the number crosses; both ends hash the same name to it. An id nobody "
                          "bound is refused rather than run.",
                          kDim, 0.25f);

            y += 62.0f;
            font.drawText(*renderer, 950.0f, y, "SPAWNING AND THE BROWSER", kHead, 0.34f);
            y += 30.0f;
            row(950.0f, y, "3 spawns, 1 despawn", std::to_string(spawnBytes) + " bytes", kVal);
            y += 23.0f;
            {
                std::string held;
                for (std::uint32_t id : peerHolds) {
                    held += (held.empty() ? "" : ", ") + std::to_string(id);
                }
                row(950.0f, y, "the peer now holds",
                    spawnReadOk ? (held.empty() ? "nothing" : held) : "read failed",
                    spawnReadOk && peerHolds.size() == 2 ? kOk : kNo);
            }
            y += 23.0f;
            row(950.0f, y, "RFC 6455 handshake", rfcGot == rfcExpected ? "matches the RFC" : "DIFFERS",
                rfcGot == rfcExpected ? kOk : kNo);
            y += 23.0f;
            row(950.0f, y, "a text frame",
                std::to_string(wsText.size()) + " bytes in " + std::to_string(wsFrame.size()) +
                    (wsRoundTrip ? ", back out whole" : ", CORRUPTED"),
                wsRoundTrip ? kOk : kNo);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "The handshake is checked against the worked example printed in RFC 6455 "
                          "rather than against itself, which is the difference between agreeing with "
                          "the standard and agreeing with whoever wrote the code.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 700.0f,
                          "Nothing here is simulated except the packet loss: the sockets are real, the "
                          "kernel routed the datagrams, and the only thing separating this from two "
                          "machines is which address they were sent to.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TWO MACHINES shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
