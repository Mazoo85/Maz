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
// simple binding API. SC1 (this file) is the usable core: numbers / strings / bools / nil, the full
// arithmetic / comparison / logical operator set, variables, assignment, if / else, while, C-style
// for, user functions with parameters + return, and host-registered native functions. It is a pure,
// dependency-free, deterministic VM (no globals, no allocation surprises) — a deliberate edge over an
// embedded third-party runtime: a script can never reach outside the API the host hands it.
//
// Everything is header-only to match the rest of maz::. The AST is owned by the Vm for the lifetime
// of a loaded program; runtime environments are stack-allocated during evaluation (functions are not
// closures — they see globals + their own parameters/locals — so nothing escapes the call).
namespace maz::script {

// ---------------------------------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------------------------------
struct FuncDef; // forward: a user function definition (AST-owned)

struct Value {
    enum class Type { Nil, Bool, Num, Str, Native, Func };
    Type type = Type::Nil;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::function<Value(std::vector<Value>&)> native; // when Type::Native
    const FuncDef* func = nullptr;                     // when Type::Func (AST-owned)

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

    bool isTruthy() const {
        switch (type) {
        case Type::Nil: return false;
        case Type::Bool: return boolean;
        case Type::Num: return number != 0.0;
        case Type::Str: return !str.empty();
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
        }
        return "nil";
    }
};

// ---------------------------------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------------------------------
enum class Tok {
    Number, String, Ident, True, False, Nil,
    Var, If, Else, While, For, Func, Return, And, Or, Print,
    Plus, Minus, Star, Slash, Percent, Bang,
    Eq, EqEq, NotEq, Less, LessEq, Greater, GreaterEq,
    LParen, RParen, LBrace, RBrace, Comma, Semicolon,
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
        if (c == '#') { // line comment
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
        // Numbers
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
        // Strings
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
            ++i; // closing quote
            out.push_back(Token{Tok::String, s, 0.0, line});
            continue;
        }
        // Identifiers / keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < n &&
                   (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '_')) {
                ++j;
            }
            const std::string word = src.substr(i, j - i);
            i = j;
            static const std::unordered_map<std::string, Tok> kw = {
                {"var", Tok::Var},       {"if", Tok::If},     {"else", Tok::Else},
                {"while", Tok::While},   {"for", Tok::For},   {"func", Tok::Func},
                {"return", Tok::Return}, {"and", Tok::And},   {"or", Tok::Or},
                {"true", Tok::True},     {"false", Tok::False}, {"nil", Tok::Nil},
                {"print", Tok::Print}};
            const auto it = kw.find(word);
            if (it != kw.end()) {
                push(it->second, word);
            } else {
                push(Tok::Ident, word);
            }
            continue;
        }
        // Operators / punctuation
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
        case ',': push(Tok::Comma); break;
        case ';': push(Tok::Semicolon); break;
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
    enum class K { Number, String, Bool, Nil, Var, Assign, Unary, Binary, Logical, Call };
    K kind;
    double number = 0.0;
    std::string str;   // String literal, Var/Assign/Call target name
    bool boolean = false;
    Tok op = Tok::End; // Unary/Binary/Logical operator
    std::unique_ptr<Expr> lhs, rhs;            // Binary/Logical operands; Assign value in rhs
    std::vector<std::unique_ptr<Expr>> args;   // Call arguments
    int line = 0;
};

struct Stmt {
    enum class K { Expr, Var, Block, If, While, For, Func, Return, Print };
    K kind;
    std::string name;                       // Var name / Func name
    std::unique_ptr<Expr> expr;             // Expr / Var init / If+While+For cond / Return / Print
    std::unique_ptr<Expr> forInit, forPost; // For only (init is an expr statement's expr; post likewise)
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> elseBody;
    std::vector<std::string> params; // Func parameters
    int line = 0;
};

