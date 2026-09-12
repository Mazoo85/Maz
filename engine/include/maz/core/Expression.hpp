#pragma once

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace maz::core {

// Runtime math-expression parser + evaluator — Godot's Expression class. Parse a formula string ONCE
// (e.g. "sin(x*3) * amp + 0.5"), naming the free variables, then execute() it repeatedly with different
// variable values. This is the workhorse behind data-driven design: damage/difficulty/economy formulas
// in a config file, procedural-parameter curves, spawn weights, tool sliders — anything you'd otherwise
// hard-code and recompile. Recursive-descent parser -> a flat AST node pool (copyable, no owning
// pointers) -> a pure recursive evaluator. Deterministic and dependency-free, so it unit-tests exactly
// and drives a function-plot golden.
//
// Grammar (standard precedence; '^' is right-associative and binds tighter than unary minus, so
// -2^2 == -(2^2) == -4, and 2^2^3 == 2^(2^3)):
//   expr   := term (('+'|'-') term)*
//   term   := unary (('*'|'/'|'%') unary)*
//   unary  := ('+'|'-') unary | power
//   power  := primary ('^' unary)?
//   primary:= number | const | ident | ident '(' [expr (',' expr)*] ')' | '(' expr ')'
//
// Constants: pi, tau, e. Functions: sin cos tan asin acos atan exp log log2 sqrt abs floor ceil round
// sign frac (1-arg); pow atan2 min max mod (2-arg); clamp lerp (3-arg). Unknown names / bad arity /
// syntax errors set an error string and make parse() return false.
//
// Honest scope vs Godot's Expression: this is the numeric subset — doubles in, one double out. It does
// NOT evaluate Variant types, strings, booleans/comparisons, array/dictionary literals, or method calls
// on an arbitrary base object; those are tied to Godot's Variant/Object model and are out of scope here.

class Expression {
public:
    // Parse `text`, binding the given free-variable names (their order is the index order used by
    // execute()). Returns false and sets errorText() on any lexical/syntax/semantic error.
    bool parse(const std::string& text, const std::vector<std::string>& varNames = {}) {
        m_nodes.clear();
        m_root = -1;
        m_error.clear();
        m_vars = varNames;
        if (!tokenize(text)) {
            return false;
        }
        m_pos = 0;
        m_root = parseExpr();
        if (!m_error.empty()) {
            return false;
        }
        if (m_pos != m_tokens.size()) {
            setError("unexpected trailing tokens");
            return false;
        }
        return true;
    }

    bool hasError() const { return !m_error.empty(); }
    const std::string& errorText() const { return m_error; }

    // Evaluate with variable values indexed by the parse() name order. Missing trailing inputs read 0.
    double execute(const std::vector<double>& inputs = {}) const {
        if (m_root < 0) {
            return 0.0;
        }
        return eval(m_root, inputs);
    }

private:
    enum class Kind { Num, Var, Neg, Add, Sub, Mul, Div, Mod, Pow, Call };

    struct Node {
        Kind kind = Kind::Num;
        double num = 0.0;    // Num
        int varIndex = -1;   // Var
        std::string fn;      // Call
        std::vector<int> kids;
    };

    struct Token {
        enum Type { Number, Ident, Op, LParen, RParen, Comma } type;
        double value = 0.0;
        std::string text;
        char op = 0;
    };

    // ---- lexer ------------------------------------------------------------------------------------
    bool tokenize(const std::string& s) {
        m_tokens.clear();
        std::size_t i = 0;
        const std::size_t n = s.size();
        while (i < n) {
            const char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i;
                continue;
            }
            if ((c >= '0' && c <= '9') || c == '.') {
                std::size_t j = i;
                bool dot = false;
                bool exp = false;
                while (j < n) {
                    const char d = s[j];
                    if (d >= '0' && d <= '9') {
                        ++j;
                    } else if (d == '.' && !dot && !exp) {
                        dot = true;
                        ++j;
                    } else if ((d == 'e' || d == 'E') && !exp && j > i) {
                        exp = true;
                        ++j;
                        if (j < n && (s[j] == '+' || s[j] == '-')) {
                            ++j;
                        }
                    } else {
                        break;
                    }
                }
                Token t;
                t.type = Token::Number;
                t.value = std::strtod(s.substr(i, j - i).c_str(), nullptr);
                m_tokens.push_back(t);
                i = j;
                continue;
            }
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                std::size_t j = i;
                while (j < n) {
                    const char d = s[j];
                    if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') ||
                        d == '_') {
                        ++j;
                    } else {
                        break;
                    }
                }
                Token t;
                t.type = Token::Ident;
                t.text = s.substr(i, j - i);
                m_tokens.push_back(t);
                i = j;
                continue;
            }
            Token t;
            if (c == '(') {
                t.type = Token::LParen;
            } else if (c == ')') {
                t.type = Token::RParen;
            } else if (c == ',') {
                t.type = Token::Comma;
            } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^') {
                t.type = Token::Op;
                t.op = c;
            } else {
                setError(std::string("unexpected character '") + c + "'");
                return false;
            }
            m_tokens.push_back(t);
            ++i;
        }
        return true;
    }

    // ---- parser -----------------------------------------------------------------------------------
    int addNode(Node n) {
        m_nodes.push_back(std::move(n));
        return static_cast<int>(m_nodes.size()) - 1;
    }

    const Token* peek() const { return m_pos < m_tokens.size() ? &m_tokens[m_pos] : nullptr; }

    int parseExpr() {
        int left = parseTerm();
        if (!m_error.empty()) {
            return -1;
        }
        while (const Token* t = peek()) {
            if (t->type != Token::Op || (t->op != '+' && t->op != '-')) {
                break;
            }
            const char op = t->op;
            ++m_pos;
            const int right = parseTerm();
            if (!m_error.empty()) {
                return -1;
            }
            Node n;
            n.kind = op == '+' ? Kind::Add : Kind::Sub;
            n.kids = {left, right};
            left = addNode(std::move(n));
        }
        return left;
    }

    int parseTerm() {
        int left = parseUnary();
        if (!m_error.empty()) {
            return -1;
        }
        while (const Token* t = peek()) {
            if (t->type != Token::Op || (t->op != '*' && t->op != '/' && t->op != '%')) {
                break;
            }
            const char op = t->op;
            ++m_pos;
            const int right = parseUnary();
            if (!m_error.empty()) {
                return -1;
            }
            Node n;
            n.kind = op == '*' ? Kind::Mul : (op == '/' ? Kind::Div : Kind::Mod);
            n.kids = {left, right};
            left = addNode(std::move(n));
        }
        return left;
    }

    int parseUnary() {
        if (const Token* t = peek()) {
            if (t->type == Token::Op && (t->op == '+' || t->op == '-')) {
                const char op = t->op;
                ++m_pos;
                const int operand = parseUnary();
                if (!m_error.empty()) {
                    return -1;
                }
                if (op == '+') {
                    return operand;
                }
                Node n;
                n.kind = Kind::Neg;
                n.kids = {operand};
                return addNode(std::move(n));
            }
        }
        return parsePower();
    }

    int parsePower() {
        const int base = parsePrimary();
        if (!m_error.empty()) {
            return -1;
        }
        if (const Token* t = peek()) {
            if (t->type == Token::Op && t->op == '^') {
                ++m_pos;
                const int expo = parseUnary(); // right-associative
                if (!m_error.empty()) {
                    return -1;
                }
                Node n;
                n.kind = Kind::Pow;
                n.kids = {base, expo};
                return addNode(std::move(n));
            }
        }
        return base;
    }

    int parsePrimary() {
        const Token* t = peek();
        if (!t) {
            setError("unexpected end of expression");
            return -1;
        }
        if (t->type == Token::Number) {
            ++m_pos;
            Node n;
            n.kind = Kind::Num;
            n.num = t->value;
            return addNode(std::move(n));
        }
        if (t->type == Token::LParen) {
            ++m_pos;
            const int inner = parseExpr();
            if (!m_error.empty()) {
                return -1;
            }
            const Token* r = peek();
            if (!r || r->type != Token::RParen) {
                setError("expected ')'");
                return -1;
            }
            ++m_pos;
            return inner;
        }
        if (t->type == Token::Ident) {
            const std::string name = t->text;
            ++m_pos;
            const Token* after = peek();
            if (after && after->type == Token::LParen) {
                // function call
                ++m_pos;
                std::vector<int> args;
                if (const Token* p = peek(); !p || p->type != Token::RParen) {
                    while (true) {
                        const int arg = parseExpr();
                        if (!m_error.empty()) {
                            return -1;
                        }
                        args.push_back(arg);
                        const Token* c = peek();
                        if (c && c->type == Token::Comma) {
                            ++m_pos;
                            continue;
                        }
                        break;
                    }
                }
                const Token* close = peek();
                if (!close || close->type != Token::RParen) {
                    setError("expected ')' after arguments to '" + name + "'");
                    return -1;
                }
                ++m_pos;
                const int arity = functionArity(name);
                if (arity < 0) {
                    setError("unknown function '" + name + "'");
                    return -1;
                }
                if (static_cast<int>(args.size()) != arity) {
                    setError("function '" + name + "' expects " + std::to_string(arity) + " argument(s)");
                    return -1;
                }
                Node n;
                n.kind = Kind::Call;
                n.fn = name;
                n.kids = args;
                return addNode(std::move(n));
            }
            // constant?
            double cval = 0.0;
            if (constant(name, cval)) {
                Node n;
                n.kind = Kind::Num;
                n.num = cval;
                return addNode(std::move(n));
            }
            // variable?
            for (std::size_t vi = 0; vi < m_vars.size(); ++vi) {
                if (m_vars[vi] == name) {
                    Node n;
                    n.kind = Kind::Var;
                    n.varIndex = static_cast<int>(vi);
                    return addNode(std::move(n));
                }
            }
            setError("unknown identifier '" + name + "'");
            return -1;
        }
        setError("unexpected token");
        return -1;
    }

    // ---- semantics --------------------------------------------------------------------------------
    static bool constant(const std::string& name, double& out) {
        if (name == "pi") {
            out = 3.14159265358979323846;
            return true;
        }
        if (name == "tau") {
            out = 6.28318530717958647692;
            return true;
        }
        if (name == "e") {
            out = 2.71828182845904523536;
            return true;
        }
        return false;
    }

    static int functionArity(const std::string& n) {
        if (n == "sin" || n == "cos" || n == "tan" || n == "asin" || n == "acos" || n == "atan" ||
            n == "exp" || n == "log" || n == "log2" || n == "sqrt" || n == "abs" || n == "floor" ||
            n == "ceil" || n == "round" || n == "sign" || n == "frac") {
            return 1;
        }
        if (n == "pow" || n == "atan2" || n == "min" || n == "max" || n == "mod") {
            return 2;
        }
        if (n == "clamp" || n == "lerp") {
            return 3;
        }
        return -1;
    }

    double eval(int idx, const std::vector<double>& in) const {
        const Node& n = m_nodes[static_cast<std::size_t>(idx)];
        switch (n.kind) {
        case Kind::Num:
            return n.num;
        case Kind::Var: {
            const std::size_t vi = static_cast<std::size_t>(n.varIndex);
            return vi < in.size() ? in[vi] : 0.0;
        }
        case Kind::Neg:
            return -eval(n.kids[0], in);
        case Kind::Add:
            return eval(n.kids[0], in) + eval(n.kids[1], in);
        case Kind::Sub:
            return eval(n.kids[0], in) - eval(n.kids[1], in);
        case Kind::Mul:
            return eval(n.kids[0], in) * eval(n.kids[1], in);
        case Kind::Div: {
            const double d = eval(n.kids[1], in);
            return d == 0.0 ? 0.0 : eval(n.kids[0], in) / d;
        }
        case Kind::Mod: {
            const double d = eval(n.kids[1], in);
            return d == 0.0 ? 0.0 : std::fmod(eval(n.kids[0], in), d);
        }
        case Kind::Pow:
            return std::pow(eval(n.kids[0], in), eval(n.kids[1], in));
        case Kind::Call:
            return callFn(n, in);
        }
        return 0.0;
    }

    double callFn(const Node& n, const std::vector<double>& in) const {
        const std::string& f = n.fn;
        const double a = n.kids.empty() ? 0.0 : eval(n.kids[0], in);
        if (f == "sin") return std::sin(a);
        if (f == "cos") return std::cos(a);
        if (f == "tan") return std::tan(a);
        if (f == "asin") return std::asin(a);
        if (f == "acos") return std::acos(a);
        if (f == "atan") return std::atan(a);
        if (f == "exp") return std::exp(a);
        if (f == "log") return std::log(a);
        if (f == "log2") return std::log2(a);
        if (f == "sqrt") return std::sqrt(a);
        if (f == "abs") return std::fabs(a);
        if (f == "floor") return std::floor(a);
        if (f == "ceil") return std::ceil(a);
        if (f == "round") return std::round(a);
        if (f == "sign") return a > 0.0 ? 1.0 : (a < 0.0 ? -1.0 : 0.0);
        if (f == "frac") return a - std::floor(a);
        const double b = n.kids.size() > 1 ? eval(n.kids[1], in) : 0.0;
        if (f == "pow") return std::pow(a, b);
        if (f == "atan2") return std::atan2(a, b);
        if (f == "min") return a < b ? a : b;
        if (f == "max") return a > b ? a : b;
        if (f == "mod") return b == 0.0 ? 0.0 : std::fmod(a, b);
        const double c = n.kids.size() > 2 ? eval(n.kids[2], in) : 0.0;
        if (f == "clamp") return a < b ? b : (a > c ? c : a);
        if (f == "lerp") return a + (b - a) * c;
        return 0.0;
    }

    void setError(const std::string& msg) {
        if (m_error.empty()) {
            m_error = msg;
        }
    }

    std::vector<Node> m_nodes;
    int m_root = -1;
    std::string m_error;
    std::vector<std::string> m_vars;
    std::vector<Token> m_tokens;
    std::size_t m_pos = 0;
};

} // namespace maz::core
