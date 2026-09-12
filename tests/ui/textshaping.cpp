// tests/ui/textshaping.cpp — verifies the OpenType-style shaper (ui::shapeGlyphs / shapeCodepoints).
// Ground truths, all pure CPU integer/float math, deterministic:
//   * ligature substitution collapses a matching component run to one glyph and sums to its advance;
//   * longest ligature match wins when several rules could apply at the same position;
//   * clusters are merged (the ligature inherits the earliest source index) so text after a ligature
//     still maps to its true character index;
//   * pair kerning adjusts the advance between two specific adjacent base glyphs, and only them;
//   * a combining mark carries zero advance and is offset so its anchor meets the base's anchor
//     (the mark-to-base positioning math is checked against a hand-computed offset);
//   * a run with no applicable rules passes through unchanged (advances preserved);
//   * shapedWidth sums advances (marks contribute nothing).
#include "maz/ui/TextShaping.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::ui::LigatureRule;
using maz::ui::ShapedGlyph;
using maz::ui::ShapeAnchor;
using maz::ui::ShapingTable;
using maz::ui::shapeGlyphs;
using maz::ui::shapedWidth;

static bool nearf(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// Glyph ids used across the tests (Latin letters as their code points, plus synthetic ligature/mark ids).
enum : uint32_t {
    F = 'f', I = 'i', A = 'A', V = 'V', X = 'x', BASE_A = 'a',
    LIG_FI = 1000, LIG_FII = 1002, MARK_ACUTE = 2000
};

static ShapingTable makeTable() {
    ShapingTable t;
    t.advance[F] = 6.0f;
    t.advance[I] = 4.0f;
    t.advance[A] = 10.0f;
    t.advance[V] = 10.0f;
    t.advance[X] = 5.0f;
    t.advance[BASE_A] = 7.0f;
    t.advance[LIG_FI] = 8.0f;
    t.advance[LIG_FII] = 11.0f;
    t.advance[MARK_ACUTE] = 0.0f;

    // GSUB: f i -> fi, and the longer f i i -> ffii-style ligature (to test longest match).
    t.ligatures.push_back(LigatureRule{{F, I}, LIG_FI});
    t.ligatures.push_back(LigatureRule{{F, I, I}, LIG_FII});

    // GPOS pair kern: tuck "AV" together by -3.
    t.kern[ShapingTable::kernKey(A, V)] = -3.0f;

    // GPOS mark-to-base: acute is a mark; base 'a' anchor at (3,8), mark anchor at (1,0).
    t.marks.insert(MARK_ACUTE);
    t.baseAnchor[BASE_A] = ShapeAnchor{3.0f, 8.0f};
    t.markAnchor[MARK_ACUTE] = ShapeAnchor{1.0f, 0.0f};
    return t;
}

int main() {
    const ShapingTable t = makeTable();

    // --- 1. Ligature substitution: "fi" -> one glyph with the ligature advance. ---
    {
        const auto r = shapeGlyphs({F, I}, t);
        CHECK(r.size() == 1, "fi collapses to one glyph");
        CHECK(r[0].glyph == LIG_FI, "and it is the fi ligature");
        CHECK(nearf(r[0].xAdvance, 8.0f), "with the ligature advance");
        CHECK(r[0].cluster == 0, "cluster is the first source index");
        CHECK(nearf(shapedWidth(r), 8.0f), "shaped width is the ligature advance");
    }

    // --- 2. Longest match wins: "fii" prefers f-i-i over f-i + i. ---
    {
        const auto r = shapeGlyphs({F, I, I}, t);
        CHECK(r.size() == 1 && r[0].glyph == LIG_FII, "fii uses the longest ligature");
        CHECK(nearf(r[0].xAdvance, 11.0f), "longest-ligature advance");
    }

    // --- 3. Cluster merging: after a ligature, the next glyph keeps its true source index. ---
    {
        const auto r = shapeGlyphs({F, I, X}, t); // fi + x
        CHECK(r.size() == 2, "fi + x -> two glyphs");
        CHECK(r[0].glyph == LIG_FI && r[0].cluster == 0, "ligature cluster 0");
        CHECK(r[1].glyph == X && r[1].cluster == 2, "x keeps original cluster index 2");
    }

    // --- 4. Pair kerning: "AV" adjusts the first advance by the kern, second untouched. ---
    {
        const auto r = shapeGlyphs({A, V}, t);
        CHECK(r.size() == 2, "AV -> two glyphs");
        CHECK(nearf(r[0].xAdvance, 10.0f - 3.0f), "A advance kerned by -3");
        CHECK(nearf(r[1].xAdvance, 10.0f), "V advance unchanged");
        CHECK(nearf(shapedWidth(r), 17.0f), "total width reflects the kern");
        // No kern pair for "Ax": both advances default.
        const auto r2 = shapeGlyphs({A, X}, t);
        CHECK(nearf(r2[0].xAdvance, 10.0f) && nearf(r2[1].xAdvance, 5.0f), "no kern for AX");
    }

    // --- 5. Mark-to-base: acute after 'a' is zero-advance and anchor-aligned. ---
    {
        const auto r = shapeGlyphs({BASE_A, MARK_ACUTE}, t);
        CHECK(r.size() == 2, "a + acute -> two glyphs");
        CHECK(nearf(r[0].xAdvance, 7.0f), "base advance is normal");
        CHECK(nearf(r[1].xAdvance, 0.0f), "mark has zero advance");
        // xOffset = baseAnchor.x - baseAdvance - markAnchor.x = 3 - 7 - 1 = -5; yOffset = 8 - 0 = 8.
        CHECK(nearf(r[1].xOffset, -5.0f), "mark x-offset aligns anchors");
        CHECK(nearf(r[1].yOffset, 8.0f), "mark y-offset aligns anchors");
        CHECK(r[1].cluster == 1, "mark cluster is its own source index");
        // The run's total advance is just the base (marks contribute 0).
        CHECK(nearf(shapedWidth(r), 7.0f), "mark does not widen the run");
    }

    // --- 6. Passthrough: a run with no rules is unchanged. ---
    {
        const auto r = shapeGlyphs({X, X, X}, t);
        CHECK(r.size() == 3, "no substitution when nothing matches");
        CHECK(nearf(r[0].xAdvance, 5.0f) && nearf(r[2].xAdvance, 5.0f), "advances preserved");
        CHECK(r[0].cluster == 0 && r[1].cluster == 1 && r[2].cluster == 2, "clusters are identity");
    }

    // --- 7. Combined: shape "fiAV" — ligature then kern in the same run. ---
    {
        const auto r = shapeGlyphs({F, I, A, V}, t);
        CHECK(r.size() == 3, "fiAV -> fi, A, V");
        CHECK(r[0].glyph == LIG_FI, "fi ligated");
        CHECK(nearf(r[1].xAdvance, 7.0f), "A kerned before V in the same run");
        CHECK(r[2].cluster == 3, "V keeps source index 3 past the ligature");
    }

    if (g_fail == 0) {
        std::printf("textshaping: OK — ligatures, longest match, clusters, kerning, mark-to-base, "
                    "passthrough, combined.\n");
        return 0;
    }
    std::printf("textshaping: %d failure(s).\n", g_fail);
    return 1;
}
