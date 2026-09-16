// Maz Engine — "INTERCHANGE" (io::msgpackEncode, XmlParser, ConfigFile, StreamPeerBuffer,
// base64Encode, lzCompress, gunzip, TranslationTable, PoCatalog — the formats data leaves the
// process in, and comes back from)
// apps/squeeze already covers the codecs that make bytes smaller. These are the ones that make bytes
// PORTABLE: a save file something else can open, a settings file a person can edit, a packet that
// survives a text channel, a catalogue somebody translated last year. LEFT: one player record written
// six ways, with what each costs and every one read back — the sizes are the argument, and they are
// not close. MIDDLE: base64, whose length is exact arithmetic rather than a rule of thumb, checked at
// eight sizes; then three gzip streams this engine never made, produced by Python, decoded here — and
// a fourth with one byte of its checksum flipped, which must be REFUSED rather than quietly returning
// almost the right data. RIGHT: the same two strings in two catalogues whose only difference is the
// plural rule in the header, which is how you tell a parsed rule from an assumed one.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/io/Base64.hpp"
#include "maz/io/Compression.hpp"
#include "maz/io/ConfigFile.hpp"
#include "maz/io/GettextPo.hpp"
#include "maz/io/Gzip.hpp"
#include "maz/io/Localization.hpp"
#include "maz/io/MessagePack.hpp"
#include "maz/io/StreamPeer.hpp"
#include "maz/io/Xml.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using namespace maz;
using namespace maz::io;

