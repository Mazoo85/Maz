#pragma once

#include <functional>
#include <limits>
#include <vector>

// maz::game minimax with alpha-beta pruning — the classic adversarial search for perfect-information,
// turn-based games (tic-tac-toe, connect-four, checkers, reversi, and simpler chess-likes). Given a way
// to list moves, apply a move, tell terminal states, and score a position from the maximizing player's
// point of view, it returns the optimal move and its value, looking `depth` plies ahead. Alpha-beta
// pruning skips branches that cannot affect the result, so it explores far fewer nodes than naive
// minimax while returning the exact same value. Godot ships no game-tree search, so this is a
// beyond-Godot AI utility. Generic over the caller's State/Move types via std::function callbacks.
// Header-only, std-only, deterministic (ties keep the first optimal move in move order).
namespace maz::game {

// The rules of a two-player zero-sum game, supplied by the caller.
template <typename State, typename Move>
struct GameRules {
    std::function<bool(const State&)> isTerminal;                       // game over?
    std::function<float(const State&)> evaluate;                        // + favours the maximizer
    std::function<std::vector<Move>(const State&)> moves;              // legal moves (empty if none)
    std::function<State(const State&, const Move&)> apply;             // state after a move
};

template <typename Move>
struct SearchResult {
    float value = 0.0f;      // minimax value of the position (maximizer's perspective)
    Move bestMove{};         // best move found (valid only if hasMove)
    bool hasMove = false;    // false at terminal/leaf states
    long nodesVisited = 0;   // states expanded, incl. the root (for measuring pruning)
};

// Alpha-beta search. `maximizing` is true when it is the maximizing player's turn. `depth` bounds the
// look-ahead (evaluate() is used at depth 0 or terminal states). Leave alpha/beta at their defaults for
// a full-window search; the pruning window is used internally during recursion.
template <typename State, typename Move>
SearchResult<Move> minimax(const GameRules<State, Move>& game, const State& state, int depth,
                           bool maximizing,
                           float alpha = -std::numeric_limits<float>::infinity(),
                           float beta = std::numeric_limits<float>::infinity()) {
    SearchResult<Move> result;
    result.nodesVisited = 1;

    if (depth <= 0 || game.isTerminal(state)) {
        result.value = game.evaluate(state);
        return result;
    }
    const std::vector<Move> moves = game.moves(state);
    if (moves.empty()) {
        result.value = game.evaluate(state);
        return result;
    }

    if (maximizing) {
        result.value = -std::numeric_limits<float>::infinity();
        for (const Move& m : moves) {
            const State next = game.apply(state, m);
            const SearchResult<Move> child = minimax(game, next, depth - 1, false, alpha, beta);
            result.nodesVisited += child.nodesVisited;
            if (child.value > result.value) {
                result.value = child.value;
                result.bestMove = m;
                result.hasMove = true;
            }
            if (result.value > alpha) {
                alpha = result.value;
            }
            if (alpha >= beta) {
                break; // beta cutoff: the minimizer would never allow this branch
            }
        }
    } else {
        result.value = std::numeric_limits<float>::infinity();
        for (const Move& m : moves) {
            const State next = game.apply(state, m);
            const SearchResult<Move> child = minimax(game, next, depth - 1, true, alpha, beta);
            result.nodesVisited += child.nodesVisited;
            if (child.value < result.value) {
                result.value = child.value;
                result.bestMove = m;
                result.hasMove = true;
            }
            if (result.value < beta) {
                beta = result.value;
            }
            if (alpha >= beta) {
                break; // alpha cutoff
            }
        }
    }
    return result;
}

} // namespace maz::game