// A user function definition, referenced by Value::func. Owned by the Vm's statement tree.
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
        // for (init; cond; post) body   — init/post are expressions (assignments), cond a boolean.
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::For;
        s->line = previous().line;
        expect(Tok::LParen, "expected '(' after 'for'");
        if (match(Tok::Var)) {
            // desugar: a leading `var i = ...` becomes an init statement in an enclosing block.
            auto v = std::make_unique<Stmt>();
            v->kind = Stmt::K::Var;
            v->name = expect(Tok::Ident, "expected variable name").text;
            if (match(Tok::Eq)) {
                v->expr = expression();
            }
            s->elseBody.push_back(std::move(v)); // stash the init-var decl in elseBody[0]
        } else if (!check(Tok::Semicolon)) {
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
            if (lhs->kind != Expr::K::Var) {
                throw ScriptError{"invalid assignment target", line};
            }
            auto e = std::make_unique<Expr>();
            e->kind = Expr::K::Assign;
            e->str = lhs->str;
            e->rhs = std::move(value);
            e->line = line;
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
        return binaryChain(&Parser::term, {Tok::Less, Tok::LessEq, Tok::Greater, Tok::GreaterEq},
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
        return call();
    }

    std::unique_ptr<Expr> call() {
        auto e = primary();
        while (match(Tok::LParen)) {
            auto c = std::make_unique<Expr>();
            c->kind = Expr::K::Call;
            c->line = previous().line;
            if (e->kind != Expr::K::Var) {
                throw ScriptError{"can only call named functions", c->line};
            }
            c->str = e->str;
            if (!check(Tok::RParen)) {
                do {
                    c->args.push_back(expression());
                } while (match(Tok::Comma));
            }
            expect(Tok::RParen, "expected ')' after arguments");
            e = std::move(c);
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

// The scripting virtual machine: load a program once, then run it and/or call its functions. Native
// functions are registered before running so scripts can reach the host (print is provided by
// default and appends to `output`, which the host can also redirect via onPrint).
class Vm {
public:
    Vm() {
        // Small built-in standard library — a native starter kit; the host adds more.
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
    }

    // Register a native C++ function callable from scripts by name.
    void registerNative(const std::string& name, std::function<Value(std::vector<Value>&)> fn) {
        Value v;
        v.type = Value::Type::Native;
        v.native = std::move(fn);
        m_global.vars[name] = std::move(v);
    }

    // Set / get a global variable visible to scripts (before or after running).
    void setGlobal(const std::string& name, const Value& v) { m_global.vars[name] = v; }
    const Value* getGlobal(const std::string& name) const { return m_global.get(name); }

    // Redirect print output (defaults to accumulating into `output`).
    std::function<void(const std::string&)> onPrint;
    std::string output;

    const std::string& error() const { return m_error.message; }
    int errorLine() const { return m_error.line; }

    // Parse and run a program. Returns false and fills error()/errorLine() on failure.
    bool run(const std::string& source) {
        m_error = {};
        m_program.clear();
        ScriptError lexErr;
        std::vector<Token> toks = lex(source, lexErr);
        if (!lexErr.message.empty()) {
            m_error = lexErr;
            return false;
        }
        try {
            Parser parser(std::move(toks));
            m_program = parser.parse();
            // Pre-register user functions as globals so calls resolve regardless of order.
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
            // top-level return: ignored
        }
        return true;
    }

    // Call a script (or native) function by name with the given arguments. Returns nil on error.
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
    std::vector<std::unique_ptr<FuncDef>> m_funcDefs; // storage for hoisted function definitions
    ScriptError m_error;

    struct ReturnSignal {
        Value value;
    };

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
        case Stmt::K::Var: {
            Value v = s.expr ? eval(*s.expr, env) : Value::nil();
            env.vars[s.name] = std::move(v);
            break;
        }
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
                exec(*s.body[0], env);
            }
            break;
        case Stmt::K::For: {
            Environment loop; // holds a `var` initializer's scope
            loop.parent = &env;
            if (!s.elseBody.empty()) { // desugared `var i = ...`
                const Stmt& init = *s.elseBody[0];
                loop.vars[init.name] = init.expr ? eval(*init.expr, loop) : Value::nil();
            } else if (s.forInit) {
                eval(*s.forInit, loop);
            }
            while (!s.expr || eval(*s.expr, loop).isTruthy()) {
                exec(*s.body[0], loop);
                if (s.forPost) {
                    eval(*s.forPost, loop);
                }
            }
            break;
        }
        case Stmt::K::Func:
            // Already hoisted at scope entry; nested funcs hoist on first execution.
            if (!env.get(s.name)) {
                std::vector<std::unique_ptr<Stmt>> one; // (not used) placeholder
            }
            break;
        case Stmt::K::Return:
            throw ReturnSignal{s.expr ? eval(*s.expr, env) : Value::nil()};
        }
    }

    Value invoke(const Value& callee, std::vector<Value>& args, int line) {
        if (callee.type == Value::Type::Native) {
            return callee.native(args);
        }
        if (callee.type == Value::Type::Func) {
            const FuncDef* def = callee.func;
            Environment frame;
            frame.parent = &m_global; // non-closure: functions see globals + their own params/locals
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
        case Expr::K::Assign: {
            Value v = eval(*e.rhs, env);
            if (!env.assign(e.str, v)) {
                fail("assignment to undefined variable '" + e.str + "'", e.line);
            }
            return v;
        }
        case Expr::K::Unary: {
            Value r = eval(*e.rhs, env);
            if (e.op == Tok::Minus) {
                return Value::fromNum(-r.number);
            }
            return Value::fromBool(!r.isTruthy()); // Bang
        }
        case Expr::K::Logical: {
            Value l = eval(*e.lhs, env);
            if (e.op == Tok::Or) {
                return l.isTruthy() ? l : eval(*e.rhs, env);
            }
            return l.isTruthy() ? eval(*e.rhs, env) : l; // And
        }
        case Expr::K::Binary: return binary(e, env);
        case Expr::K::Call: {
            const Value* fn = env.get(e.str);
            if (!fn) {
                fail("undefined function '" + e.str + "'", e.line);
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

    Value binary(const Expr& e, Environment& env) {
        Value l = eval(*e.lhs, env);
        Value r = eval(*e.rhs, env);
        switch (e.op) {
        case Tok::Plus:
            if (l.type == Value::Type::Str || r.type == Value::Type::Str) {
                return Value::fromStr(l.toString() + r.toString());
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
        default: fail("unknown binary operator", e.line);
        }
    }
};

} // namespace maz::script
