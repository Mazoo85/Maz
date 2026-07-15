#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// maz::script — a small dynamically-typed scripting language with a tree-walking interpreter, the
// engine's answer to Godot's GDScript. Game logic can live in text scripts (hot-reloadable, no
// recompile) instead of compiled C++, and native C++ functions are exposed to scripts through a
// simple binding API. It is a pure, dependency-free, deterministic VM (no globals, no allocation
// surprises) — a deliberate edge over an embedded third-party runtime: a script can never reach
// outside the API the host hands it.
//
//   SC1: numbers / strings / bools / nil, the full operator set, variables, if/else, while, C-style
//        for, functions + return, native host functions, line-numbered errors.
//   SC2: arrays [..] and dictionaries {k: v} (reference semantics), indexing a[i] / d[k] / d.k with
//        read + write, for-in over arrays / dict keys / ranges / string chars, break / continue, the
//        `in` / membership operator, method calls (arr.append(x), dict.keys(), ...), len()/range().
//
// Everything is header-only to match the rest of maz::. The AST is owned by the Vm for the lifetime
// of a loaded program; runtime environments are stack-allocated during evaluation (functions are not
// closures — they see globals + their own parameters/locals — so nothing escapes the call).
namespace maz::script {

// ---------------------------------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------------------------------
struct FuncDef; // forward: a user function definition (AST-owned)
struct Value;
using ArrayData = std::vector<Value>;
using DictData = std::vector<std::pair<Value, Value>>; // insertion-ordered, any-typed keys (like GDScript)

struct Value {
    enum class Type { Nil, Bool, Num, Str, Native, Func, Array, Dict };
    Type type = Type::Nil;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::function<Value(std::vector<Value>&)> native; // when Type::Native
    const FuncDef* func = nullptr;                     // when Type::Func (AST-owned)
    std::shared_ptr<ArrayData> array;                      // when Type::Array (shared/reference semantics)
    std::shared_ptr<DictData> dict;                        // when Type::Dict

    Value() = default;
    static Value nil() { return Value{}; }
    static Value fromBool(bool b) {
        Value v;
        v.type = Type::Bool;
        v.boolean = b;
        return v;
    }
    static Value fromNum(double n) {
        Value v;
        v.type = Type::Num;
        v.number = n;
        return v;
    }
    static Value fromStr(std::string s) {
        Value v;
        v.type = Type::Str;
        v.str = std::move(s);
        return v;
    }
    static Value newArray() {
        Value v;
        v.type = Type::Array;
        v.array = std::make_shared<ArrayData>();
        return v;
    }
    static Value fromArray(std::shared_ptr<ArrayData> a) {
        Value v;
        v.type = Type::Array;
        v.array = std::move(a);
        return v;
    }
    static Value newDict() {
        Value v;
        v.type = Type::Dict;
        v.dict = std::make_shared<DictData>();
        return v;
    }

    bool isTruthy() const {
        switch (type) {
        case Type::Nil: return false;
        case Type::Bool: return boolean;
        case Type::Num: return number != 0.0;
        case Type::Str: return !str.empty();
        case Type::Array: return array && !array->empty();
        case Type::Dict: return dict && !dict->empty();
        default: return true; // callables are truthy
        }
    }

    bool equals(const Value& o) const {
        if (type != o.type) {
            return false;
        }
        switch (type) {
        case Type::Nil: return true;
        case Type::Bool: return boolean == o.boolean;
        case Type::Num: return number == o.number;
        case Type::Str: return str == o.str;
        case Type::Func: return func == o.func;
        case Type::Array: return array == o.array; // reference identity
        case Type::Dict: return dict == o.dict;
        default: return false; // natives compare unequal
        }
    }

    std::string toString() const {
        switch (type) {
        case Type::Nil: return "nil";
        case Type::Bool: return boolean ? "true" : "false";
        case Type::Str: return str;
        case Type::Num: {
            if (number == std::floor(number) && std::abs(number) < 1e15) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(number));
                return buf;
            }
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%g", number);
            return buf;
        }
        case Type::Native: return "<native fn>";
        case Type::Func: return "<fn>";
        case Type::Array: {
            std::string s = "[";
            if (array) {
                for (size_t i = 0; i < array->size(); ++i) {
                    if (i) {
                        s += ", ";
                    }
                    const Value& e = (*array)[i];
                    s += e.type == Type::Str ? ("\"" + e.str + "\"") : e.toString();
                }
            }
            return s + "]";
        }
        case Type::Dict: {
            std::string s = "{";
            if (dict) {
                for (size_t i = 0; i < dict->size(); ++i) {
                    if (i) {
                        s += ", ";
                    }
                    const Value& k = (*dict)[i].first;
                    const Value& val = (*dict)[i].second;
                    s += (k.type == Type::Str ? ("\"" + k.str + "\"") : k.toString()) + ": " +
                         (val.type == Type::Str ? ("\"" + val.str + "\"") : val.toString());
                }
            }
            return s + "}";
        }
        }
        return "nil";
    }
};

// ---------------------------------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------------------------------
enum class Tok {
    Number, String, Ident, True, False, Nil,
    Var, If, Else, While, For, Func, Return, And, Or, Print, In, Break, Continue,
    Plus, Minus, Star, Slash, Percent, Bang,
    Eq, EqEq, NotEq, Less, LessEq, Greater, GreaterEq,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Comma, Semicolon, Colon, Dot,
    End
};

struct Token {
    Tok kind = Tok::End;
    std::string text;
    double number = 0.0;
    int line = 1;
};

struct ScriptError {
    std::string message;
    int line = 0;
};

