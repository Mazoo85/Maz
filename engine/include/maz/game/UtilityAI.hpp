#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game utility AI — score-based decision making (Dave Mark's "Infinite Axis Utility System"), the AI
// paradigm behind The Sims and many modern shooters. Instead of a fixed tree or plan, every candidate ACTION
// is scored each think-tick and the highest scorer wins, so behaviour emerges smoothly from the situation and
// scales to dozens of actions without hand-wiring transitions. Each action's score is the product of its
// CONSIDERATIONS: a consideration takes a normalized game input (hunger/100, distance/range, ammo fraction, …
// all in [0,1]) and runs it through a RESPONSE CURVE (linear, polynomial, logistic, step, constant) to a
// factor in [0,1]. Multiplying the factors means any single unmet consideration (≈0) VETOES the action, which
// is exactly the desired "don't reload while a zombie is biting you" behaviour; an optional make-up
// compensation counteracts the natural shrink of multiplying many sub-one factors so richer actions aren't
// unfairly penalised.
//
// This complements — and is distinct from — the engine's other AI: BehaviorTree (fixed priority/sequence
// structure), GOAP (plans a sequence toward a goal), StateMachine (explicit states/transitions), and Minimax
// (adversarial game tree). Utility AI is the fuzzy, reactive "what's the most appropriate thing to do right
// now?" scorer. Deterministic, header-only, std-only. Godot ships no utility-AI system.
namespace maz::game {

inline float utilClamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

enum class ResponseCurveType {
    Linear,     // slope * (x - xShift) + yShift
    Polynomial, // slope * (x - xShift)^exponent + yShift  (base clamped >= 0)
    Logistic,   // yShift + slope / (1 + exp(-exponent * (x - xShift)))  (sigmoid; xShift is the midpoint)
    Step,       // x >= xShift ? slope + yShift : yShift   (threshold)
    Constant,   // yShift
};

// Maps a normalized input in [0,1] to a normalized response in [0,1] (output is clamped).
struct ResponseCurve {
    ResponseCurveType type = ResponseCurveType::Linear;
    float slope = 1.0f;
    float exponent = 1.0f;
    float xShift = 0.0f;
    float yShift = 0.0f;

    float evaluate(float x) const {
        float y = 0.0f;
        switch (type) {
            case ResponseCurveType::Linear:
                y = slope * (x - xShift) + yShift;
                break;
            case ResponseCurveType::Polynomial: {
                const float base = x - xShift;
                y = slope * std::pow(base < 0.0f ? 0.0f : base, exponent) + yShift;
                break;
            }
            case ResponseCurveType::Logistic:
                y = yShift + slope / (1.0f + std::exp(-exponent * (x - xShift)));
                break;
            case ResponseCurveType::Step:
                y = (x >= xShift) ? slope + yShift : yShift;
                break;
            case ResponseCurveType::Constant:
                y = yShift;
                break;
        }
        return utilClamp01(y);
    }
};

// One axis of an action's appropriateness: a normalized game input passed through a response curve.
struct Consideration {
    float input = 0.0f; // normalized [0,1] game value (clamped on use)
    ResponseCurve curve;

    float score() const { return curve.evaluate(utilClamp01(input)); }
};

// A candidate action: a base weight (priority) times the (compensated) product of its considerations.
struct UtilityAction {
    float weight = 1.0f;
    std::vector<Consideration> considerations;
    bool compensate = true; // Dave Mark's make-up-value: counteract multiplying many sub-one factors

    float score() const {
        const std::size_t n = considerations.size();
        if (n == 0) {
            return weight; // no considerations: pure base weight
        }
        const float mod = 1.0f - 1.0f / static_cast<float>(n);
        float product = 1.0f;
        for (const Consideration& c : considerations) {
            float s = c.score();
            if (compensate) {
                s = s + (1.0f - s) * mod * s; // pull each factor back up toward 1 by the make-up amount
            }
            product *= s;
            if (product == 0.0f) {
                break; // a vetoing (zero) consideration kills the action
            }
        }
        return weight * product;
    }
};

// Index of the highest-scoring action, or -1 if `actions` is empty. Deterministic: on ties the LOWEST index
// wins. If `outScore` is non-null it receives the winning score (0 when empty).
inline int selectBestAction(const std::vector<UtilityAction>& actions, float* outScore = nullptr) {
    int best = -1;
    float bestScore = 0.0f;
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        const float s = actions[static_cast<std::size_t>(i)].score();
        if (best < 0 || s > bestScore) {
            best = i;
            bestScore = s;
        }
    }
    if (outScore) {
        *outScore = best < 0 ? 0.0f : bestScore;
    }
    return best;
}

} // namespace maz::game