namespace {

std::string num(double v, int decimals = 0) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

// Hex text to bytes, so a real gzip stream can be written down in the source.
std::vector<std::uint8_t> fromHex(const char* h) {
    auto nibble = [](char c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
    std::vector<std::uint8_t> v;
    for (std::size_t i = 0; h[i] != '\0' && h[i + 1] != '\0'; i += 2) {
        v.push_back(static_cast<std::uint8_t>((nibble(h[i]) << 4) | nibble(h[i + 1])));
    }
    return v;
}

struct Written {
    std::string format;
    std::size_t bytes = 0;
    std::string readBack; // what came back out, or why it did not
    bool ok = false;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("INTERCHANGE starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Interchange";
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

    // ---- one player record, written six ways --------------------------------------------------------
    const std::string playerName = "Wren";
    const int playerLevel = 7;
    const float playerHp = 63.5f;
    const float posX = 12.25f;
    const float posY = 0.0f;
    const float posZ = -8.75f;

    std::vector<Written> written;
    std::vector<std::uint8_t> packed;
    {
        std::vector<std::pair<MsgValue, MsgValue>> fields;
        fields.push_back({MsgValue::str("name"), MsgValue::str(playerName)});
        fields.push_back({MsgValue::str("level"), MsgValue::integer(playerLevel)});
        fields.push_back({MsgValue::str("hp"), MsgValue::number(playerHp)});
        fields.push_back({MsgValue::str("pos"),
                          MsgValue::array({MsgValue::number(posX), MsgValue::number(posY),
                                           MsgValue::number(posZ)})});
        fields.push_back({MsgValue::str("items"),
                          MsgValue::array({MsgValue::str("lamp"), MsgValue::str("rope")})});
        packed = msgpackEncode(MsgValue::map(fields));
        MsgValue back;
        const bool ok = msgpackDecode(packed, back) && back.type() == MsgValue::Type::Map &&
                        back.asMap().size() == 5;
        written.push_back(Written{"MessagePack", packed.size(),
                                  ok ? "5 fields back" : "did not decode", ok});
    }

    const std::string xml =
        "<save><name>Wren</name><level>7</level><hp>63.5</hp>"
        "<pos x=\"12.25\" y=\"0\" z=\"-8.75\"/>"
        "<items><item>lamp</item><item>rope</item></items></save>";
    {
        XmlParser parser;
        int nodes = 0;
        float readX = 0.0f;
        if (parser.parse(xml)) {
            while (parser.read()) {
                ++nodes;
                if (parser.nodeName() == "pos" && parser.hasAttribute("x")) {
                    readX = static_cast<float>(std::atof(parser.attributeValue(0).c_str()));
                }
            }
        }
        const bool ok = nodes > 0 && readX == posX;
        written.push_back(Written{"XML", xml.size(),
                                  std::to_string(nodes) + " nodes, x=" + num(static_cast<double>(readX), 2),
                                  ok});
    }

    std::string ini;
    {
        ConfigFile out;
        out.setValue("player", "name", playerName);
        out.setInt("player", "level", playerLevel);
        out.setFloat("player", "hp", static_cast<double>(playerHp));
        out.setFloat("pos", "x", static_cast<double>(posX));
        out.setFloat("pos", "y", static_cast<double>(posY));
        out.setFloat("pos", "z", static_cast<double>(posZ));
        out.setValue("items", "0", "lamp");
        out.setValue("items", "1", "rope");
        ini = out.encode();
        ConfigFile back;
        const bool parsed = back.parse(ini);
        const bool ok = parsed && back.getInt("player", "level", 0) == playerLevel;
        written.push_back(Written{"ConfigFile (ini)", ini.size(),
                                  ok ? "level " + std::to_string(back.getInt("player", "level", 0)) +
                                           ", hp " + num(back.getFloat("player", "hp", 0.0), 1)
                                     : "did not parse",
                                  ok});
    }

    {
        StreamPeerBuffer out;
        out.putString(playerName);
        out.put32(playerLevel);
        out.putFloat(playerHp);
        out.putFloat(posX);
        out.putFloat(posY);
        out.putFloat(posZ);
        out.putU8(2);
        out.putString("lamp");
        out.putString("rope");
        StreamPeerBuffer back;
        back.dataArray() = out.dataArray();
        back.seek(0);
        const std::string n = back.getString();
        const int lvl = back.get32();
        const float hp = back.getFloat();
        const bool ok = (n == playerName && lvl == playerLevel && hp == playerHp);
        written.push_back(Written{"StreamPeer (raw)", out.dataArray().size(),
                                  ok ? "\"" + n + "\", level " + std::to_string(lvl) : "mismatch", ok});
    }

    std::size_t base64Bytes = 0;
    {
        const std::string encoded = base64Encode(packed);
        std::vector<std::uint8_t> back;
        const bool ok = base64Decode(encoded, back) && back == packed;
        base64Bytes = encoded.size();
        written.push_back(Written{"...as base64", encoded.size(),
                                  ok ? "identical bytes back" : "corrupted", ok});
    }
    {
        const std::vector<std::uint8_t> squeezed = lzCompress(packed);
        const bool ok = lzDecompress(squeezed) == packed;
        written.push_back(Written{"...lz compressed", squeezed.size(),
                                  ok ? "identical bytes back" : "corrupted", ok});
    }
    const std::size_t xmlSqueezed = lzCompress(std::vector<std::uint8_t>(xml.begin(), xml.end())).size();

    // ---- base64's length is arithmetic ---------------------------------------------------------------
    struct SizeCheck {
        std::size_t in = 0;
        std::size_t out = 0;
        std::size_t formula = 0;
    };
    std::vector<SizeCheck> sizes;
    bool base64Exact = true;
    for (std::size_t n : {1u, 2u, 3u, 4u, 100u, 1000u}) {
        const std::vector<std::uint8_t> data(n, 0x41);
        const std::size_t out = base64Encode(data).size();
        const std::size_t formula = 4 * ((n + 2) / 3);
        sizes.push_back(SizeCheck{n, out, formula});
        if (out != formula) {
            base64Exact = false;
        }
    }

    // ---- gzip streams this engine never made ---------------------------------------------------------
    // Produced by Python's gzip.compress. Decoding somebody else's bytes is the only test of a format
    // reader that means anything; a round trip against your own writer agrees with itself by
    // construction.
    struct Foreign {
        std::string what;
        std::string got;
        bool ok = false;
    };
    std::vector<Foreign> foreign;
    {
        std::vector<std::uint8_t> out;
        const bool a = gunzip(
            fromHex("1f8b0800000000000203cb48cdc9c9d751c840a1caf38b7252140183891f6e1b000000"), out);
        foreign.push_back(Foreign{"a sentence", a ? std::string(out.begin(), out.end()) : "failed", a});
        const bool b = gunzip(fromHex("1f8b080000000000020373741c05c402002333a0bb2c010000"), out);
        foreign.push_back(Foreign{"300 bytes of one letter",
                                  b ? std::to_string(out.size()) + " x '" +
                                          std::string(1, out.empty() ? '?' : static_cast<char>(out[0])) + "'"
                                    : "failed",
                                  b});
        const bool c = gunzip(fromHex("1f8b08080000000002ff666f6f2e74787400cb4bcc4d4d512848acccc94f4c51c848"
                                      "2d4a0500cc2de9e912000000"),
                              out);
        foreign.push_back(
            Foreign{"one carrying a filename", c ? std::string(out.begin(), out.end()) : "failed", c});
        std::vector<std::uint8_t> broken =
            fromHex("1f8b0800000000000203cb48cdc9c9d751c840a1caf38b7252140183891f6e1b000000");
        broken[broken.size() - 8] ^= 0xff; // flip a byte of the stored CRC-32
        const bool d = gunzip(broken, out);
        foreign.push_back(Foreign{"the first, checksum broken",
                                  d ? "ACCEPTED IT" : "refused, as it must", !d});
    }

    // ---- the same strings, two plural rules -----------------------------------------------------------
    std::string csvGreeting;
    std::string csvFarewell;
    std::string csvMissing;
    {
        TranslationTable table;
        table.loadCsv("key,en,fr\ngreeting,Hello,Bonjour\nfarewell,Goodbye,Au revoir\n");
        table.setLocale("fr");
        csvGreeting = table.tr("greeting");
        csvFarewell = table.tr("farewell");
        csvMissing = table.tr("no_such_key");
    }
    struct Plural {
        long n = 0;
        std::string looseRule;  // n != 1
        std::string tightRule;  // n > 1
    };
    std::vector<Plural> plurals;
    std::string doorPhrase;
    std::string untranslated;
    {
        auto catalogue = [](const char* rule) {
            PoCatalog po;
            const std::string src =
                std::string("msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=") + rule +
                ";\\n\"\n\n"
                "msgid \"Open door\"\nmsgstr \"Ouvrir la porte\"\n\n"
                "msgid \"%d coin\"\nmsgid_plural \"%d coins\"\n"
                "msgstr[0] \"%d piece\"\nmsgstr[1] \"%d pieces\"\n";
            po.parse(src);
            return po;
        };
        const PoCatalog loose = catalogue("n != 1");
        const PoCatalog tight = catalogue("n > 1");
        doorPhrase = loose.gettext("Open door");
        untranslated = loose.gettext("Close door");
        for (long n : {0, 1, 2, 5}) {
            plurals.push_back(Plural{n, loose.ngettext("%d coin", "%d coins", n),
                                     tight.ngettext("%d coin", "%d coins", n)});
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
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  INTERCHANGE", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "the formats data leaves the process in — and the only test of a reader that "
                          "means anything: somebody else's bytes",
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 205.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1 ----
            float y = 100.0f;
            font.drawText(*renderer, 24.0f, y, "ONE SAVE RECORD, SIX WAYS", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "name, level, hit points, a position, two items", kDim, 0.25f);
            y += 24.0f;
            cell(24.0f, y, "format", kDim, 0.25f);
            cell(190.0f, y, "bytes", kDim, 0.25f);
            cell(255.0f, y, "read back", kDim, 0.25f);
            y += 22.0f;
            for (const Written& w : written) {
                cell(24.0f, y, w.format, kText, sz);
                cell(190.0f, y, std::to_string(w.bytes), kVal, sz);
                cell(255.0f, y, w.readBack, w.ok ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 10.0f;
            font.drawText(*renderer, 24.0f, y,
                          ("Every one of them survives the trip, so the only question left is price, "
                           "and the spread is over three to one. StreamPeer is smallest because it "
                           "stores no field names at all — position IS the schema, which is perfect "
                           "for a packet and unreadable in six months. XML costs the most and is the "
                           "only one a person can edit by hand without a tool. MessagePack sits "
                           "between: named fields, binary values. Base64 adds " +
                           num(100.0 * (static_cast<double>(base64Bytes) /
                                            static_cast<double>(written[0].bytes) - 1.0)) +
                           "% for the privilege of surviving a text channel, and compressing the "
                           "MessagePack barely pays — it is already dense. The XML squeezes to " +
                           num(100.0 * static_cast<double>(xmlSqueezed) / static_cast<double>(xml.size())) +
                           "%, because tag names repeat and that is exactly what a compressor eats.")
                              .c_str(),
                          kDim, 0.25f);

            // ---- column 2 ----
            y = 100.0f;
            font.drawText(*renderer, 470.0f, y, "BASE64 IS ARITHMETIC", kHead, 0.34f);
            y += 28.0f;
            cell(470.0f, y, "bytes in", kDim, 0.25f);
            cell(570.0f, y, "chars out", kDim, 0.25f);
            cell(680.0f, y, "4 x ceil(n/3)", kDim, 0.25f);
            y += 22.0f;
            for (const SizeCheck& s : sizes) {
                cell(470.0f, y, std::to_string(s.in), kText, sz);
                cell(570.0f, y, std::to_string(s.out), kVal, sz);
                cell(680.0f, y, std::to_string(s.formula), s.out == s.formula ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 470.0f, y,
                          base64Exact
                              ? "Not a rule of thumb: three bytes become four characters, and a short "
                                "tail is padded to four anyway. That is why the overhead is a third at "
                                "the limit but more on anything small."
                              : "A size did not match the formula — something is wrong.",
                          kDim, 0.25f);

            y += 82.0f;
            font.drawText(*renderer, 470.0f, y, "BYTES THIS ENGINE NEVER MADE", kHead, 0.34f);
            y += 28.0f;
            cell(470.0f, y, "gzip streams produced by Python", kDim, 0.25f);
            y += 24.0f;
            for (const Foreign& f : foreign) {
                row(470.0f, y, f.what.c_str(), f.got, f.ok ? kOk : kNo);
                y += 23.0f;
            }
            y += 10.0f;
            font.drawText(*renderer, 470.0f, y,
                          "A round trip against your own writer proves only that the code agrees with "
                          "itself. These four were made elsewhere: a plain stream, one long enough to "
                          "need back-references, one carrying a stored filename the header has to be "
                          "stepped over — and one with a byte of its checksum flipped. The last is the "
                          "important row. Its DEFLATE data still decodes perfectly; only the CRC "
                          "disagrees, so a reader that skipped the check would hand back plausible "
                          "bytes and never say a word.",
                          kDim, 0.25f);

            // ---- column 3 ----
            y = 100.0f;
            font.drawText(*renderer, 950.0f, y, "A TABLE OF TRANSLATIONS", kHead, 0.34f);
            y += 28.0f;
            row(950.0f, y, "greeting, in French", csvGreeting, kVal);
            y += 23.0f;
            row(950.0f, y, "farewell, in French", csvFarewell, kVal);
            y += 23.0f;
            row(950.0f, y, "a key nobody wrote", csvMissing, csvMissing.empty() ? kNo : kOk);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "The last row is the one that matters in a shipped game: an untranslated key "
                          "comes back as the key, not as empty space. A blank label is a bug report; a "
                          "visible key is a job ticket.",
                          kDim, 0.25f);

            y += 76.0f;
            font.drawText(*renderer, 950.0f, y, "AND THE RULE FOR PLURALS", kHead, 0.34f);
            y += 28.0f;
            row(950.0f, y, "a plain string", doorPhrase, kVal);
            y += 23.0f;
            row(950.0f, y, "one nobody translated", untranslated, kOk);
            y += 26.0f;
            cell(950.0f, y, "n", kDim, 0.25f);
            cell(990.0f, y, "rule \"n != 1\"", kDim, 0.25f);
            cell(1120.0f, y, "rule \"n > 1\"", kDim, 0.25f);
            y += 22.0f;
            for (const Plural& p : plurals) {
                cell(950.0f, y, std::to_string(p.n), kText, sz);
                cell(990.0f, y, p.looseRule, kVal, sz);
                cell(1120.0f, y, p.tightRule, p.looseRule == p.tightRule ? kVal : kOk, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Two catalogues holding the same two strings, differing only in the plural "
                          "expression written in their header. They agree about one, two and five, and "
                          "disagree about ZERO — which is the whole test. A reader that assumed "
                          "English would give the same answer twice; this one parses the expression "
                          "and evaluates it, so a language that counts differently counts differently.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 700.0f,
                          "Nothing here touches a disk or a socket: every format is encoded into a "
                          "buffer and decoded out of one, which is why the whole panel runs in a unit "
                          "test — and why the gzip row can be somebody else's bytes pasted into the "
                          "source.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("INTERCHANGE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
