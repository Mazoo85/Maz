// Maz Engine — "WIRE" (net::BitStream, net::FloatQuant, net::QuatCompress, net::Snapshot,
// net::NetSim, net::ClockSync — what actually goes over the network, toward Godot's
// MultiplayerSynchronizer)
// Six modules that together answer one question: how few bits can describe this player, and what happens
// to them on the way. LEFT: the same entity written three ways — as a plain struct, as quantised fields
// packed bit by bit, and as a delta against the last snapshot — with the byte count each produces and
// what the receiver got back, so the error a quantiser introduces is shown rather than promised.
// MIDDLE: a rotation through net::compressQuat, where dropping the largest component and sending the
// other three costs 29 bits instead of 128. RIGHT: 600 packets through a simulated link — 60ms each way
// plus jitter, 8% loss — and a clock synchronised across it from four timestamps. One seed, no input.
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the six are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/net/BitStream.hpp"
#include "maz/net/ClockSync.hpp"
#include "maz/net/FloatQuant.hpp"
#include "maz/net/NetSim.hpp"
#include "maz/net/QuatCompress.hpp"
#include "maz/net/Snapshot.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 3) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("WIRE starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Wire";
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

    // ---- 1. One player, three ways to describe them ----------------------------------------------------
    // Position inside a 512m world, a facing angle, health and a couple of flags. The naive wire format
    // sends every field at full width; the engine's sends each at the width it actually needs.
    const float posX = 137.482f, posY = 8.914f, posZ = -64.203f;
    const float facing = 2.41f;
    const int health = 73;
    const bool crouching = true, firing = false;

    const std::size_t naiveBytes = sizeof(float) * 4 + sizeof(int) + sizeof(bool) * 2;

    // Quantised: 16 bits per axis over a 512m span is 8mm of precision, 9 bits of angle is 0.7 degrees,
    // 7 bits covers 0..127 health, 1 bit each for the flags.
    const int posBits = 16, angleBits = 9, healthBits = 7;
    net::BitWriter w;
    w.writeUint(net::quantizeFloat(posX, -256.0f, 256.0f, posBits), posBits);
    w.writeUint(net::quantizeFloat(posY, -256.0f, 256.0f, posBits), posBits);
    w.writeUint(net::quantizeFloat(posZ, -256.0f, 256.0f, posBits), posBits);
    w.writeUint(net::quantizeAngle(facing, angleBits), angleBits);
    w.writeUint(static_cast<std::uint32_t>(health), healthBits);
    w.writeBool(crouching);
    w.writeBool(firing);
    const std::size_t packedBits = w.bitCount();
    const std::size_t packedBytes = w.bytes().size();

    // Read it back and measure what the precision actually cost.
    net::BitReader r(w.bytes());
    const float gotX = net::dequantizeFloat(r.readUint(posBits), -256.0f, 256.0f, posBits);
    const float gotY = net::dequantizeFloat(r.readUint(posBits), -256.0f, 256.0f, posBits);
    const float gotZ = net::dequantizeFloat(r.readUint(posBits), -256.0f, 256.0f, posBits);
    const float gotFacing = net::dequantizeAngle(r.readUint(angleBits), angleBits);
    const int gotHealth = static_cast<int>(r.readUint(healthBits));
    const bool gotCrouching = r.readBool();
    const bool gotFiring = r.readBool();

    const double posError = std::sqrt(static_cast<double>((gotX - posX) * (gotX - posX) +
                                                          (gotY - posY) * (gotY - posY) +
                                                          (gotZ - posZ) * (gotZ - posZ)));
    const double angleErrorDeg = std::abs(gotFacing - facing) * 180.0 / 3.14159265358979;
    const bool exactRest = gotHealth == health && gotCrouching == crouching && gotFiring == firing;

    // ---- 2. A delta against the last snapshot -----------------------------------------------------------
    // The usual case: almost nothing changed since the last packet. A delta sends one bit per unchanged
    // field instead of the field.
    const std::vector<net::FieldSpec> schema = {{16}, {16}, {16}, {9}, {7}, {1}, {1}};
    const std::vector<std::uint32_t> baseline = {
        net::quantizeFloat(posX, -256.0f, 256.0f, 16), net::quantizeFloat(posY, -256.0f, 256.0f, 16),
        net::quantizeFloat(posZ, -256.0f, 256.0f, 16), net::quantizeAngle(facing, 9),
        static_cast<std::uint32_t>(health), 1u, 0u};
    std::vector<std::uint32_t> moved = baseline;
    moved[0] = net::quantizeFloat(posX + 0.4f, -256.0f, 256.0f, 16);   // a step forward
    moved[3] = net::quantizeAngle(facing + 0.05f, 9);                   // a small turn

    net::BitWriter fullW;
    net::writeSnapshotFull(fullW, schema, moved);
    net::BitWriter deltaW;
    net::writeSnapshotDelta(deltaW, schema, baseline, moved);
    const std::size_t fullSnapBits = fullW.bitCount();
    const std::size_t deltaBits = deltaW.bitCount();
    const std::size_t changed = net::countChanged(baseline, moved);

    net::BitReader deltaR(deltaW.bytes());
    const std::vector<std::uint32_t> rebuilt = net::readSnapshotDelta(deltaR, schema, baseline);
    const bool deltaOk = rebuilt == moved;

    // ---- 3. A rotation in 29 bits ------------------------------------------------------------------------
    // The smallest-three trick: the largest component is recoverable from the other three, so it is not
    // sent. Two bits say which one it was.
    math::quat turn = glm::normalize(math::quat(0.3f, 0.51f, -0.2f, 0.78f));
    const std::uint32_t packedQuat = net::compressQuat(turn, 9);
    const math::quat back = net::decompressQuat(packedQuat, 9);
    const int quatBits = net::compressedQuatBits(9);
    const double quatDot = std::abs(static_cast<double>(glm::dot(turn, back)));
    const double quatErrorDeg = 2.0 * std::acos(quatDot > 1.0 ? 1.0 : quatDot) * 180.0 /
                                3.14159265358979;

    // ---- 4. The link itself ------------------------------------------------------------------------------
    net::NetConditions link;
    link.latency = 0.06;        // 60ms one way
    link.jitter = 0.02;
    link.lossChance = 0.08f;
    link.dupChance = 0.01f;
    net::NetSim sim(2024u);
    sim.setConditions(link);

    const int sent = 600;
    int arrived = 0;
    double t = 0.0;
    for (int i = 0; i < sent; ++i) {
        sim.send(w.bytes(), t);
        t += 1.0 / 60.0;
        arrived += static_cast<int>(sim.receive(t).size());
    }
    // Drain whatever is still in flight.
    t += 1.0;
    arrived += static_cast<int>(sim.receive(t).size());
    const double lossSeen = 100.0 * static_cast<double>(sim.dropped()) / static_cast<double>(sent);
    const double bytesPerSecond = static_cast<double>(packedBytes) * 60.0;
    const double naivePerSecond = static_cast<double>(naiveBytes) * 60.0;

    // ---- 5. Agreeing what time it is ---------------------------------------------------------------------
    // Four timestamps per exchange, the way NTP does it. The client's clock is 3.5s fast and the link
    // is 60ms each way; ClockSync has to find both from the round trips alone.
    net::ClockSync sync(8);
    const double trueOffset = -3.5;      // server time = client time + offset
    for (int i = 0; i < 12; ++i) {
        const double t0 = 100.0 + static_cast<double>(i);            // client sends
        const double t1 = t0 + trueOffset + 0.060;                   // server receives
        const double t2 = t1 + 0.002;                                // server replies
        const double t3 = t2 - trueOffset + 0.060;                   // client receives
        sync.addSample(t0, t1, t2, t3);
    }
    const double foundOffset = sync.offset();
    const double foundRtt = sync.rtt() * 1000.0;
    const double offsetError = std::abs(foundOffset - trueOffset) * 1000.0;

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

            const float sz = 0.29f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  WIRE", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "How few bits describe a player, and what the link does to them", kDim, 0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 225.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1: three ways to send one player ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "ONE PLAYER, THREE WAYS", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "as a plain struct", std::to_string(naiveBytes) + " bytes", kNo); y += 25.0f;
            row(24.0f, y, "quantised + packed", std::to_string(packedBytes) + " bytes  (" +
                              std::to_string(packedBits) + " bits)", kOk); y += 25.0f;
            row(24.0f, y, "as a delta", std::to_string((deltaBits + 7) / 8) + " bytes  (" +
                              std::to_string(deltaBits) + " bits)", kOk); y += 30.0f;
            row(24.0f, y, "full snapshot, for scale", std::to_string(fullSnapBits) + " bits", kDim);
            y += 25.0f;
            row(24.0f, y, "fields changed", std::to_string(changed) + " of " +
                              std::to_string(schema.size()), kVal); y += 25.0f;
            row(24.0f, y, "delta rebuilds exactly", deltaOk ? "yes" : "NO", deltaOk ? kOk : kNo);
            y += 32.0f;
            font.drawText(*renderer, 24.0f, y,
                          "a delta spends one bit saying \"unchanged\" instead of the field", kDim,
                          0.26f);

            y += 40.0f;
            font.drawText(*renderer, 24.0f, y, "WHAT THE PRECISION COST", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "position error", num(posError * 1000.0, 1) + " mm", kOk); y += 25.0f;
            row(24.0f, y, "facing error", num(angleErrorDeg, 2) + " degrees", kOk); y += 25.0f;
            row(24.0f, y, "health and flags", exactRest ? "exact" : "WRONG", exactRest ? kOk : kNo);
            y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "16 bits across 512m is 8mm — smaller than the player notices", kDim, 0.26f);

            // ---- column 2: the rotation ----
            y = 106.0f;
            font.drawText(*renderer, 610.0f, y, "net::compressQuat  -  A ROTATION", kHead, 0.35f);
            y += 34.0f;
            row(610.0f, y, "four floats", "128 bits", kNo); y += 25.0f;
            row(610.0f, y, "smallest-three", std::to_string(quatBits) + " bits", kOk); y += 25.0f;
            row(610.0f, y, "angle error", num(quatErrorDeg, 3) + " degrees", kOk); y += 30.0f;
            font.drawText(*renderer, 610.0f, y,
                          "a unit quaternion's largest component is recoverable", kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 610.0f, y,
                          "from the other three, so it is not sent — two bits", kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 610.0f, y, "say which one it was", kDim, 0.26f);

            y += 40.0f;
            font.drawText(*renderer, 610.0f, y, "WHAT THAT IS WORTH", kHead, 0.35f);
            y += 34.0f;
            row(610.0f, y, "naive, 60 times a second", num(naivePerSecond / 1024.0, 1) + " KB/s",
                kNo); y += 25.0f;
            row(610.0f, y, "packed, same rate", num(bytesPerSecond / 1024.0, 1) + " KB/s", kOk);
            y += 25.0f;
            row(610.0f, y, "for 32 players", num(bytesPerSecond * 32.0 / 1024.0, 1) + " KB/s", kVal);
            y += 30.0f;
            font.drawText(*renderer, 610.0f, y,
                          ("the same 32 players naively: " + num(naivePerSecond * 32.0 / 1024.0, 1) +
                           " KB/s").c_str(), kDim, 0.26f);

            // ---- column 3: the link ----
            y = 106.0f;
            font.drawText(*renderer, 1090.0f, y, "net::NetSim  -  A BAD LINK", kHead, 0.35f);
            y += 34.0f;
            row(1090.0f, y, "latency", "60 ms +/- 20", kText); y += 25.0f;
            row(1090.0f, y, "loss asked for", "8%", kText); y += 25.0f;
            row(1090.0f, y, "packets sent", std::to_string(sent), kText); y += 25.0f;
            row(1090.0f, y, "dropped", std::to_string(sim.dropped()) + "  (" + num(lossSeen, 1) + "%)",
                kNo); y += 25.0f;
            row(1090.0f, y, "duplicated", std::to_string(sim.duplicated()), kVal); y += 25.0f;
            row(1090.0f, y, "arrived", std::to_string(arrived), kOk); y += 30.0f;
            font.drawText(*renderer, 1090.0f, y, "jitter reorders as well as delays", kDim, 0.26f);

            y += 40.0f;
            font.drawText(*renderer, 1090.0f, y, "net::ClockSync", kHead, 0.35f);
            y += 34.0f;
            row(1090.0f, y, "clocks really differ by", num(trueOffset, 3) + " s", kText); y += 25.0f;
            row(1090.0f, y, "found", num(foundOffset, 3) + " s", kVal); y += 25.0f;
            row(1090.0f, y, "off by", num(offsetError, 2) + " ms", offsetError < 5.0 ? kOk : kNo);
            y += 25.0f;
            row(1090.0f, y, "round trip found", num(foundRtt, 1) + " ms", kVal); y += 30.0f;
            font.drawText(*renderer, 1090.0f, y, "four timestamps per exchange, the", kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 1090.0f, y, "way NTP does it", kDim, 0.26f);

            font.drawText(*renderer, 24.0f, 664.0f,
                          "Every size and every error above is measured by writing the packet and "
                          "reading it back, not quoted from the documentation.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("WIRE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