inline std::vector<Token> lex(const std::string& src, ScriptError& err) {
    std::vector<Token> out;
    int line = 1;
    size_t i = 0;
    const size_t n = src.size();
    auto push = [&](Tok k, std::string t = "") { out.push_back(Token{k, std::move(t), 0.0, line}); };

    while (i < n) {
        const char c = src[i];
        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            ++i;
            continue;
        }
        if (c == '#') {
            while (i < n && src[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            while (i < n && src[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            size_t j = i;
            while (j < n && (std::isdigit(static_cast<unsigned char>(src[j])) || src[j] == '.')) {
                ++j;
            }
            Token t{Tok::Number, src.substr(i, j - i), 0.0, line};
            t.number = std::strtod(t.text.c_str(), nullptr);
            out.push_back(t);
            i = j;
            continue;
        }
        if (c == '"') {
            ++i;
            std::string s;
            while (i < n && src[i] != '"') {
                char ch = src[i];
                if (ch == '\\' && i + 1 < n) {
                    const char e = src[i + 1];
                    if (e == 'n') {
                        ch = '\n';
                    } else if (e == 't') {
                        ch = '\t';
                    } else {
                        ch = e;
                    }
                    i += 2;
                    s.push_back(ch);
                    continue;
                }
                if (ch == '\n') {
                    ++line;
                }
                s.push_back(ch);
                ++i;
            }
            if (i >= n) {
                err = {"unterminated string", line};
                return {};
            }
            ++i;
            out.push_back(Token{Tok::String, s, 0.0, line});
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < n &&
                   (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '_')) {
                ++j;
            }
            const std::string word = src.substr(i, j - i);
            i = j;
            static const std::unordered_map<std::string, Tok> kw = {
                {"var", Tok::Var},         {"if", Tok::If},         {"else", Tok::Else},
                {"while", Tok::While},     {"for", Tok::For},       {"func", Tok::Func},
                {"return", Tok::Return},   {"and", Tok::And},       {"or", Tok::Or},
                {"true", Tok::True},       {"false", Tok::False},   {"nil", Tok::Nil},
                {"print", Tok::Print},     {"in", Tok::In},         {"break", Tok::Break},
                {"continue", Tok::Continue}};
            const auto it = kw.find(word);
            if (it != kw.end()) {
                push(it->second, word);
            } else {
                push(Tok::Ident, word);
            }
            continue;
        }
        auto two = [&](char a, char b) { return c == a && i + 1 < n && src[i + 1] == b; };
        if (two('=', '=')) { push(Tok::EqEq); i += 2; continue; }
        if (two('!', '=')) { push(Tok::NotEq); i += 2; continue; }
        if (two('<', '=')) { push(Tok::LessEq); i += 2; continue; }
        if (two('>', '=')) { push(Tok::GreaterEq); i += 2; continue; }
        switch (c) {
        case '+': push(Tok::Plus); break;
        case '-': push(Tok::Minus); break;
        case '*': push(Tok::Star); break;
        case '/': push(Tok::Slash); break;
        case '%': push(Tok::Percent); break;
        case '!': push(Tok::Bang); break;
        case '=': push(Tok::Eq); break;
        case '<': push(Tok::Less); break;
        case '>': push(Tok::Greater); break;
        case '(': push(Tok::LParen); break;
        case ')': push(Tok::RParen); break;
        case '{': push(Tok::LBrace); break;
        case '}': push(Tok::RBrace); break;
        case '[': push(Tok::LBracket); break;
        case ']': push(Tok::RBracket); break;
        case ',': push(Tok::Comma); break;
        case ';': push(Tok::Semicolon); break;
        case ':': push(Tok::Colon); break;
        case '.': push(Tok::Dot); break;
        default:
            err = {std::string("unexpected character '") + c + "'", line};
            return {};
        }
        ++i;
    }
    push(Tok::End);
    return out;
}

// ---------------------------------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------------------------------
struct Expr {
    enum class K {
        Number, String, Bool, Nil, Var, Assign, Unary, Binary, Logical, Call,
        ArrayLit, DictLit, Index, Get
    };
    K kind;
    double number = 0.0;
    std::string str;   // String literal, Var/Assign name, Get property
    bool boolean = false;
    Tok op = Tok::End; // Unary/Binary/Logical operator
    std::unique_ptr<Expr> lhs, rhs;          // operands; Assign target=lhs value=rhs; Index obj=lhs idx=rhs
    std::unique_ptr<Expr> callee;            // Call target (Var or Get)
    std::vector<std::unique_ptr<Expr>> args; // Call args / ArrayLit elems / DictLit values
    std::vector<std::unique_ptr<Expr>> keys; // DictLit keys (parallel to args)
    int line = 0;
};

struct Stmt {
    enum class K { Expr, Var, Block, If, While, For, ForIn, Func, Return, Print, Break, Continue };
    K kind;
    std::string name;                       // Var name / Func name / ForIn loop var
    std::unique_ptr<Expr> expr;             // Expr / Var init / If+While+For cond / ForIn iterable / Return / Print
    std::unique_ptr<Expr> forInit, forPost; // C-style For only
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> elseBody; // If else, or For's desugared `var` init
    std::vector<std::string> params;             // Func parameters
    int line = 0;
};

struct FuncDef {
    std::string name;
    std::vector<std::string> params;
    const std::vector<std::unique_ptr<Stmt>>* body = nullptr;
};

// ---------------------------------------------------------------------------------------------------
// Parser (recursive descent)
// ---------------------------------------------------------------------------------------------------
class Parser {
public:
    Parser(std::vector<Token> toks) : m_toks(std::move(toks)) {}

    std::vector<std::unique_ptr<Stmt>> parse() {
        std::vector<std::unique_ptr<Stmt>> stmts;
        while (!check(Tok::End)) {
            stmts.push_back(declaration());
        }
        return stmts;
    }

private:
    std::vector<Token> m_toks;
    size_t m_pos = 0;

    const Token& peek() const { return m_toks[m_pos]; }
    const Token& peekNext() const {
        return m_pos + 1 < m_toks.size() ? m_toks[m_pos + 1] : m_toks.back();
    }
    const Token& previous() const { return m_toks[m_pos - 1]; }
    bool check(Tok k) const { return peek().kind == k; }
    bool isAtEnd() const { return peek().kind == Tok::End; }
    const Token& advance() { return m_toks[m_pos++]; }
    bool match(Tok k) {
        if (check(k)) {
            ++m_pos;
            return true;
        }
        return false;
    }
    [[noreturn]] void error(const std::string& msg) { throw ScriptError{msg, peek().line}; }
    const Token& expect(Tok k, const std::string& msg) {
        if (!check(k)) {
            error(msg);
        }
        return advance();
    }

    // ---- statements ----
    std::unique_ptr<Stmt> declaration() {
        if (match(Tok::Var)) {
            return varDecl();
        }
        if (match(Tok::Func)) {
            return funcDecl();
        }
        return statement();
    }

    std::unique_ptr<Stmt> varDecl() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Var;
        s->line = peek().line;
        s->name = expect(Tok::Ident, "expected variable name").text;
        if (match(Tok::Eq)) {
            s->expr = expression();
        }
        expect(Tok::Semicolon, "expected ';' after variable declaration");
        return s;
    }

    std::unique_ptr<Stmt> funcDecl() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Func;
        s->line = peek().line;
        s->name = expect(Tok::Ident, "expected function name").text;
        expect(Tok::LParen, "expected '(' after function name");
        if (!check(Tok::RParen)) {
            do {
                s->params.push_back(expect(Tok::Ident, "expected parameter name").text);
            } while (match(Tok::Comma));
        }
        expect(Tok::RParen, "expected ')' after parameters");
        expect(Tok::LBrace, "expected '{' before function body");
        s->body = block();
        return s;
    }

    std::unique_ptr<Stmt> statement() {
        if (match(Tok::If)) {
            return ifStmt();
        }
        if (match(Tok::While)) {
            return whileStmt();
        }
        if (match(Tok::For)) {
            return forStmt();
        }
        if (match(Tok::Return)) {
            return returnStmt();
        }
        if (match(Tok::Print)) {
            return printStmt();
        }
        if (match(Tok::Break)) {
            auto s = std::make_unique<Stmt>();
            s->kind = Stmt::K::Break;
            s->line = previous().line;
            expect(Tok::Semicolon, "expected ';' after 'break'");
            return s;
        }
        if (match(Tok::Continue)) {
            auto s = std::make_unique<Stmt>();
            s->kind = Stmt::K::Continue;
            s->line = previous().line;
            expect(Tok::Semicolon, "expected ';' after 'continue'");
            return s;
        }
        if (match(Tok::LBrace)) {
            auto s = std::make_unique<Stmt>();
            s->kind = Stmt::K::Block;
            s->body = block();
            return s;
        }
        return exprStmt();
    }

    std::vector<std::unique_ptr<Stmt>> block() {
        std::vector<std::unique_ptr<Stmt>> stmts;
        while (!check(Tok::RBrace) && !isAtEnd()) {
            stmts.push_back(declaration());
        }
        expect(Tok::RBrace, "expected '}' after block");
        return stmts;
    }

    std::unique_ptr<Stmt> ifStmt() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::If;
        s->line = previous().line;
        expect(Tok::LParen, "expected '(' after 'if'");
        s->expr = expression();
        expect(Tok::RParen, "expected ')' after condition");
        s->body.push_back(statement());
        if (match(Tok::Else)) {
            s->elseBody.push_back(statement());
        }
        return s;
    }

    std::unique_ptr<Stmt> whileStmt() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::While;
        s->line = previous().line;
        expect(Tok::LParen, "expected '(' after 'while'");
        s->expr = expression();
        expect(Tok::RParen, "expected ')' after condition");
        s->body.push_back(statement());
        return s;
    }

    std::unique_ptr<Stmt> forStmt() {
        // Two shapes: for-in `for (x in iterable) body` / `for (var x in iterable) body`, and
        // C-style `for (init; cond; post) body`.
        const int line = previous().line;
        expect(Tok::LParen, "expected '(' after 'for'");

        const bool leadingVar = check(Tok::Var);
        // for-in?  (var x in ...) | (x in ...)
        if ((leadingVar && peekNext().kind == Tok::Ident) ||
            (check(Tok::Ident) && peekNext().kind == Tok::In)) {
            if (leadingVar) {
                advance(); // 'var'
            }
            const std::string loopVar = expect(Tok::Ident, "expected loop variable").text;
            if (match(Tok::In)) {
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::K::ForIn;
                s->line = line;
                s->name = loopVar;
                s->expr = expression();
                expect(Tok::RParen, "expected ')' after for-in clause");
                s->body.push_back(statement());
                return s;
            }
            // not `in` — it was a C-style `var x = ...`; fall through by reconstructing.
            auto s = std::make_unique<Stmt>();
            s->kind = Stmt::K::For;
            s->line = line;
            auto v = std::make_unique<Stmt>();
            v->kind = Stmt::K::Var;
            v->name = loopVar;
            if (match(Tok::Eq)) {
                v->expr = expression();
            }
            s->elseBody.push_back(std::move(v));
            expect(Tok::Semicolon, "expected ';' after for-initializer");
            if (!check(Tok::Semicolon)) {
                s->expr = expression();
            }
            expect(Tok::Semicolon, "expected ';' after for-condition");
            if (!check(Tok::RParen)) {
                s->forPost = expression();
            }
            expect(Tok::RParen, "expected ')' after for-clauses");
            s->body.push_back(statement());
            return s;
        }

        // C-style for.
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::For;
        s->line = line;
        if (!check(Tok::Semicolon)) {
            s->forInit = expression();
        }
        expect(Tok::Semicolon, "expected ';' after for-initializer");
        if (!check(Tok::Semicolon)) {
            s->expr = expression();
        }
        expect(Tok::Semicolon, "expected ';' after for-condition");
        if (!check(Tok::RParen)) {
            s->forPost = expression();
        }
        expect(Tok::RParen, "expected ')' after for-clauses");
        s->body.push_back(statement());
        return s;
    }

    std::unique_ptr<Stmt> returnStmt() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Return;
        s->line = previous().line;
        if (!check(Tok::Semicolon)) {
            s->expr = expression();
        }
        expect(Tok::Semicolon, "expected ';' after return value");
        return s;
    }

    std::unique_ptr<Stmt> printStmt() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Print;
        s->line = previous().line;
        s->expr = expression();
        expect(Tok::Semicolon, "expected ';' after value");
        return s;
    }

    std::unique_ptr<Stmt> exprStmt() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Expr;
        s->line = peek().line;
        s->expr = expression();
        expect(Tok::Semicolon, "expected ';' after expression");
        return s;
    }

    // ---- expressions ----
    std::unique_ptr<Expr> expression() { return assignment(); }

    std::unique_ptr<Expr> assignment() {
        auto lhs = logicOr();
        if (match(Tok::Eq)) {
            const int line = previous().line;
            auto value = assignment();
            if (lhs->kind != Expr::K::Var && lhs->kind != Expr::K::Index &&
                lhs->kind != Expr::K::Get) {
                throw ScriptError{"invalid assignment target", line};
            }
            auto e = std::make_unique<Expr>();
            e->kind = Expr::K::Assign;
            e->line = line;
            e->lhs = std::move(lhs);
            e->rhs = std::move(value);
            return e;
        }
        return lhs;
    }

    std::unique_ptr<Expr> binaryChain(std::unique_ptr<Expr> (Parser::*next)(),
                                      std::initializer_list<Tok> ops, Expr::K kind) {
        auto left = (this->*next)();
        for (;;) {
            bool matched = false;
            for (Tok o : ops) {
                if (check(o)) {
                    const Token t = advance();
                    auto right = (this->*next)();
                    auto e = std::make_unique<Expr>();
                    e->kind = kind;
                    e->op = t.kind;
                    e->line = t.line;
                    e->lhs = std::move(left);
                    e->rhs = std::move(right);
                    left = std::move(e);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Expr> logicOr() {
        return binaryChain(&Parser::logicAnd, {Tok::Or}, Expr::K::Logical);
    }
    std::unique_ptr<Expr> logicAnd() {
        return binaryChain(&Parser::equality, {Tok::And}, Expr::K::Logical);
    }
    std::unique_ptr<Expr> equality() {
        return binaryChain(&Parser::comparison, {Tok::EqEq, Tok::NotEq}, Expr::K::Binary);
    }
    std::unique_ptr<Expr> comparison() {
        return binaryChain(&Parser::term,
                           {Tok::Less, Tok::LessEq, Tok::Greater, Tok::GreaterEq, Tok::In},
                           Expr::K::Binary);
    }
    std::unique_ptr<Expr> term() {
        return binaryChain(&Parser::factor, {Tok::Plus, Tok::Minus}, Expr::K::Binary);
    }
    std::unique_ptr<Expr> factor() {
        return binaryChain(&Parser::unary, {Tok::Star, Tok::Slash, Tok::Percent}, Expr::K::Binary);
    }

    std::unique_ptr<Expr> unary() {
        if (check(Tok::Bang) || check(Tok::Minus)) {
            const Token t = advance();
            auto e = std::make_unique<Expr>();
            e->kind = Expr::K::Unary;
            e->op = t.kind;
            e->line = t.line;
            e->rhs = unary();
            return e;
        }
        return postfix();
    }

    // Postfix chain: call '(...)', index '[...]', property '.name'.
    std::unique_ptr<Expr> postfix() {
        auto e = primary();
        for (;;) {
            if (match(Tok::LParen)) {
                auto c = std::make_unique<Expr>();
                c->kind = Expr::K::Call;
                c->line = previous().line;
                c->callee = std::move(e);
                if (!check(Tok::RParen)) {
                    do {
                        c->args.push_back(expression());
                    } while (match(Tok::Comma));
                }
                expect(Tok::RParen, "expected ')' after arguments");
                e = std::move(c);
            } else if (match(Tok::LBracket)) {
                auto idx = std::make_unique<Expr>();
                idx->kind = Expr::K::Index;
                idx->line = previous().line;
                idx->lhs = std::move(e);
                idx->rhs = expression();
                expect(Tok::RBracket, "expected ']' after index");
                e = std::move(idx);
            } else if (match(Tok::Dot)) {
                auto g = std::make_unique<Expr>();
                g->kind = Expr::K::Get;
                g->line = previous().line;
                g->lhs = std::move(e);
                g->str = expect(Tok::Ident, "expected property name after '.'").text;
                e = std::move(g);
            } else {
                break;
            }
        }
        return e;
    }

    std::unique_ptr<Expr> primary() {
        auto e = std::make_unique<Expr>();
        e->line = peek().line;
        if (match(Tok::Number)) {
            e->kind = Expr::K::Number;
            e->number = previous().number;
            return e;
        }
        if (match(Tok::String)) {
            e->kind = Expr::K::String;
            e->str = previous().text;
            return e;
        }
        if (match(Tok::True)) {
            e->kind = Expr::K::Bool;
            e->boolean = true;
            return e;
        }
        if (match(Tok::False)) {
            e->kind = Expr::K::Bool;
            e->boolean = false;
            return e;
        }
        if (match(Tok::Nil)) {
            e->kind = Expr::K::Nil;
            return e;
        }
        if (match(Tok::Ident)) {
            e->kind = Expr::K::Var;
            e->str = previous().text;
            return e;
        }
        if (match(Tok::LBracket)) { // array literal
            e->kind = Expr::K::ArrayLit;
            if (!check(Tok::RBracket)) {
                do {
                    if (check(Tok::RBracket)) {
                        break; // trailing comma
                    }
                    e->args.push_back(expression());
                } while (match(Tok::Comma));
            }
            expect(Tok::RBracket, "expected ']' after array");
            return e;
        }
        if (match(Tok::LBrace)) { // dict literal (in expression position)
            e->kind = Expr::K::DictLit;
            if (!check(Tok::RBrace)) {
                do {
                    if (check(Tok::RBrace)) {
                        break;
                    }
                    e->keys.push_back(expression());
                    expect(Tok::Colon, "expected ':' in dictionary entry");
                    e->args.push_back(expression());
                } while (match(Tok::Comma));
            }
            expect(Tok::RBrace, "expected '}' after dictionary");
            return e;
        }
        if (match(Tok::LParen)) {
            auto inner = expression();
            expect(Tok::RParen, "expected ')' after expression");
            return inner;
        }
        error("expected expression");
    }
};

// ---------------------------------------------------------------------------------------------------
// Interpreter
// ---------------------------------------------------------------------------------------------------
struct Environment {
    std::unordered_map<std::string, Value> vars;
    Environment* parent = nullptr;

    bool assign(const std::string& name, const Value& v) {
        for (Environment* e = this; e; e = e->parent) {
            const auto it = e->vars.find(name);
            if (it != e->vars.end()) {
                it->second = v;
                return true;
            }
        }
        return false;
    }
    const Value* get(const std::string& name) const {
        for (const Environment* e = this; e; e = e->parent) {
            const auto it = e->vars.find(name);
            if (it != e->vars.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }
};

class Vm {
public:
    Vm() {
        registerNative("abs", [](std::vector<Value>& a) {
            return Value::fromNum(a.empty() ? 0.0 : std::abs(a[0].number));
        });
        registerNative("min", [](std::vector<Value>& a) {
            if (a.size() < 2) return a.empty() ? Value::nil() : a[0];
            return Value::fromNum(std::min(a[0].number, a[1].number));
        });
        registerNative("max", [](std::vector<Value>& a) {
            if (a.size() < 2) return a.empty() ? Value::nil() : a[0];
            return Value::fromNum(std::max(a[0].number, a[1].number));
        });
        registerNative("floor", [](std::vector<Value>& a) {
            return Value::fromNum(a.empty() ? 0.0 : std::floor(a[0].number));
        });
        registerNative("sqrt", [](std::vector<Value>& a) {
            return Value::fromNum(a.empty() ? 0.0 : std::sqrt(a[0].number));
        });
        registerNative("str", [](std::vector<Value>& a) {
            return Value::fromStr(a.empty() ? std::string() : a[0].toString());
        });
        // Length of arrays / dicts / strings.
        registerNative("len", [](std::vector<Value>& a) {
            if (a.empty()) return Value::fromNum(0.0);
            const Value& v = a[0];
            if (v.type == Value::Type::Array) return Value::fromNum(v.array ? static_cast<double>(v.array->size()) : 0.0);
            if (v.type == Value::Type::Dict) return Value::fromNum(v.dict ? static_cast<double>(v.dict->size()) : 0.0);
            if (v.type == Value::Type::Str) return Value::fromNum(static_cast<double>(v.str.size()));
            return Value::fromNum(0.0);
        });
        // range(n) / range(a,b) / range(a,b,step) -> array of numbers (GDScript's range()).
        registerNative("range", [](std::vector<Value>& a) {
            double start = 0.0, stop = 0.0, step = 1.0;
            if (a.size() == 1) {
                stop = a[0].number;
            } else if (a.size() >= 2) {
                start = a[0].number;
                stop = a[1].number;
                if (a.size() >= 3 && a[2].number != 0.0) {
                    step = a[2].number;
                }
            }
            Value arr = Value::newArray();
            if (step > 0.0) {
                for (double i = start; i < stop; i += step) {
                    arr.array->push_back(Value::fromNum(i));
                }
            } else if (step < 0.0) {
                for (double i = start; i > stop; i += step) {
                    arr.array->push_back(Value::fromNum(i));
                }
            }
            return arr;
        });
    }

    void registerNative(const std::string& name, std::function<Value(std::vector<Value>&)> fn) {
        Value v;
        v.type = Value::Type::Native;
        v.native = std::move(fn);
        m_global.vars[name] = std::move(v);
    }

    void setGlobal(const std::string& name, const Value& v) { m_global.vars[name] = v; }
    const Value* getGlobal(const std::string& name) const { return m_global.get(name); }

    std::function<void(const std::string&)> onPrint;
    std::string output;

    const std::string& error() const { return m_error.message; }
    int errorLine() const { return m_error.line; }

    bool run(const std::string& source) {
        m_error = {};
        m_program.clear();
        m_funcDefs.clear();
        ScriptError lexErr;
        std::vector<Token> toks = lex(source, lexErr);
        if (!lexErr.message.empty()) {
            m_error = lexErr;
            return false;
        }
        try {
            Parser parser(std::move(toks));
            m_program = parser.parse();
            hoistFunctions(m_program, m_global);
            for (const auto& s : m_program) {
                if (s->kind != Stmt::K::Func) {
                    exec(*s, m_global);
                }
            }
        } catch (const ScriptError& e) {
            m_error = e;
            return false;
        } catch (const ReturnSignal&) {
        } catch (const BreakSignal&) {
        } catch (const ContinueSignal&) {
        }
        return true;
    }

    Value call(const std::string& name, std::vector<Value> args) {
        const Value* fn = m_global.get(name);
        if (!fn) {
            m_error = {"undefined function '" + name + "'", 0};
            return Value::nil();
        }
        try {
            return invoke(*fn, args, 0);
        } catch (const ScriptError& e) {
            m_error = e;
            return Value::nil();
        }
    }

private:
    Environment m_global;
    std::vector<std::unique_ptr<Stmt>> m_program;
    std::vector<std::unique_ptr<FuncDef>> m_funcDefs;
    ScriptError m_error;

    struct ReturnSignal {
        Value value;
    };
    struct BreakSignal {};
    struct ContinueSignal {};

    void hoistFunctions(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env) {
        for (const auto& s : stmts) {
            if (s->kind == Stmt::K::Func) {
                auto def = std::make_unique<FuncDef>();
                def->name = s->name;
                def->params = s->params;
                def->body = &s->body;
                Value v;
                v.type = Value::Type::Func;
                v.func = def.get();
                env.vars[s->name] = v;
                m_funcDefs.push_back(std::move(def));
            }
        }
    }

    [[noreturn]] void fail(const std::string& msg, int line) { throw ScriptError{msg, line}; }

    void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env) {
        for (const auto& s : stmts) {
            exec(*s, env);
        }
    }

    void exec(const Stmt& s, Environment& env) {
        switch (s.kind) {
        case Stmt::K::Expr:
            eval(*s.expr, env);
            break;
        case Stmt::K::Print: {
            const std::string text = eval(*s.expr, env).toString();
            if (onPrint) {
                onPrint(text);
            } else {
                output += text;
                output.push_back('\n');
            }
            break;
        }
        case Stmt::K::Var:
            env.vars[s.name] = s.expr ? eval(*s.expr, env) : Value::nil();
            break;
        case Stmt::K::Block: {
            Environment local;
            local.parent = &env;
            execBlock(s.body, local);
            break;
        }
        case Stmt::K::If:
            if (eval(*s.expr, env).isTruthy()) {
                exec(*s.body[0], env);
            } else if (!s.elseBody.empty()) {
                exec(*s.elseBody[0], env);
            }
            break;
        case Stmt::K::While:
            while (eval(*s.expr, env).isTruthy()) {
                try {
                    exec(*s.body[0], env);
                } catch (const BreakSignal&) {
                    break;
                } catch (const ContinueSignal&) {
                }
            }
            break;
        case Stmt::K::For: {
            Environment loop;
            loop.parent = &env;
            if (!s.elseBody.empty()) {
                const Stmt& init = *s.elseBody[0];
                loop.vars[init.name] = init.expr ? eval(*init.expr, loop) : Value::nil();
            } else if (s.forInit) {
                eval(*s.forInit, loop);
            }
            while (!s.expr || eval(*s.expr, loop).isTruthy()) {
                try {
                    exec(*s.body[0], loop);
                } catch (const BreakSignal&) {
                    break;
                } catch (const ContinueSignal&) {
                }
                if (s.forPost) {
                    eval(*s.forPost, loop);
                }
            }
            break;
        }
        case Stmt::K::ForIn: {
            Value seq = eval(*s.expr, env);
            std::vector<Value> items = iterate(seq, s.line);
            Environment loop;
            loop.parent = &env;
            for (Value& item : items) {
                loop.vars[s.name] = item;
                try {
                    exec(*s.body[0], loop);
                } catch (const BreakSignal&) {
                    break;
                } catch (const ContinueSignal&) {
                }
            }
            break;
        }
        case Stmt::K::Func:
            break; // hoisted
        case Stmt::K::Return:
            throw ReturnSignal{s.expr ? eval(*s.expr, env) : Value::nil()};
        case Stmt::K::Break:
            throw BreakSignal{};
        case Stmt::K::Continue:
            throw ContinueSignal{};
        }
    }

    // Expand an iterable into a concrete list of values for a for-in loop.
    std::vector<Value> iterate(const Value& seq, int line) {
        std::vector<Value> out;
        switch (seq.type) {
        case Value::Type::Array:
            if (seq.array) {
                out = *seq.array;
            }
            break;
        case Value::Type::Dict:
            if (seq.dict) {
                for (const auto& kv : *seq.dict) {
                    out.push_back(kv.first);
                }
            }
            break;
        case Value::Type::Str:
            for (char c : seq.str) {
                out.push_back(Value::fromStr(std::string(1, c)));
            }
            break;
        default:
            fail("value is not iterable", line);
        }
        return out;
    }

    Value invoke(const Value& callee, std::vector<Value>& args, int line) {
        if (callee.type == Value::Type::Native) {
            return callee.native(args);
        }
        if (callee.type == Value::Type::Func) {
            const FuncDef* def = callee.func;
            Environment frame;
            frame.parent = &m_global;
            for (size_t i = 0; i < def->params.size(); ++i) {
                frame.vars[def->params[i]] = i < args.size() ? args[i] : Value::nil();
            }
            try {
                if (def->body) {
                    execBlock(*def->body, frame);
                }
            } catch (const ReturnSignal& r) {
                return r.value;
            }
            return Value::nil();
        }
        fail("attempt to call a non-function value", line);
    }

    // Built-in methods on arrays / dicts / strings (arr.append(x), dict.keys(), str.length(), ...).
    Value callMethod(Value& obj, const std::string& name, std::vector<Value>& args, int line) {
        if (obj.type == Value::Type::Array) {
            auto& a = *obj.array;
            if (name == "size" || name == "length") return Value::fromNum(static_cast<double>(a.size()));
            if (name == "append" || name == "push_back") { a.push_back(args.empty() ? Value::nil() : args[0]); return Value::nil(); }
            if (name == "pop_back") { if (a.empty()) return Value::nil(); Value v = a.back(); a.pop_back(); return v; }
            if (name == "clear") { a.clear(); return Value::nil(); }
            if (name == "has") { for (const Value& e : a) if (e.equals(args.empty() ? Value::nil() : args[0])) return Value::fromBool(true); return Value::fromBool(false); }
            if (name == "find") { for (size_t i = 0; i < a.size(); ++i) if (a[i].equals(args.empty() ? Value::nil() : args[0])) return Value::fromNum(static_cast<double>(i)); return Value::fromNum(-1.0); }
            fail("array has no method '" + name + "'", line);
        }
        if (obj.type == Value::Type::Dict) {
            auto& d = *obj.dict;
            const Value key = args.empty() ? Value::nil() : args[0];
            if (name == "size") return Value::fromNum(static_cast<double>(d.size()));
            if (name == "has") { for (const auto& kv : d) if (kv.first.equals(key)) return Value::fromBool(true); return Value::fromBool(false); }
            if (name == "keys") { Value r = Value::newArray(); for (const auto& kv : d) r.array->push_back(kv.first); return r; }
            if (name == "values") { Value r = Value::newArray(); for (const auto& kv : d) r.array->push_back(kv.second); return r; }
            if (name == "erase") { for (size_t i = 0; i < d.size(); ++i) if (d[i].first.equals(key)) { d.erase(d.begin() + static_cast<long>(i)); return Value::fromBool(true); } return Value::fromBool(false); }
            if (name == "clear") { d.clear(); return Value::nil(); }
            fail("dictionary has no method '" + name + "'", line);
        }
        if (obj.type == Value::Type::Str) {
            if (name == "length" || name == "size") return Value::fromNum(static_cast<double>(obj.str.size()));
            if (name == "to_upper") { std::string s = obj.str; for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return Value::fromStr(s); }
            if (name == "to_lower") { std::string s = obj.str; for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return Value::fromStr(s); }
            fail("string has no method '" + name + "'", line);
        }
        fail("value has no methods", line);
    }

    Value eval(const Expr& e, Environment& env) {
        switch (e.kind) {
        case Expr::K::Number: return Value::fromNum(e.number);
        case Expr::K::String: return Value::fromStr(e.str);
        case Expr::K::Bool: return Value::fromBool(e.boolean);
        case Expr::K::Nil: return Value::nil();
        case Expr::K::Var: {
            const Value* v = env.get(e.str);
            if (!v) {
                fail("undefined variable '" + e.str + "'", e.line);
            }
            return *v;
        }
        case Expr::K::ArrayLit: {
            Value arr = Value::newArray();
            arr.array->reserve(e.args.size());
            for (const auto& el : e.args) {
                arr.array->push_back(eval(*el, env));
            }
            return arr;
        }
        case Expr::K::DictLit: {
            Value d = Value::newDict();
            for (size_t i = 0; i < e.args.size(); ++i) {
                Value k = eval(*e.keys[i], env);
                Value v = eval(*e.args[i], env);
                dictSet(*d.dict, k, v);
            }
            return d;
        }
        case Expr::K::Index: {
            Value obj = eval(*e.lhs, env);
            Value idx = eval(*e.rhs, env);
            return indexGet(obj, idx, e.line);
        }
        case Expr::K::Get: {
            Value obj = eval(*e.lhs, env);
            if (obj.type == Value::Type::Dict) {
                return dictGet(*obj.dict, Value::fromStr(e.str));
            }
            fail("cannot read property '" + e.str + "' of this value", e.line);
        }
        case Expr::K::Assign:
            return assign(e, env);
        case Expr::K::Unary: {
            Value r = eval(*e.rhs, env);
            if (e.op == Tok::Minus) {
                return Value::fromNum(-r.number);
            }
            return Value::fromBool(!r.isTruthy());
        }
        case Expr::K::Logical: {
            Value l = eval(*e.lhs, env);
            if (e.op == Tok::Or) {
                return l.isTruthy() ? l : eval(*e.rhs, env);
            }
            return l.isTruthy() ? eval(*e.rhs, env) : l;
        }
        case Expr::K::Binary: return binary(e, env);
        case Expr::K::Call: {
            // Method call:  obj.method(args)
            if (e.callee->kind == Expr::K::Get) {
                Value obj = eval(*e.callee->lhs, env);
                std::vector<Value> args;
                args.reserve(e.args.size());
                for (const auto& a : e.args) {
                    args.push_back(eval(*a, env));
                }
                return callMethod(obj, e.callee->str, args, e.line);
            }
            // Named function call.
            if (e.callee->kind != Expr::K::Var) {
                fail("can only call functions", e.line);
            }
            const Value* fn = env.get(e.callee->str);
            if (!fn) {
                fail("undefined function '" + e.callee->str + "'", e.line);
            }
            std::vector<Value> args;
            args.reserve(e.args.size());
            for (const auto& a : e.args) {
                args.push_back(eval(*a, env));
            }
            return invoke(*fn, args, e.line);
        }
        }
        return Value::nil();
    }

    // ---- dict / index helpers ----
    static void dictSet(DictData& d, const Value& key, const Value& v) {
        for (auto& kv : d) {
            if (kv.first.equals(key)) {
                kv.second = v;
                return;
            }
        }
        d.emplace_back(key, v);
    }
    static Value dictGet(const DictData& d, const Value& key) {
        for (const auto& kv : d) {
            if (kv.first.equals(key)) {
                return kv.second;
            }
        }
        return Value::nil();
    }

    Value indexGet(const Value& obj, const Value& idx, int line) {
        if (obj.type == Value::Type::Array) {
            const long i = static_cast<long>(idx.number);
            if (!obj.array || i < 0 || i >= static_cast<long>(obj.array->size())) {
                fail("array index out of range", line);
            }
            return (*obj.array)[static_cast<size_t>(i)];
        }
        if (obj.type == Value::Type::Dict) {
            return obj.dict ? dictGet(*obj.dict, idx) : Value::nil();
        }
        if (obj.type == Value::Type::Str) {
            const long i = static_cast<long>(idx.number);
            if (i < 0 || i >= static_cast<long>(obj.str.size())) {
                fail("string index out of range", line);
            }
            return Value::fromStr(std::string(1, obj.str[static_cast<size_t>(i)]));
        }
        fail("value is not indexable", line);
    }

    Value assign(const Expr& e, Environment& env) {
        Value v = eval(*e.rhs, env);
        const Expr& target = *e.lhs;
        if (target.kind == Expr::K::Var) {
            if (!env.assign(target.str, v)) {
                fail("assignment to undefined variable '" + target.str + "'", e.line);
            }
            return v;
        }
        if (target.kind == Expr::K::Index) {
            Value obj = eval(*target.lhs, env);
            Value idx = eval(*target.rhs, env);
            if (obj.type == Value::Type::Array) {
                const long i = static_cast<long>(idx.number);
                if (!obj.array || i < 0 || i >= static_cast<long>(obj.array->size())) {
                    fail("array index out of range", e.line);
                }
                (*obj.array)[static_cast<size_t>(i)] = v;
                return v;
            }
            if (obj.type == Value::Type::Dict) {
                dictSet(*obj.dict, idx, v);
                return v;
            }
            fail("value is not indexable", e.line);
        }
        if (target.kind == Expr::K::Get) {
            Value obj = eval(*target.lhs, env);
            if (obj.type == Value::Type::Dict) {
                dictSet(*obj.dict, Value::fromStr(target.str), v);
                return v;
            }
            fail("cannot set property on this value", e.line);
        }
        fail("invalid assignment target", e.line);
    }

    bool membership(const Value& needle, const Value& hay) {
        if (hay.type == Value::Type::Array) {
            if (hay.array) {
                for (const Value& x : *hay.array) {
                    if (x.equals(needle)) {
                        return true;
                    }
                }
            }
            return false;
        }
        if (hay.type == Value::Type::Dict) {
            if (hay.dict) {
                for (const auto& kv : *hay.dict) {
                    if (kv.first.equals(needle)) {
                        return true;
                    }
                }
            }
            return false;
        }
        if (hay.type == Value::Type::Str) {
            return hay.str.find(needle.toString()) != std::string::npos;
        }
        return false;
    }

    Value binary(const Expr& e, Environment& env) {
        Value l = eval(*e.lhs, env);
        Value r = eval(*e.rhs, env);
        switch (e.op) {
        case Tok::Plus:
            if (l.type == Value::Type::Str || r.type == Value::Type::Str) {
                return Value::fromStr(l.toString() + r.toString());
            }
            if (l.type == Value::Type::Array && r.type == Value::Type::Array) {
                Value out = Value::newArray();
                if (l.array) *out.array = *l.array;
                if (r.array) out.array->insert(out.array->end(), r.array->begin(), r.array->end());
                return out;
            }
            return Value::fromNum(l.number + r.number);
        case Tok::Minus: return Value::fromNum(l.number - r.number);
        case Tok::Star: return Value::fromNum(l.number * r.number);
        case Tok::Slash:
            if (r.number == 0.0) {
                fail("division by zero", e.line);
            }
            return Value::fromNum(l.number / r.number);
        case Tok::Percent:
            if (r.number == 0.0) {
                fail("modulo by zero", e.line);
            }
            return Value::fromNum(std::fmod(l.number, r.number));
        case Tok::EqEq: return Value::fromBool(l.equals(r));
        case Tok::NotEq: return Value::fromBool(!l.equals(r));
        case Tok::Less: return Value::fromBool(l.number < r.number);
        case Tok::LessEq: return Value::fromBool(l.number <= r.number);
        case Tok::Greater: return Value::fromBool(l.number > r.number);
        case Tok::GreaterEq: return Value::fromBool(l.number >= r.number);
        case Tok::In: return Value::fromBool(membership(l, r));
        default: fail("unknown binary operator", e.line);
        }
    }
};

} // namespace maz::script
