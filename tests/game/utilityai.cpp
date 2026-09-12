// tests/game/utilityai.cpp — verifies the utility-AI scorer (game::ResponseCurve / Consideration /
// UtilityAction / selectBestAction). Ground truths: response curves evaluate to their hand-computed values
// and clamp to [0,1]; a consideration clamps its input; an action's score is the (optionally compensated)
// product of its considerations times the weight; a zero (veto) consideration kills an action even if it has
// a high weight; selectBestAction returns the highest scorer, breaks ties toward the lowest index, reports
// the winning score, and returns -1 on an empty list. Pure CPU, deterministic.
#include "maz/game/UtilityAI.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::game;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // --- 1. Response curves evaluate to hand-computed values and clamp. ---
    {
        ResponseCurve lin; // identity
        CHECK(near(lin.evaluate(0.3f), 0.3f), "linear identity at 0.3");
        CHECK(near(lin.evaluate(-1.0f), 0.0f) && near(lin.evaluate(2.0f), 1.0f), "linear clamps to [0,1]");

        ResponseCurve quad{ResponseCurveType::Polynomial, 1.0f, 2.0f, 0.0f, 0.0f};
        CHECK(near(quad.evaluate(0.5f), 0.25f), "quadratic at 0.5 = 0.25");

        ResponseCurve step{ResponseCurveType::Step, 1.0f, 1.0f, 0.5f, 0.0f}; // threshold at 0.5
        CHECK(near(step.evaluate(0.4f), 0.0f) && near(step.evaluate(0.6f), 1.0f), "step threshold at 0.5");

        ResponseCurve konst{ResponseCurveType::Constant, 0.0f, 0.0f, 0.0f, 0.42f};
        CHECK(near(konst.evaluate(0.1f), 0.42f) && near(konst.evaluate(0.9f), 0.42f), "constant curve");

        ResponseCurve logi{ResponseCurveType::Logistic, 1.0f, 10.0f, 0.5f, 0.0f}; // midpoint 0.5
        CHECK(near(logi.evaluate(0.5f), 0.5f), "logistic midpoint = 0.5");
        CHECK(logi.evaluate(0.9f) > logi.evaluate(0.1f), "logistic increasing");
    }

    // --- 2. Consideration clamps its input. ---
    {
        Consideration c{5.0f, ResponseCurve{}}; // input > 1 clamps to 1 -> identity -> 1
        CHECK(near(c.score(), 1.0f), "consideration clamps input to 1");
        Consideration c2{-3.0f, ResponseCurve{}};
        CHECK(near(c2.score(), 0.0f), "consideration clamps input to 0");
    }

    // --- 3. Action score = weight * product (no compensation). ---
    {
        UtilityAction a;
        a.weight = 2.0f;
        a.compensate = false;
        a.considerations = {Consideration{0.5f, {}}, Consideration{0.5f, {}}};
        CHECK(near(a.score(), 2.0f * 0.25f), "uncompensated product = weight*0.25");
    }

    // --- 4. Compensation raises the product of many sub-one factors. ---
    {
        UtilityAction plain, comp;
        plain.compensate = false;
        comp.compensate = true;
        plain.considerations = comp.considerations = {Consideration{0.5f, {}}, Consideration{0.5f, {}}};
        CHECK(comp.score() > plain.score(), "compensation raises the score");
        // Hand value: mod = 0.5; each factor 0.5 -> 0.5 + 0.5*0.5*0.5 = 0.625; product 0.390625.
        CHECK(near(comp.score(), 0.390625f), "compensated score hand-computed");
    }

    // --- 5. A zero (veto) consideration kills the action regardless of weight. ---
    {
        UtilityAction vetoed;
        vetoed.weight = 100.0f;
        vetoed.considerations = {Consideration{1.0f, {}}, Consideration{0.0f, {}}}; // second vetoes
        CHECK(near(vetoed.score(), 0.0f), "veto consideration zeroes the score");
    }

    // --- 6. selectBestAction picks the highest, and a veto loses to a modest all-satisfied action. ---
    {
        UtilityAction reload; // high weight but a zombie is adjacent -> vetoed
        reload.weight = 10.0f;
        reload.considerations = {Consideration{0.0f, {}}}; // "safe to reload?" = 0
        UtilityAction attack; // modest but all considerations satisfied
        attack.weight = 1.0f;
        attack.considerations = {Consideration{0.8f, {}}};
        float best = -1.0f;
        const int idx = selectBestAction({reload, attack}, &best);
        CHECK(idx == 1, "veto action loses to the satisfied action");
        CHECK(near(best, attack.score()), "winning score reported");
    }

    // --- 7. Ties break toward the lowest index; empty list returns -1. ---
    {
        UtilityAction a, b;
        a.considerations = {Consideration{0.5f, {}}};
        b.considerations = {Consideration{0.5f, {}}}; // identical score
        CHECK(selectBestAction({a, b}) == 0, "tie breaks toward lowest index");
        float s = 123.0f;
        CHECK(selectBestAction({}, &s) == -1 && near(s, 0.0f), "empty list -> -1, score 0");
    }

    if (g_fail == 0) {
        std::printf("utility ai: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
