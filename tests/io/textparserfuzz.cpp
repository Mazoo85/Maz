// Malformed-input robustness fuzz for the text/structured parsers that consume untrusted documents:
// io::parseJson, io::XmlParser, io::base64Decode, render::parsePly, render::parseObj, render::parseMtl.
// A game loads these as levels, scenes, config, and models — a corrupt or hostile file must never crash the
// process. Existing tests feed only well-formed input; this feeds random buffers, truncations/mutations of
// valid seeds, and structural stressors (deeply nested JSON that would overflow the stack, a PLY header
// declaring an absurd element count). A parser may reject bad input by returning false/empty — it just must
// never overflow the stack, read out of bounds, OOM, or invoke UB. Built under ASan+UBSan (the sanitizer
// CI job runs it). This harness found and now guards two real bugs: unbounded JSON recursion (stack
// overflow) and an unbounded PLY reserve (OOM).
#include "maz/io/Json.hpp"
#include "maz/io/Xml.hpp"
#include "maz/io/Base64.hpp"
#include "maz/render/PlyLoader.hpp"
#include "maz/render/ObjLoader.hpp"
#include "maz/render/MtlLoader.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feedJson(const std::string& s) {
    auto r = io::parseJson(s);
    g_sink += r.ok ? 1u : 0u;
}
static void feedXml(const std::string& s) {
    io::XmlParser p;
    if (!p.parse(s)) return;
    int guard = 0;
    while (p.read() && ++guard < 100000) g_sink += 1;
}
static void feedBase64(const std::string& s) {
    std::vector<std::uint8_t> out;
    if (io::base64Decode(s, out)) g_sink += out.size();
}
static void feedPly(const std::string& s) {
    render::shapes::MeshData m;
    if (render::parsePly(s, m)) g_sink += m.vertices.size() + m.indices.size();
}
static void feedObj(const std::string& s) {
    render::shapes::MeshData m;
    if (render::parseObj(s, m)) g_sink += m.vertices.size();
}
static void feedMtl(const std::string& s) {
    std::vector<render::MtlMaterial> m;
    if (render::parseMtl(s, m)) g_sink += m.size();
}

using StrFn = void (*)(const std::string&);

static void truncateAndMutate(StrFn fn, const std::string& seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) fn(seed.substr(0, k));
    std::string s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char orig = s[i];
        for (char p : {'\0', '\xff', '<', '>', '"', '\\', '{', '['}) {
            s[i] = p;
            fn(s);
        }
        s[i] = orig;
    }
}

int main() {
    // --- structural stressors: these are the shapes that crashed before the fixes ---
    feedJson(std::string(300000, '['));                        // unbounded-recursion stack overflow (fixed)
    { std::string s(4000, '['); s.append(4000, ']'); feedJson(s); }
    feedPly("ply\nformat ascii 1.0\nelement vertex 2000000000\n"
            "property float x\nproperty float y\nproperty float z\nend_header\n0 0 0\n"); // PLY OOM (fixed)
    feedXml(std::string(100000, '<'));                         // pathological tag soup

    // --- truncation + mutation of small valid seeds ---
    truncateAndMutate(feedJson, "{\"a\":[1,2,{\"b\":true,\"c\":null}],\"d\":\"x\\n\"}");
    truncateAndMutate(feedXml, "<root a=\"1\"><child>text</child><!--c--><![CDATA[x]]></root>");
    truncateAndMutate(feedBase64, "SGVsbG8sIFdvcmxkIQ==");
    truncateAndMutate(feedPly, "ply\nformat ascii 1.0\nelement vertex 2\nproperty float x\nproperty float y\n"
                               "property float z\nelement face 0\nproperty list uchar int vertex_indices\n"
                               "end_header\n0 0 0\n1 1 1\n");
    truncateAndMutate(feedObj, "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1//1 2//1 3//1\n");
    truncateAndMutate(feedMtl, "newmtl m\nKa 0 0 0\nKd 1 1 1\nNs 32\nmap_Kd t.png\n");

    // --- random buffers through every parser (deterministic PRNG) ---
    StrFn all[] = {feedJson, feedXml, feedBase64, feedPly, feedObj, feedMtl};
    std::mt19937 rng(0xF0F0u);
    for (int iter = 0; iter < 30000; ++iter) {
        const std::size_t n = rng() % 257u;
        std::string s(n, '\0');
        for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<char>(rng() & 0xff);
        for (StrFn fn : all) fn(s);
    }

    std::printf("textparser fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
