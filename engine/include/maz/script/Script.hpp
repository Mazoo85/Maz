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
//   SC3: string / math / conversion stdlib + a seedable deterministic RNG + assert.
//   SC4: first-class functions — lambdas (func(x){...}), real closures capturing (and mutating) the
//        scope they were defined in, and higher-order array methods (map/filter/reduce/any/all/
//        sort/sort_custom). Environments are heap-allocated (make_shared) so a returned closure keeps
//        its captured scope alive after the enclosing call returns.
//   SC5: classes — `class Foo { var fields; func methods }` with `self`, the `_init` constructor,
//        `Foo.new(...)` / `Foo(...)` construction, single inheritance (`extends`) and `super`
//        dispatch. Instances have reference semantics; methods read off an instance are bound
//        callables. Classes are top-level, hoisted like functions.
//   SC6: host-object binding — bindClass("T").property(get,set).method(fn) exposes a C++ type;
//        makeNativeObject wraps a live host object behind a weak handle (touching a freed object is
//        a catchable error, not a crash). instantiate()/objectHasMethod()/callOn() let the engine
//        drive script instances' _ready / _process(dt) / _physics_process(dt) lifecycle hooks.
//   SC7: signals — class-level `signal name;` fields + a standalone Signal() builtin;
//        connect / disconnect / is_connected / emit / connection_count, one-shot connections, and
//        sync-vs-deferred dispatch (emit_deferred queues; the host drains it via flushDeferred()
//        for deterministic netcode ordering). Coroutine `await` is deferred to a VM-core pass.
//   SC8: safety & diagnostics — stack traces on error (stackTrace()); an execution step budget
//        (setStepBudget) and recursion limit (setRecursionLimit) that turn a runaway loop/recursion
//        into a catchable error instead of a hang/crash; and a static warnings pass (warnings())
//        for variable shadowing and unreachable code.
//   SC9: hot reload — reload(source) swaps in new code without a restart, updating global functions
//        and class method bodies IN PLACE so live instances keep their field state while gaining the
//        new behavior. A lex/parse failure leaves the previous version fully live. Old ASTs are
//        retained so still-referenced closures stay valid. (Top-level statements are not re-run.)
//   SC10: gradual typing — type hints (var x: int, func f(a: int) -> T, Array[int]) + inference
//        (var x := ...); a static type-checker flags literal-level mismatches (typeErrors()); and
//        setStrictTypes() promotes them to run() failures + enforces typed declarations at runtime.
//        Untyped code stays fully dynamic. (Typed fast-path optimization is deferred.)
//   SC11: modules & tooling — import "name" pulls a host-registered module's funcs/classes into
//        scope (registerModule; transitive + cycle-safe, no filesystem access); introspection
//        (has_method / call-by-name / get_property / set_property / has_property / class_name);
//        and debugger hooks (onStep per statement + addBreakpoint/onBreakpoint). The SC1–SC11
//        roadmap is complete; a true coroutine `await` and a typed fast-path await a VM-core pass.
//
// Everything is header-only to match the rest of maz::. The AST is owned by the Vm for the lifetime
// of a loaded program; runtime environments are reference-counted (shared_ptr) so closures capture.
namespace maz::script {

// ---------------------------------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------------------------------
struct FuncDef;     // forward: a user function definition (AST-owned)
struct Environment; // forward: a runtime scope (heap-allocated so closures can capture it)
struct ClassInfo;   // forward: a runtime class (SC5) — methods + field initializers + superclass
struct Instance;    // forward: a runtime object (SC5) — a class + its per-instance fields
struct NativeClass; // forward: a host-registered C++ type binding (SC6)
struct NativeObjectData; // forward: a live handle to a host C++ object (SC6)
struct SignalData;  // forward: a named event with connected callables (SC7)
struct Value;
using ArrayData = std::vector<Value>;
using DictData = std::vector<std::pair<Value, Value>>; // insertion-ordered, any-typed keys (like GDScript)

struct Value {
    enum class Type { Nil, Bool, Num, Str, Native, Func, Array, Dict, Class, Object, NativeObject, Signal };
    Type type = Type::Nil;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::function<Value(std::vector<Value>&)> native; // when Type::Native
    const FuncDef* func = nullptr;                     // when Type::Func (AST-owned)
    std::shared_ptr<Environment> closure;              // when Type::Func: the scope captured at definition
    std::shared_ptr<ArrayData> array;                      // when Type::Array (shared/reference semantics)
    std::shared_ptr<DictData> dict;                        // when Type::Dict
    std::shared_ptr<ClassInfo> klass;                      // when Type::Class (SC5)
    std::shared_ptr<Instance> instance;                    // when Type::Object (SC5, reference semantics)
    std::shared_ptr<NativeObjectData> nobj;                // when Type::NativeObject (SC6, host handle)
    std::shared_ptr<SignalData> sig;                       // when Type::Signal (SC7, shared/reference)

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
    static Value newSignal(std::string name = ""); // defined after SignalData (needs the complete type)

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
        case Type::Class: return klass == o.klass;
        case Type::Object: return instance == o.instance; // reference identity
        case Type::NativeObject: return nobj == o.nobj;   // handle identity
        case Type::Signal: return sig == o.sig;           // signal identity
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
        case Type::Class: return "<class>";   // detailed name handled by the Vm (needs ClassInfo)
        case Type::Object: return "<object>"; // detailed form handled by the Vm (needs Instance)
        case Type::NativeObject: return "<native object>"; // detailed form handled by the Vm
        case Type::Signal: return "<signal>"; // detailed form handled by the Vm
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
    Var, If, Else, While, For, Func, Return, And, Or, Print, In, Break, Continue, Class, Extends, Signal, Import,
    Plus, Minus, Star, Slash, Percent, Bang,
    Eq, EqEq, NotEq, Less, LessEq, Greater, GreaterEq,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Comma, Semicolon, Colon, Dot, Arrow,
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
                {"continue", Tok::Continue}, {"class", Tok::Class},  {"extends", Tok::Extends},
                {"signal", Tok::Signal},     {"import", Tok::Import}};
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
        if (two('-', '>')) { push(Tok::Arrow); i += 2; continue; } // SC10: return-type arrow
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
struct Stmt; // forward: Expr's Lambda kind owns a body of statements

struct Expr {
    enum class K {
        Number, String, Bool, Nil, Var, Assign, Unary, Binary, Logical, Call,
        ArrayLit, DictLit, Index, Get, Lambda, SignalLit
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
    std::vector<std::string> params;         // Lambda parameters
    std::vector<std::unique_ptr<Stmt>> body; // Lambda body
    mutable std::shared_ptr<FuncDef> cachedDef; // Lambda: FuncDef built once, reused
    int line = 0;
};

struct Stmt {
    enum class K { Expr, Var, Block, If, While, For, ForIn, Func, Return, Print, Break, Continue, Class, Import };
    K kind;
    std::string name;                       // Var name / Func name / ForIn loop var / Class name / Import module
    std::string superName;                  // Class: superclass name ("" if none)
    std::unique_ptr<Expr> expr;             // Expr / Var init / If+While+For cond / ForIn iterable / Return / Print
    std::unique_ptr<Expr> forInit, forPost; // C-style For only
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> elseBody; // If else, or For's desugared `var` init
    std::vector<std::string> params;             // Func parameters
    // SC10 — optional type annotations ("" = untyped/Variant; "@infer" = infer from initializer).
    std::string declType;                        // Var: declared type
    std::string returnType;                      // Func: return type
    std::vector<std::string> paramTypes;         // Func: per-parameter types (parallel to params)
    int line = 0;
};

struct FuncDef {
    std::string name;
    std::vector<std::string> params;
    const std::vector<std::unique_ptr<Stmt>>* body = nullptr;
};

// SC5 — a runtime class: its own methods + field initializers, and an optional superclass. Method
// resolution walks the super chain (findMethod reports the *defining* class so `super` can start one
// level up). Field initializers run base-first at construction so a subclass can override defaults.
struct ClassInfo : std::enable_shared_from_this<ClassInfo> {
    std::string name;
    std::shared_ptr<ClassInfo> super;                        // base class, or nullptr
    std::vector<std::pair<std::string, Value>> methods;      // name -> Type::Func Value (ordered)
    std::vector<const Stmt*> fieldInits;                     // this class's own `var` field decls

    // Find a method, walking the super chain. Reports the *defining* class (owner) so `super` can
    // resume resolution one level above it.
    const Value* findMethod(const std::string& n, std::shared_ptr<const ClassInfo>* owner = nullptr) const {
        for (const auto& m : methods) {
            if (m.first == n) {
                if (owner) *owner = shared_from_this();
                return &m.second;
            }
        }
        return super ? super->findMethod(n, owner) : nullptr;
    }

    bool isSubclassOf(const ClassInfo* other) const {
        for (const ClassInfo* c = this; c; c = c->super.get()) {
            if (c == other) return true;
        }
        return false;
    }
};

// SC5 — a runtime object (instance). Reference semantics (shared_ptr), like arrays/dicts: assigning
// an object aliases it. Fields are insertion-ordered for deterministic printing.
struct Instance {
    std::shared_ptr<ClassInfo> klass;
    std::vector<std::pair<std::string, Value>> fields;

    Value* findField(const std::string& n) {
        for (auto& f : fields) {
            if (f.first == n) return &f.second;
        }
        return nullptr;
    }
};

// SC6 — a host C++ type exposed to scripts. Register properties (get/set closures over the raw
// object pointer) and methods (closures taking the object pointer + script args). Build it fluently:
//   vm.bindClass("Sprite").property("x", getX, setX).method("move", moveFn);
// This is the sandboxing boundary: a script can only touch host state through what is registered here.
struct NativeClass {
    struct Property {
        std::function<Value(void*)> get;              // required
        std::function<void(void*, const Value&)> set; // null => read-only
    };
    using MethodFn = std::function<Value(void*, std::vector<Value>&)>;

    std::string name;
    std::vector<std::pair<std::string, Property>> properties; // ordered
    std::vector<std::pair<std::string, MethodFn>> methods;    // ordered

    NativeClass& property(const std::string& n, std::function<Value(void*)> get,
                          std::function<void(void*, const Value&)> set = nullptr) {
        properties.emplace_back(n, Property{std::move(get), std::move(set)});
        return *this;
    }
    NativeClass& method(const std::string& n, MethodFn fn) {
        methods.emplace_back(n, std::move(fn));
        return *this;
    }
    const Property* findProperty(const std::string& n) const {
        for (const auto& p : properties) {
            if (p.first == n) return &p.second;
        }
        return nullptr;
    }
    const MethodFn* findMethod(const std::string& n) const {
        for (const auto& m : methods) {
            if (m.first == n) return &m.second;
        }
        return nullptr;
    }
};

// SC6 — a script-visible handle to a live host object. The host object is held weakly, so if the
// host destroys it the script sees a clean, catchable error instead of a dangling-pointer crash
// (a deliberate edge over GDScript's freed-object footguns).
struct NativeObjectData {
    std::weak_ptr<void> handle;      // the host object; expired() => freed
    const NativeClass* cls = nullptr;
};

// SC7 — a named event with a list of connected callables. `connect` subscribes a callable, `emit`
// invokes them all in connection order; one-shot connections auto-remove after firing. Emit can be
// immediate (synchronous, deterministic) or deferred (queued, drained by the host at a safe point —
// the basis for netcode lockstep ordering). Signals have reference semantics (shared_ptr).
struct SignalData {
    struct Connection {
        Value callable;
        bool oneshot = false;
    };
    std::string name;
    std::vector<Connection> connections;

    void connect(const Value& callable, bool oneshot) {
        connections.push_back({callable, oneshot});
    }
    bool isConnected(const Value& callable) const {
        for (const auto& c : connections) {
            if (c.callable.equals(callable)) return true;
        }
        return false;
    }
    bool disconnect(const Value& callable) {
        for (size_t i = 0; i < connections.size(); ++i) {
            if (connections[i].callable.equals(callable)) {
                connections.erase(connections.begin() + static_cast<long>(i));
                return true;
            }
        }
        return false;
    }
};

inline Value Value::newSignal(std::string name) {
    Value v;
    v.type = Type::Signal;
    v.sig = std::make_shared<SignalData>();
    v.sig->name = std::move(name);
    return v;
}

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

    // Bound recursive-descent depth. Every nested parenthesis, array/dict literal, type parameter, and
    // nested `if`/block/func adds one native stack frame chain; a hostile or accidentally pathological
    // script (thousands of nested `(((`, `[[[`, or `if(1)if(1)...`, e.g. loaded from an untrusted mod)
    // would otherwise recurse until the process stack overflows and crashes — an unrecoverable DoS. Each
    // recursive parse entry point opens a DepthGuard, which fails with a clean ScriptError past the cap
    // and restores the counter on every return/throw path via RAII. Mirrors the JSON parser's depth guard.
    // The cap sits far above any legitimate source nesting while staying well under the native limit.
    static constexpr int kMaxParseDepth = 500;
    int m_depth = 0;
    struct DepthGuard {
        Parser& p;
        explicit DepthGuard(Parser& parser) : p(parser) {
            if (++p.m_depth > kMaxParseDepth) {
                --p.m_depth; // constructor throws => destructor won't run; keep the counter consistent
                p.error("maximum nesting depth exceeded");
            }
        }
        ~DepthGuard() { --p.m_depth; }
    };

    // ---- statements ----
    std::unique_ptr<Stmt> declaration() {
        DepthGuard guard(*this);
        if (match(Tok::Var)) {
            return varDecl();
        }
        if (match(Tok::Func)) {
            return funcDecl();
        }
        if (match(Tok::Class)) {
            return classDecl();
        }
        if (match(Tok::Import)) {
            auto s = std::make_unique<Stmt>();
            s->kind = Stmt::K::Import;
            s->line = previous().line;
            s->name = expect(Tok::String, "expected module name string after 'import'").text;
            expect(Tok::Semicolon, "expected ';' after import");
            return s;
        }
        return statement();
    }

    // class Name [extends Base] { var field = expr;  func method(...) { ... }  ... }
    std::unique_ptr<Stmt> classDecl() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Class;
        s->line = peek().line;
        s->name = expect(Tok::Ident, "expected class name").text;
        if (match(Tok::Extends)) {
            s->superName = expect(Tok::Ident, "expected superclass name after 'extends'").text;
        }
        expect(Tok::LBrace, "expected '{' before class body");
        while (!check(Tok::RBrace) && !isAtEnd()) {
            if (match(Tok::Var)) {
                s->body.push_back(varDecl()); // a field declaration
            } else if (match(Tok::Func)) {
                s->body.push_back(funcDecl()); // a method
            } else if (match(Tok::Signal)) {
                // `signal name;` — sugar for a field initialized to a fresh Signal.
                auto field = std::make_unique<Stmt>();
                field->kind = Stmt::K::Var;
                field->line = peek().line;
                field->name = expect(Tok::Ident, "expected signal name").text;
                auto lit = std::make_unique<Expr>();
                lit->kind = Expr::K::SignalLit;
                lit->str = field->name; // carry the signal's name for introspection
                lit->line = field->line;
                field->expr = std::move(lit);
                expect(Tok::Semicolon, "expected ';' after signal declaration");
                s->body.push_back(std::move(field));
            } else {
                error("expected 'var' field, 'func' method, or 'signal' in class body");
            }
        }
        expect(Tok::RBrace, "expected '}' after class body");
        return s;
    }

    std::unique_ptr<Stmt> varDecl() {
        auto s = std::make_unique<Stmt>();
        s->kind = Stmt::K::Var;
        s->line = peek().line;
        s->name = expect(Tok::Ident, "expected variable name").text;
        // SC10: optional type annotation — `var x: int = ...`, or inference `var x := ...`.
        if (match(Tok::Colon)) {
            if (check(Tok::Eq)) {
                s->declType = "@infer"; // `:=` — infer the type from the initializer
            } else {
                s->declType = parseTypeName();
            }
        }
        if (match(Tok::Eq)) {
            s->expr = expression();
        }
        expect(Tok::Semicolon, "expected ';' after variable declaration");
        return s;
    }

    // A type name: an identifier, optionally a container element type like `Array[int]` (parsed and
    // recorded as e.g. "Array[int]"; the checker treats the outer type as the primary constraint).
    std::string parseTypeName() {
        DepthGuard guard(*this);
        std::string t = expect(Tok::Ident, "expected type name").text;
        if (match(Tok::LBracket)) {
            std::string inner = parseTypeName();
            expect(Tok::RBracket, "expected ']' after element type");
            t += "[" + inner + "]";
        }
        return t;
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
                s->paramTypes.push_back(match(Tok::Colon) ? parseTypeName() : ""); // SC10: param type
            } while (match(Tok::Comma));
        }
        expect(Tok::RParen, "expected ')' after parameters");
        if (match(Tok::Arrow)) {
            s->returnType = parseTypeName(); // SC10: return type
        }
        expect(Tok::LBrace, "expected '{' before function body");
        s->body = block();
        return s;
    }

    std::unique_ptr<Stmt> statement() {
        DepthGuard guard(*this);
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
    std::unique_ptr<Expr> expression() {
        DepthGuard guard(*this);
        return assignment();
    }

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
        if (match(Tok::Func)) { // lambda / anonymous function expression
            e->kind = Expr::K::Lambda;
            expect(Tok::LParen, "expected '(' after 'func'");
            if (!check(Tok::RParen)) {
                do {
                    e->params.push_back(expect(Tok::Ident, "expected parameter name").text);
                    if (match(Tok::Colon)) parseTypeName(); // SC10: accept (and ignore) lambda param types
                } while (match(Tok::Comma));
            }
            expect(Tok::RParen, "expected ')' after lambda parameters");
            if (match(Tok::Arrow)) parseTypeName(); // SC10: accept lambda return type
            expect(Tok::LBrace, "expected '{' before lambda body");
            e->body = block();
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
// A runtime scope. Heap-allocated (always via make_shared) so a closure can capture the scope it was
// defined in and keep it alive after the enclosing block returns. enable_shared_from_this lets the
// interpreter obtain a shared_ptr to the current scope when building a lambda value.
struct Environment : std::enable_shared_from_this<Environment> {
    std::unordered_map<std::string, Value> vars;
    std::shared_ptr<Environment> parent;

    bool assign(const std::string& name, const Value& v) {
        for (Environment* e = this; e; e = e->parent.get()) {
            const auto it = e->vars.find(name);
            if (it != e->vars.end()) {
                it->second = v;
                return true;
            }
        }
        return false;
    }
    const Value* get(const std::string& name) const {
        for (const Environment* e = this; e; e = e->parent.get()) {
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

        // ---- SC3: math library ----
        auto num1 = [](std::vector<Value>& a) { return a.empty() ? 0.0 : a[0].number; };
        registerNative("ceil", [num1](std::vector<Value>& a) { return Value::fromNum(std::ceil(num1(a))); });
        registerNative("round", [num1](std::vector<Value>& a) { return Value::fromNum(std::round(num1(a))); });
        registerNative("sin", [num1](std::vector<Value>& a) { return Value::fromNum(std::sin(num1(a))); });
        registerNative("cos", [num1](std::vector<Value>& a) { return Value::fromNum(std::cos(num1(a))); });
        registerNative("tan", [num1](std::vector<Value>& a) { return Value::fromNum(std::tan(num1(a))); });
        registerNative("pow", [](std::vector<Value>& a) {
            return Value::fromNum(std::pow(a.size() > 0 ? a[0].number : 0.0, a.size() > 1 ? a[1].number : 0.0));
        });
        registerNative("fmod", [](std::vector<Value>& a) {
            const double d = a.size() > 1 ? a[1].number : 1.0;
            return Value::fromNum(d == 0.0 ? 0.0 : std::fmod(a.empty() ? 0.0 : a[0].number, d));
        });
        registerNative("sign", [num1](std::vector<Value>& a) {
            const double x = num1(a);
            return Value::fromNum(x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0));
        });
        registerNative("clamp", [](std::vector<Value>& a) {
            const double x = a.size() > 0 ? a[0].number : 0.0, lo = a.size() > 1 ? a[1].number : 0.0,
                         hi = a.size() > 2 ? a[2].number : 0.0;
            return Value::fromNum(x < lo ? lo : (x > hi ? hi : x));
        });
        registerNative("lerp", [](std::vector<Value>& a) {
            const double x = a.size() > 0 ? a[0].number : 0.0, y = a.size() > 1 ? a[1].number : 0.0,
                         t = a.size() > 2 ? a[2].number : 0.0;
            return Value::fromNum(x + (y - x) * t);
        });
        setGlobal("PI", Value::fromNum(3.14159265358979323846));
        setGlobal("TAU", Value::fromNum(6.28318530717958647692));

        // ---- SC3: conversions + type introspection ----
        registerNative("int", [](std::vector<Value>& a) {
            if (a.empty()) return Value::fromNum(0.0);
            const Value& v = a[0];
            if (v.type == Value::Type::Str) return Value::fromNum(std::trunc(std::strtod(v.str.c_str(), nullptr)));
            if (v.type == Value::Type::Bool) return Value::fromNum(v.boolean ? 1.0 : 0.0);
            return Value::fromNum(std::trunc(v.number));
        });
        registerNative("float", [](std::vector<Value>& a) {
            if (a.empty()) return Value::fromNum(0.0);
            const Value& v = a[0];
            if (v.type == Value::Type::Str) return Value::fromNum(std::strtod(v.str.c_str(), nullptr));
            if (v.type == Value::Type::Bool) return Value::fromNum(v.boolean ? 1.0 : 0.0);
            return Value::fromNum(v.number);
        });
        registerNative("bool", [](std::vector<Value>& a) {
            return Value::fromBool(!a.empty() && a[0].isTruthy());
        });
        registerNative("typeof", [](std::vector<Value>& a) {
            if (a.empty()) return Value::fromStr("nil");
            switch (a[0].type) {
            case Value::Type::Nil: return Value::fromStr("nil");
            case Value::Type::Bool: return Value::fromStr("bool");
            case Value::Type::Num: return Value::fromStr("number");
            case Value::Type::Str: return Value::fromStr("string");
            case Value::Type::Array: return Value::fromStr("array");
            case Value::Type::Dict: return Value::fromStr("dictionary");
            default: return Value::fromStr("function");
            }
        });
        registerNative("assert", [](std::vector<Value>& a) -> Value {
            if (a.empty() || !a[0].isTruthy()) {
                throw ScriptError{a.size() > 1 ? a[1].toString() : "assertion failed", 0};
            }
            return Value::nil();
        });

        // ---- SC3: seedable, deterministic RNG (xorshift64) — reproducible for lockstep/replay ----
        registerNative("seed", [this](std::vector<Value>& a) {
            m_rngState = a.empty() ? 0x9E3779B97F4A7C15ULL
                                   : static_cast<uint64_t>(static_cast<int64_t>(a[0].number));
            if (m_rngState == 0) {
                m_rngState = 1;
            }
            return Value::nil();
        });
        registerNative("randf", [this](std::vector<Value>&) { return Value::fromNum(rngFloat()); });
        registerNative("randi", [this](std::vector<Value>&) {
            return Value::fromNum(static_cast<double>(rngNext() >> 33));
        });
        registerNative("randf_range", [this](std::vector<Value>& a) {
            const double lo = a.size() > 0 ? a[0].number : 0.0, hi = a.size() > 1 ? a[1].number : 1.0;
            return Value::fromNum(lo + rngFloat() * (hi - lo));
        });
        registerNative("randi_range", [this](std::vector<Value>& a) {
            double lo = a.size() > 0 ? a[0].number : 0.0, hi = a.size() > 1 ? a[1].number : 0.0;
            if (hi < lo) {
                std::swap(lo, hi);
            }
            return Value::fromNum(lo + std::floor(rngFloat() * (hi - lo + 1.0)));
        });
        // SC7: a standalone signal (for objects/logic without a class-level `signal` declaration).
        registerNative("Signal", [](std::vector<Value>& a) {
            return Value::newSignal(a.empty() ? "" : a[0].toString());
        });
        // SC11: introspection — has_method / call (by name) / get / set / class_name / has_property.
        registerNative("has_method", [](std::vector<Value>& a) {
            if (a.size() < 2) return Value::fromBool(false);
            const std::string n = a[1].toString();
            if (a[0].type == Value::Type::Object && a[0].instance && a[0].instance->klass) {
                return Value::fromBool(a[0].instance->klass->findMethod(n) != nullptr);
            }
            if (a[0].type == Value::Type::NativeObject && a[0].nobj && a[0].nobj->cls) {
                return Value::fromBool(a[0].nobj->cls->findMethod(n) != nullptr);
            }
            return Value::fromBool(false);
        });
        registerNative("call", [this](std::vector<Value>& a) {
            if (a.size() < 2) return Value::nil();
            Value obj = a[0];
            const std::string n = a[1].toString();
            std::vector<Value> callArgs(a.begin() + 2, a.end());
            return callMethod(obj, n, callArgs, 0);
        });
        registerNative("get_property", [](std::vector<Value>& a) {
            if (a.size() < 2) return Value::nil();
            const std::string n = a[1].toString();
            if (a[0].type == Value::Type::Object && a[0].instance) {
                if (Value* f = a[0].instance->findField(n)) return *f;
            } else if (a[0].type == Value::Type::NativeObject && a[0].nobj && a[0].nobj->cls) {
                if (auto sp = a[0].nobj->handle.lock()) {
                    if (const auto* p = a[0].nobj->cls->findProperty(n)) return p->get(sp.get());
                }
            } else if (a[0].type == Value::Type::Dict && a[0].dict) {
                return dictGet(*a[0].dict, Value::fromStr(n));
            }
            return Value::nil();
        });
        registerNative("set_property", [](std::vector<Value>& a) {
            if (a.size() < 3) return Value::nil();
            const std::string n = a[1].toString();
            if (a[0].type == Value::Type::Object && a[0].instance) {
                if (Value* f = a[0].instance->findField(n)) { *f = a[2]; }
                else { a[0].instance->fields.emplace_back(n, a[2]); }
            } else if (a[0].type == Value::Type::NativeObject && a[0].nobj && a[0].nobj->cls) {
                if (auto sp = a[0].nobj->handle.lock()) {
                    if (const auto* p = a[0].nobj->cls->findProperty(n)) {
                        if (p->set) p->set(sp.get(), a[2]);
                    }
                }
            } else if (a[0].type == Value::Type::Dict && a[0].dict) {
                dictSet(*a[0].dict, Value::fromStr(n), a[2]);
            }
            return Value::nil();
        });
        registerNative("has_property", [](std::vector<Value>& a) {
            if (a.size() < 2) return Value::fromBool(false);
            const std::string n = a[1].toString();
            if (a[0].type == Value::Type::Object && a[0].instance) {
                return Value::fromBool(a[0].instance->findField(n) != nullptr);
            }
            if (a[0].type == Value::Type::NativeObject && a[0].nobj && a[0].nobj->cls) {
                return Value::fromBool(a[0].nobj->cls->findProperty(n) != nullptr);
            }
            if (a[0].type == Value::Type::Dict && a[0].dict) {
                for (const auto& kv : *a[0].dict) {
                    if (kv.first.type == Value::Type::Str && kv.first.str == n) return Value::fromBool(true);
                }
            }
            return Value::fromBool(false);
        });
        registerNative("class_name", [](std::vector<Value>& a) {
            if (!a.empty() && a[0].type == Value::Type::Object && a[0].instance && a[0].instance->klass) {
                return Value::fromStr(a[0].instance->klass->name);
            }
            if (!a.empty() && a[0].type == Value::Type::NativeObject && a[0].nobj && a[0].nobj->cls) {
                return Value::fromStr(a[0].nobj->cls->name);
            }
            return Value::fromStr("");
        });
    }

    void registerNative(const std::string& name, std::function<Value(std::vector<Value>&)> fn) {
        Value v;
        v.type = Value::Type::Native;
        v.native = std::move(fn);
        m_global->vars[name] = std::move(v);
    }

    void setGlobal(const std::string& name, const Value& v) { m_global->vars[name] = v; }
    const Value* getGlobal(const std::string& name) const { return m_global->get(name); }

    std::function<void(const std::string&)> onPrint;
    std::string output;

    const std::string& error() const { return m_error.message; }
    int errorLine() const { return m_error.line; }

    // ---- SC8: safety & diagnostics ---------------------------------------------------------------

    // Cap total interpreter steps per run()/call() — a runaway `while(true){}` in a mod becomes a
    // catchable error instead of a frozen game. 0 = unlimited (the default). [BETTER over GDScript.]
    void setStepBudget(size_t maxSteps) { m_stepBudget = maxSteps; }
    // Cap call nesting to catch runaway recursion before it can exhaust the native stack.
    void setRecursionLimit(size_t maxDepth) { m_maxDepth = maxDepth; }
    // The call stack captured at the most recent error, formatted innermost-first (one frame/line).
    const std::string& stackTrace() const { return m_trace; }
    // Non-fatal diagnostics collected during the last run()'s parse (shadowing, unreachable code).
    const std::vector<std::string>& warnings() const { return m_warnings; }

    // SC10 — gradual typing. Static type-check findings from the last parse (literal-level: a wrong
    // literal assigned to a typed var, a wrong literal returned from a typed function, or a wrong
    // literal argument to a typed parameter). Untyped code produces none.
    const std::vector<std::string>& typeErrors() const { return m_typeErrors; }
    // When enabled, any static type error makes run()/reload() fail (opt-in strictness); off by
    // default so existing dynamic code is unaffected. Typed declarations are also checked at runtime.
    void setStrictTypes(bool enabled) { m_strictTypes = enabled; }

    // ---- SC11: modules & tooling -----------------------------------------------------------------

    // Register a named module's source. Scripts pull it in with `import "name";`, which runs the
    // module once and exposes its top-level functions and classes. Modules come from this host
    // registry, not the filesystem — deterministic and sandboxed (no ambient file access). [BETTER.]
    void registerModule(const std::string& name, const std::string& source) { m_modules[name] = source; }

    // Debugger hooks. onStep fires before every statement with (line, functionName); a tree-walker
    // makes this trivial to expose. onBreakpoint fires when a statement's line matches a breakpoint.
    std::function<void(int line, const std::string& fn)> onStep;
    std::function<void(int line)> onBreakpoint;
    void addBreakpoint(int line) { m_breakpoints.push_back(line); }
    void clearBreakpoints() { m_breakpoints.clear(); }

    // Richer debug surface (used by script::Debugger). callDepth() is 0 in <main>, 1 inside a called
    // function, etc. callStackSnapshot() returns the live function-name stack (innermost last). The
    // debugLocals()/debugResolve() views are only meaningful while a statement is executing (i.e. from
    // inside an onStep callback), where m_debugEnv points at the current scope.
    int callDepth() const { return static_cast<int>(m_callStack.size()); }
    std::vector<std::string> callStackSnapshot() const { return m_callStack; }
    // Names+values bound in the current scope chain, excluding globals (nearest binding per name wins).
    std::vector<std::pair<std::string, Value>> debugLocals() const {
        std::vector<std::pair<std::string, Value>> out;
        std::unordered_map<std::string, bool> seen;
        for (const Environment* e = m_debugEnv; e && e != m_global.get(); e = e->parent.get()) {
            for (const auto& kv : e->vars) {
                if (!seen[kv.first]) {
                    seen[kv.first] = true;
                    out.emplace_back(kv.first, kv.second);
                }
            }
        }
        return out;
    }
    // Resolve a name through the current scope chain (locals then globals), or nullptr.
    const Value* debugResolve(const std::string& name) const {
        if (m_debugEnv) {
            if (const Value* v = m_debugEnv->get(name)) {
                return v;
            }
        }
        return m_global ? m_global->get(name) : nullptr;
    }

    bool run(const std::string& source) {
        m_error = {};
        m_trace.clear();
        m_warnings.clear();
        m_typeErrors.clear();
        m_program.clear();
        m_retained.clear();
        m_funcDefs.clear();
        m_classes.clear();
        m_deferred.clear();
        m_loadedModules.clear();
        m_importing.clear();
        m_steps = 0;
        m_depth = 0;
        ScriptError lexErr;
        std::vector<Token> toks = lex(source, lexErr);
        if (!lexErr.message.empty()) {
            m_error = lexErr;
            return false;
        }
        try {
            Parser parser(std::move(toks));
            m_program = parser.parse();
            analyzeWarnings(m_program); // SC8: static diagnostics (shadowing, unreachable code)
            checkTypes(m_program);      // SC10: static type-check pass over annotations
            if (m_strictTypes && !m_typeErrors.empty()) {
                m_error = {m_typeErrors.front(), 0};
                return false;
            }
            importModules(m_program);   // SC11: resolve `import` before the main program's own decls
            hoistFunctions(m_program, *m_global);
            hoistClasses(m_program, *m_global);
            for (const auto& s : m_program) {
                if (s->kind != Stmt::K::Func && s->kind != Stmt::K::Class && s->kind != Stmt::K::Import) {
                    exec(*s, *m_global);
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
        const Value* fn = m_global->get(name);
        if (!fn) {
            m_error = {"undefined function '" + name + "'", 0};
            return Value::nil();
        }
        m_steps = 0; // SC8: each host-driven call gets a fresh step budget
        m_depth = 0;
        try {
            return invoke(*fn, args, 0);
        } catch (const ScriptError& e) {
            m_error = e;
            return Value::nil();
        }
    }

    // ---- SC9: hot reload -------------------------------------------------------------------------

    // Swap in new source WITHOUT restarting: re-hoist global functions and update every existing
    // class's method bodies IN PLACE, so live instances keep their field values (state) while gaining
    // the new behavior. Top-level statements are NOT re-run (that would reset global state). If the
    // new source fails to lex/parse, nothing changes — the old version stays live and error() is set.
    // A pure tree-walker makes this near-instant: reparse the AST, rebind, keep state. [BETTER.]
    bool reload(const std::string& source) {
        ScriptError lexErr;
        std::vector<Token> toks = lex(source, lexErr);
        if (!lexErr.message.empty()) {
            m_error = lexErr; // keep the old program & all live state
            return false;
        }
        std::vector<std::unique_ptr<Stmt>> newProg;
        try {
            Parser parser(std::move(toks));
            newProg = parser.parse();
        } catch (const ScriptError& e) {
            m_error = e; // parse failed — old version stays live
            return false;
        }
        m_error = {};
        m_warnings.clear();
        try {
            analyzeWarnings(newProg);
            // Retire the current AST but keep it alive (old FuncDef/closure bodies may still be
            // referenced by live instances or captured lambdas until they're replaced).
            m_retained.push_back(std::move(m_program));
            m_program = std::move(newProg);
            hoistFunctions(m_program, *m_global); // new global-function bodies (overwrite old Values)
            reloadClasses(m_program);              // update classes in place; add any new ones
        } catch (const ScriptError& e) {
            m_error = e;
            return false;
        }
        return true;
    }

    // ---- SC6: host-object binding ----------------------------------------------------------------

    // Register (or fetch) a host C++ type. Chain .property()/.method() on the returned reference:
    //   vm.bindClass("Sprite").property("x", ...).method("move", ...);
    NativeClass& bindClass(const std::string& name) {
        for (auto& c : m_nativeClasses) {
            if (c->name == name) return *c;
        }
        auto c = std::make_unique<NativeClass>();
        c->name = name;
        NativeClass& ref = *c;
        m_nativeClasses.push_back(std::move(c));
        return ref;
    }

    // Wrap a live host object as a script value. The object is held weakly: when the host drops its
    // shared_ptr, scripts touching the handle get a clean catchable error (see NativeObjectData).
    Value makeNativeObject(const std::string& className, std::shared_ptr<void> obj) {
        const NativeClass* cls = nullptr;
        for (auto& c : m_nativeClasses) {
            if (c->name == className) { cls = c.get(); break; }
        }
        if (!cls) {
            m_error = {"makeNativeObject: unregistered class '" + className + "'", 0};
            return Value::nil();
        }
        Value v;
        v.type = Value::Type::NativeObject;
        v.nobj = std::make_shared<NativeObjectData>();
        v.nobj->handle = obj;
        v.nobj->cls = cls;
        return v;
    }

    // ---- SC6: lifecycle driving (host -> script) -------------------------------------------------

    // Construct a script class instance by name (like `ClassName.new(args)` from C++). Used to attach
    // a script to an engine entity. Returns nil (and sets error) if the class is unknown.
    Value instantiate(const std::string& className, std::vector<Value> args = {}) {
        const Value* cls = m_global->get(className);
        if (!cls || cls->type != Value::Type::Class) {
            m_error = {"instantiate: unknown script class '" + className + "'", 0};
            return Value::nil();
        }
        try {
            return construct(*cls, args, 0);
        } catch (const ScriptError& e) {
            m_error = e;
            return Value::nil();
        }
    }

    // Does a script object define (or inherit) this method? Lets the host skip absent hooks cheaply.
    bool objectHasMethod(const Value& obj, const std::string& method) const {
        if (obj.type != Value::Type::Object || !obj.instance || !obj.instance->klass) {
            return false;
        }
        return obj.instance->klass->findMethod(method) != nullptr;
    }

    // Invoke a method on a script object from C++ (e.g. _ready / _process(dt) / _physics_process(dt)).
    // Errors are captured into error()/errorLine() rather than thrown, so the host game loop is safe.
    Value callOn(Value& obj, const std::string& method, std::vector<Value> args) {
        try {
            return callMethod(obj, method, args, 0);
        } catch (const ScriptError& e) {
            m_error = e;
            return Value::nil();
        }
    }

    // ---- SC7: deferred signal dispatch -----------------------------------------------------------

    // Dispatch all queued (emit_deferred) signals in FIFO order and clear the queue. The host calls
    // this at a controlled point in the frame (e.g. end of the physics step) so deferred handlers run
    // deterministically — the foundation for netcode lockstep. Returns the number of emits dispatched.
    size_t flushDeferred() {
        size_t n = 0;
        // A handler may itself emit_deferred; process a snapshot so newly-queued emits wait for the
        // next flush (bounded, deterministic) rather than growing the queue mid-iteration.
        std::vector<std::pair<std::shared_ptr<SignalData>, std::vector<Value>>> batch;
        batch.swap(m_deferred);
        for (auto& e : batch) {
            try {
                emitSignal(*e.first, e.second, 0);
                ++n;
            } catch (const ScriptError& err) {
                m_error = err; // capture but keep draining the rest
            }
        }
        return n;
    }
    size_t deferredCount() const { return m_deferred.size(); }

    // Break shared_ptr reference cycles at teardown. Top-level functions and class methods capture
    // the global scope (their `closure`), while the global scope holds those same function/class
    // Values — a cycle that keeps both sides alive forever (LeakSanitizer flags it). Clearing the
    // class method tables and the global bindings drops the back-references so the interpreter's heap
    // is fully reclaimed at destruction. Functionally a no-op during the VM's life. Movable-preserving
    // defaults are declared alongside so ScriptSystem can still hold a Vm by value.
    ~Vm() { releaseCycles(); }
    Vm(Vm&&) = default;
    Vm& operator=(Vm&&) = default;

private:
    void releaseCycles() {
        for (auto& c : m_classes) {
            if (c) {
                c->methods.clear();
                c->super.reset();
                c->fieldInits.clear();
            }
        }
        for (Environment* e = m_global.get(); e; e = e->parent.get()) {
            e->vars.clear();
        }
        m_deferred.clear();
    }

    std::shared_ptr<Environment> m_global = std::make_shared<Environment>();
    std::vector<std::unique_ptr<Stmt>> m_program;
    std::vector<std::vector<std::unique_ptr<Stmt>>> m_retained; // old ASTs kept alive across hot reloads (SC9)
    std::vector<std::unique_ptr<FuncDef>> m_funcDefs;
    std::vector<std::shared_ptr<ClassInfo>> m_classes; // keep runtime classes alive (SC5)
    std::vector<std::unique_ptr<NativeClass>> m_nativeClasses; // host type bindings (SC6, stable ptrs)
    std::vector<std::pair<std::shared_ptr<SignalData>, std::vector<Value>>> m_deferred; // queued emits (SC7)
    ScriptError m_error;
    // SC8 — safety & diagnostics.
    std::string m_trace;                    // call stack captured at the last error
    std::vector<std::string> m_warnings;    // non-fatal diagnostics from the last parse
    std::vector<std::string> m_callStack;   // live call stack (function names), for trace capture
    Environment* m_debugEnv = nullptr;      // current statement's scope, for debugger variable views
    // SC10 — gradual typing.
    std::vector<std::string> m_typeErrors;  // static type-check findings from the last parse
    bool m_strictTypes = false;             // when true, type errors make run() fail
    std::unordered_map<std::string, std::pair<std::vector<std::string>, std::string>> m_sigs; // fn signatures
    // SC11 — modules & tooling.
    std::unordered_map<std::string, std::string> m_modules;   // host-registered module name -> source
    std::vector<std::string> m_importing;                     // import stack (cycle detection)
    std::vector<std::string> m_loadedModules;                 // modules already imported this run
    std::vector<int> m_breakpoints;                           // line breakpoints for the debugger
    size_t m_stepBudget = 0;                // 0 = unlimited
    size_t m_steps = 0;                     // steps taken this run/call
    size_t m_maxDepth = 1000;               // recursion cap
    size_t m_depth = 0;                     // current call depth
    uint64_t m_rngState = 0x9E3779B97F4A7C15ULL; // deterministic RNG stream (seedable via seed())

    uint64_t rngNext() {
        m_rngState ^= m_rngState << 13;
        m_rngState ^= m_rngState >> 7;
        m_rngState ^= m_rngState << 17;
        return m_rngState;
    }
    double rngFloat() { return static_cast<double>(rngNext() >> 11) / 9007199254740992.0; } // [0,1)

    struct ReturnSignal {
        Value value;
    };
    struct BreakSignal {};
    struct ContinueSignal {};

    // SC8 — a lightweight static-analysis pass over the parsed AST. Collects non-fatal warnings
    // (variable shadowing, unreachable code after return/break/continue) without stopping execution.
    void analyzeWarnings(const std::vector<std::unique_ptr<Stmt>>& program) {
        std::vector<std::vector<std::string>> scopes(1); // one scope per lexical level
        analyzeStmtList(program, scopes);
    }
    static bool declaredInScopes(const std::vector<std::vector<std::string>>& scopes,
                                 const std::string& n) {
        for (const auto& s : scopes) {
            for (const auto& d : s) {
                if (d == n) return true;
            }
        }
        return false;
    }
    void analyzeStmtList(const std::vector<std::unique_ptr<Stmt>>& stmts,
                         std::vector<std::vector<std::string>>& scopes) {
        bool terminated = false;
        for (const auto& sp : stmts) {
            const Stmt& s = *sp;
            if (terminated) {
                m_warnings.push_back("unreachable code (line " + std::to_string(s.line) +
                                     ") after return/break/continue");
                break; // one warning per dead tail is enough
            }
            analyzeStmt(s, scopes);
            if (s.kind == Stmt::K::Return || s.kind == Stmt::K::Break ||
                s.kind == Stmt::K::Continue) {
                terminated = true;
            }
        }
    }
    void analyzeStmt(const Stmt& s, std::vector<std::vector<std::string>>& scopes) {
        switch (s.kind) {
        case Stmt::K::Var:
            if (declaredInScopes(scopes, s.name)) {
                m_warnings.push_back("variable '" + s.name + "' (line " + std::to_string(s.line) +
                                     ") shadows an earlier declaration");
            }
            scopes.back().push_back(s.name);
            break;
        case Stmt::K::Block:
            scopes.emplace_back();
            analyzeStmtList(s.body, scopes);
            scopes.pop_back();
            break;
        case Stmt::K::If:
            if (!s.body.empty()) analyzeStmt(*s.body[0], scopes);
            if (!s.elseBody.empty()) analyzeStmt(*s.elseBody[0], scopes);
            break;
        case Stmt::K::While:
            if (!s.body.empty()) analyzeStmt(*s.body[0], scopes);
            break;
        case Stmt::K::For:
            scopes.emplace_back();
            if (!s.elseBody.empty()) scopes.back().push_back(s.elseBody[0]->name); // loop var
            if (!s.body.empty()) analyzeStmt(*s.body[0], scopes);
            scopes.pop_back();
            break;
        case Stmt::K::ForIn:
            scopes.emplace_back();
            scopes.back().push_back(s.name);
            if (!s.body.empty()) analyzeStmt(*s.body[0], scopes);
            scopes.pop_back();
            break;
        case Stmt::K::Func:
            scopes.emplace_back();
            for (const auto& p : s.params) scopes.back().push_back(p);
            analyzeStmtList(s.body, scopes);
            scopes.pop_back();
            break;
        case Stmt::K::Class:
            for (const auto& m : s.body) {
                if (m->kind == Stmt::K::Func) analyzeStmt(*m, scopes);
            }
            break;
        default:
            break;
        }
    }

    // ---- SC10: static type checker ---------------------------------------------------------------

    // Canonicalize a type name: fold synonyms, strip a container element (Array[int] -> "array").
    static std::string normalizeType(std::string t) {
        const size_t b = t.find('[');
        if (b != std::string::npos) t = t.substr(0, b);
        if (t == "String" || t == "str" || t == "string") return "string";
        if (t == "int") return "int";
        if (t == "float") return "float";
        if (t == "number" || t == "Number") return "number";
        if (t == "bool" || t == "Bool") return "bool";
        if (t == "Array" || t == "array") return "array";
        if (t == "Dictionary" || t == "dict" || t == "Dict") return "dict";
        if (t == "Variant" || t == "any" || t == "Any" || t.empty()) return "variant";
        return t; // a class name or unknown type — treated as an object constraint
    }
    // The category of a literal expression, or "" when it isn't a compile-time-known literal.
    static std::string literalCategory(const Expr& e) {
        switch (e.kind) {
        case Expr::K::Number: return (e.number == std::floor(e.number)) ? "int" : "float";
        case Expr::K::String: return "string";
        case Expr::K::Bool: return "bool";
        case Expr::K::Nil: return "nil";
        case Expr::K::ArrayLit: return "array";
        case Expr::K::DictLit: return "dict";
        default: return "";
        }
    }
    // Is a literal of `cat` assignable to a declared type? Unknown (non-literal) is always allowed
    // — the point of gradual typing is that only provable mismatches are flagged.
    static bool categoryAccepted(const std::string& declTypeRaw, const std::string& cat) {
        if (cat.empty() || cat == "nil") return true; // non-literal, or null (assignable to anything)
        const std::string t = normalizeType(declTypeRaw);
        if (t == "variant") return true;
        if (t == "int") return cat == "int";
        if (t == "float" || t == "number") return cat == "int" || cat == "float";
        if (t == "string") return cat == "string";
        if (t == "bool") return cat == "bool";
        if (t == "array") return cat == "array";
        if (t == "dict") return cat == "dict";
        return false; // class/object type — a primitive literal cannot satisfy it
    }
    void addTypeError(const std::string& msg, int line) {
        m_typeErrors.push_back("type error: " + msg + " (line " + std::to_string(line) + ")");
    }

    void checkTypes(const std::vector<std::unique_ptr<Stmt>>& program) {
        m_sigs.clear();
        for (const auto& s : program) {
            if (s->kind == Stmt::K::Func) m_sigs[s->name] = {s->paramTypes, s->returnType};
        }
        for (const auto& s : program) checkStmt(*s, "");
    }
    void checkStmtList(const std::vector<std::unique_ptr<Stmt>>& stmts, const std::string& retType) {
        for (const auto& s : stmts) checkStmt(*s, retType);
    }
    // `retType` is the enclosing function's declared return type (or "" at top level).
    void checkStmt(const Stmt& s, const std::string& retType) {
        switch (s.kind) {
        case Stmt::K::Var:
            if (s.expr) checkExpr(*s.expr);
            if (!s.declType.empty() && s.declType != "@infer" && s.expr) {
                const std::string cat = literalCategory(*s.expr);
                if (!categoryAccepted(s.declType, cat)) {
                    addTypeError("cannot assign " + cat + " to '" + s.name + "' of type " + s.declType,
                                 s.line);
                }
            }
            break;
        case Stmt::K::Return:
            if (s.expr) checkExpr(*s.expr);
            if (!retType.empty() && s.expr) {
                const std::string cat = literalCategory(*s.expr);
                if (!categoryAccepted(retType, cat)) {
                    addTypeError("returning " + cat + " from a function typed -> " + retType, s.line);
                }
            }
            break;
        case Stmt::K::Expr:
        case Stmt::K::Print:
            if (s.expr) checkExpr(*s.expr);
            break;
        case Stmt::K::Block:
            checkStmtList(s.body, retType);
            break;
        case Stmt::K::If:
            if (s.expr) checkExpr(*s.expr);
            if (!s.body.empty()) checkStmt(*s.body[0], retType);
            if (!s.elseBody.empty()) checkStmt(*s.elseBody[0], retType);
            break;
        case Stmt::K::While:
            if (s.expr) checkExpr(*s.expr);
            if (!s.body.empty()) checkStmt(*s.body[0], retType);
            break;
        case Stmt::K::For:
            if (s.expr) checkExpr(*s.expr);
            if (!s.body.empty()) checkStmt(*s.body[0], retType);
            break;
        case Stmt::K::ForIn:
            if (s.expr) checkExpr(*s.expr);
            if (!s.body.empty()) checkStmt(*s.body[0], retType);
            break;
        case Stmt::K::Func:
            checkStmtList(s.body, s.returnType);
            break;
        case Stmt::K::Class:
            for (const auto& m : s.body) {
                if (m->kind == Stmt::K::Func) checkStmt(*m, "");
            }
            break;
        default:
            break;
        }
    }
    void checkExpr(const Expr& e) {
        // Check literal arguments to a top-level typed function.
        if (e.kind == Expr::K::Call && e.callee && e.callee->kind == Expr::K::Var) {
            const auto it = m_sigs.find(e.callee->str);
            if (it != m_sigs.end()) {
                const auto& paramTypes = it->second.first;
                for (size_t i = 0; i < e.args.size() && i < paramTypes.size(); ++i) {
                    if (paramTypes[i].empty()) continue;
                    const std::string cat = literalCategory(*e.args[i]);
                    if (!categoryAccepted(paramTypes[i], cat)) {
                        addTypeError("argument " + std::to_string(i + 1) + " to '" + e.callee->str +
                                         "' expects " + paramTypes[i] + ", got " + cat,
                                     e.line);
                    }
                }
            }
        }
        // Recurse into sub-expressions.
        if (e.lhs) checkExpr(*e.lhs);
        if (e.rhs) checkExpr(*e.rhs);
        if (e.callee) checkExpr(*e.callee);
        for (const auto& a : e.args) checkExpr(*a);
        for (const auto& k : e.keys) checkExpr(*k);
    }

    // SC10 runtime check: does a value satisfy a declared type? Used to enforce typed `var`
    // declarations under strict mode. int/float granularity isn't enforced at runtime (all numbers
    // are doubles); category mismatches (string vs number, etc.) are.
    bool runtimeTypeAccepted(const Value& v, const std::string& declTypeRaw) {
        const std::string t = normalizeType(declTypeRaw);
        if (t == "variant") return true;
        switch (v.type) {
        case Value::Type::Nil: return true;
        case Value::Type::Num: return t == "int" || t == "float" || t == "number";
        case Value::Type::Str: return t == "string";
        case Value::Type::Bool: return t == "bool";
        case Value::Type::Array: return t == "array";
        case Value::Type::Dict: return t == "dict";
        case Value::Type::Object:
            for (const ClassInfo* c = v.instance ? v.instance->klass.get() : nullptr; c;
                 c = c->super.get()) {
                if (c->name == t) return true;
            }
            return false;
        default:
            return true; // functions / natives / signals — not enforced
        }
    }

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
                v.closure = m_global; // named functions capture globals (non-closure, as before)
                env.vars[s->name] = v;
                m_funcDefs.push_back(std::move(def));
            }
        }
    }

    // SC5 — build a runtime ClassInfo for each top-level `class` and register it as a global Value.
    // Classes are processed in source order so `extends Base` can resolve a class declared above.
    void hoistClasses(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env) {
        for (const auto& s : stmts) {
            if (s->kind != Stmt::K::Class) {
                continue;
            }
            auto info = std::make_shared<ClassInfo>();
            info->name = s->name;
            populateClassBody(*s, *info, env);
            Value cls;
            cls.type = Value::Type::Class;
            cls.klass = info;
            env.vars[s->name] = cls;
            m_classes.push_back(std::move(info));
        }
    }

    // SC11 — process every top-level `import` in a program, loading each named module once.
    void importModules(const std::vector<std::unique_ptr<Stmt>>& program) {
        for (const auto& s : program) {
            if (s->kind == Stmt::K::Import) loadModule(s->name, s->line);
        }
    }
    // Load one module: parse its source, recursively load ITS imports, then hoist its functions and
    // classes into the global scope and run its top-level setup. Idempotent; cycle-safe.
    void loadModule(const std::string& name, int line) {
        for (const auto& n : m_loadedModules) {
            if (n == name) return; // already imported
        }
        for (const auto& n : m_importing) {
            if (n == name) return; // in-progress (import cycle) — break the loop
        }
        const auto it = m_modules.find(name);
        if (it == m_modules.end()) {
            fail("unknown module '" + name + "' (register it with vm.registerModule)", line);
        }
        ScriptError lexErr;
        std::vector<Token> toks = lex(it->second, lexErr);
        if (!lexErr.message.empty()) {
            fail("module '" + name + "': " + lexErr.message, lexErr.line);
        }
        Parser parser(std::move(toks));
        std::vector<std::unique_ptr<Stmt>> modProg = parser.parse(); // ScriptError propagates
        m_importing.push_back(name);
        importModules(modProg); // nested imports resolve first
        hoistFunctions(modProg, *m_global);
        hoistClasses(modProg, *m_global);
        for (const auto& st : modProg) {
            if (st->kind != Stmt::K::Func && st->kind != Stmt::K::Class && st->kind != Stmt::K::Import) {
                exec(*st, *m_global);
            }
        }
        m_importing.pop_back();
        m_loadedModules.push_back(name);
        m_retained.push_back(std::move(modProg)); // keep the module AST alive
    }

    // SC9 — reconcile classes on hot reload. A class that still exists keeps the SAME ClassInfo
    // object (so live instances stay bound to it) but has its body repopulated with new method
    // bodies; a brand-new class is created and registered fresh. Removed classes are left in place
    // (harmless — any surviving instances keep working with their last-known methods).
    void reloadClasses(const std::vector<std::unique_ptr<Stmt>>& stmts) {
        for (const auto& s : stmts) {
            if (s->kind != Stmt::K::Class) {
                continue;
            }
            std::shared_ptr<ClassInfo> existing;
            for (const auto& c : m_classes) {
                if (c->name == s->name) { existing = c; break; }
            }
            if (existing) {
                populateClassBody(*s, *existing, *m_global); // in place → live instances keep state
            } else {
                auto info = std::make_shared<ClassInfo>();
                info->name = s->name;
                populateClassBody(*s, *info, *m_global);
                Value cls;
                cls.type = Value::Type::Class;
                cls.klass = info;
                m_global->vars[s->name] = cls;
                m_classes.push_back(std::move(info));
            }
        }
    }

    // Fill a ClassInfo's methods / field-inits / super from a `class` AST node. Used for a fresh
    // hoist (SC5) and for in-place hot reload (SC9), where reusing the same ClassInfo object keeps
    // every live instance's identity — they gain the new method bodies but keep their field values.
    void populateClassBody(const Stmt& s, ClassInfo& info, Environment& env) {
        info.super.reset();
        info.methods.clear();
        info.fieldInits.clear();
        if (!s.superName.empty()) {
            const Value* base = env.get(s.superName);
            if (!base || base->type != Value::Type::Class) {
                fail("unknown superclass '" + s.superName + "' for class '" + s.name + "'", s.line);
            }
            info.super = base->klass;
        }
        for (const auto& member : s.body) {
            if (member->kind == Stmt::K::Func) {
                auto def = std::make_unique<FuncDef>();
                def->name = member->name;
                def->params = member->params;
                def->body = &member->body;
                Value fn;
                fn.type = Value::Type::Func;
                fn.func = def.get();
                fn.closure = m_global; // methods see globals (+ self/params bound at call time)
                info.methods.emplace_back(member->name, fn);
                m_funcDefs.push_back(std::move(def));
            } else if (member->kind == Stmt::K::Var) {
                info.fieldInits.push_back(member.get());
            }
        }
    }

    // Collect a class's field initializers base-first, so a subclass field overrides a base default.
    void applyFieldInits(const ClassInfo* info, Instance& inst) {
        if (!info) {
            return;
        }
        applyFieldInits(info->super.get(), inst);
        for (const Stmt* f : info->fieldInits) {
            Value v = f->expr ? eval(*f->expr, *m_global) : Value::nil();
            if (Value* existing = inst.findField(f->name)) {
                *existing = v; // subclass overrides a base default
            } else {
                inst.fields.emplace_back(f->name, v);
            }
        }
    }

    // SC5 — construct an instance of `cls`: allocate, run field initializers, then call `_init` (if any).
    Value construct(const Value& cls, std::vector<Value>& args, int line) {
        auto inst = std::make_shared<Instance>();
        inst->klass = cls.klass;
        applyFieldInits(cls.klass.get(), *inst);
        Value self;
        self.type = Value::Type::Object;
        self.instance = inst;
        std::shared_ptr<const ClassInfo> owner;
        if (const Value* init = cls.klass->findMethod("_init", &owner)) {
            Value defCls;
            defCls.type = Value::Type::Class;
            defCls.klass = std::const_pointer_cast<ClassInfo>(owner);
            invoke(*init, args, line, &self, &defCls);
        }
        return self;
    }

    [[noreturn]] void fail(const std::string& msg, int line) {
        captureTrace(line);
        throw ScriptError{msg, line};
    }

    // SC8 — snapshot the live call stack (innermost first) at the moment of an error, before the
    // C++ exception unwinds the RAII frame guards. GDScript's errors are terse; ours name the chain.
    void captureTrace(int line) {
        std::string t;
        char at[32];
        std::snprintf(at, sizeof(at), "line %d", line);
        t += std::string("  at ") + at;
        for (auto it = m_callStack.rbegin(); it != m_callStack.rend(); ++it) {
            t += "\n  in " + *it + "()";
        }
        m_trace = t;
    }

    // SC8 — one interpreter step. Enforces the step budget so a runaway loop is a catchable error.
    void bump(int line) {
        if (m_stepBudget && ++m_steps > m_stepBudget) {
            fail("execution budget exceeded (" + std::to_string(m_stepBudget) +
                     " steps) — possible infinite loop",
                 line);
        }
    }

    void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env) {
        for (const auto& s : stmts) {
            exec(*s, env);
        }
    }

    void exec(const Stmt& s, Environment& env) {
        bump(s.line); // SC8: count a step (enforces the execution budget)
        m_debugEnv = &env; // expose the current scope for debugger variable inspection
        // SC11: debugger hooks — a tree-walker exposes stepping/breakpoints for free.
        if (onStep) {
            onStep(s.line, m_callStack.empty() ? std::string("<main>") : m_callStack.back());
        }
        if (onBreakpoint && !m_breakpoints.empty()) {
            for (int bp : m_breakpoints) {
                if (bp == s.line) { onBreakpoint(s.line); break; }
            }
        }
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
            // SC10: enforce a typed declaration at runtime under strict mode.
            if (m_strictTypes && !s.declType.empty() && s.declType != "@infer" &&
                !runtimeTypeAccepted(v, s.declType)) {
                fail("value assigned to '" + s.name + "' does not match declared type " + s.declType,
                     s.line);
            }
            env.vars[s.name] = std::move(v);
            break;
        }
        case Stmt::K::Block: {
            // Hoist nested function declarations, then run in a fresh child scope.
            auto local = std::make_shared<Environment>();
            local->parent = env.shared_from_this();
            hoistFunctions(s.body, *local);
            execBlock(s.body, *local);
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
            auto loopPtr = std::make_shared<Environment>();
            loopPtr->parent = env.shared_from_this();
            Environment& loop = *loopPtr;
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
            auto loopPtr = std::make_shared<Environment>();
            loopPtr->parent = env.shared_from_this();
            Environment& loop = *loopPtr;
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
        case Stmt::K::Class:
            break; // hoisted (see hoistClasses)
        case Stmt::K::Import:
            break; // resolved before execution (see importModules)
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

    // A method read off an instance (obj.method, without calling) becomes a Callable that remembers
    // its receiver — so it can be stored, passed around, and called later like any other function.
    Value makeBoundMethod(Value self, Value method, std::shared_ptr<const ClassInfo> owner) {
        Value defCls;
        defCls.type = Value::Type::Class;
        defCls.klass = std::const_pointer_cast<ClassInfo>(owner);
        Value bound;
        bound.type = Value::Type::Native;
        bound.native = [this, self, method, defCls](std::vector<Value>& a) -> Value {
            return invoke(method, a, 0, &self, &defCls);
        };
        return bound;
    }

    Value invoke(const Value& callee, std::vector<Value>& args, int line,
                 const Value* selfBind = nullptr, const Value* defClassBind = nullptr) {
        if (callee.type == Value::Type::Native) {
            return callee.native(args);
        }
        if (callee.type == Value::Type::Class) {
            return construct(callee, args, line); // Foo(args) constructs, like Foo.new(args)
        }
        if (callee.type == Value::Type::Func) {
            const FuncDef* def = callee.func;
            // SC8: guard against runaway recursion before it can exhaust the native stack.
            if (++m_depth > m_maxDepth) {
                --m_depth;
                fail("recursion limit exceeded (" + std::to_string(m_maxDepth) + " frames)", line);
            }
            m_callStack.push_back(def->name.empty() ? "<lambda>" : def->name);
            struct FrameGuard {
                Vm* vm;
                ~FrameGuard() {
                    --vm->m_depth;
                    if (!vm->m_callStack.empty()) vm->m_callStack.pop_back();
                }
            } guard{this};
            auto framePtr = std::make_shared<Environment>();
            framePtr->parent = callee.closure ? callee.closure : m_global; // closure scope (globals for named fns)
            Environment& frame = *framePtr;
            if (selfBind) {
                frame.vars["self"] = *selfBind; // SC5: bind the receiver inside a method
            }
            if (defClassBind) {
                frame.vars["__defclass__"] = *defClassBind; // SC5: where `super` resumes lookup
            }
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

    // SC7 — fire a signal now: invoke every connected callable in order, then drop one-shot ones.
    // Connections are snapshotted so a handler that connects/disconnects doesn't disturb this emit.
    void emitSignal(SignalData& s, std::vector<Value>& args, int line) {
        const auto snapshot = s.connections;
        for (const auto& c : snapshot) {
            Value callable = c.callable;
            std::vector<Value> callArgs = args;
            invoke(callable, callArgs, line);
        }
        // Remove one-shot connections that were present at emit time.
        for (const auto& c : snapshot) {
            if (c.oneshot) {
                s.disconnect(c.callable);
            }
        }
    }

    // Built-in methods on arrays / dicts / strings (arr.append(x), dict.keys(), str.length(), ...).
    Value callMethod(Value& obj, const std::string& name, std::vector<Value>& args, int line) {
        if (obj.type == Value::Type::Signal) {
            auto& s = *obj.sig;
            if (name == "connect") {
                if (args.empty()) fail("connect expects a callable", line);
                const bool oneshot = args.size() > 1 && args[1].isTruthy();
                s.connect(args[0], oneshot);
                return Value::nil();
            }
            if (name == "disconnect") {
                return Value::fromBool(!args.empty() && s.disconnect(args[0]));
            }
            if (name == "is_connected") {
                return Value::fromBool(!args.empty() && s.isConnected(args[0]));
            }
            if (name == "emit") {
                emitSignal(s, args, line);
                return Value::nil();
            }
            if (name == "emit_deferred") {
                m_deferred.emplace_back(obj.sig, args); // queue for flushDeferred()
                return Value::nil();
            }
            if (name == "connection_count") {
                return Value::fromNum(static_cast<double>(s.connections.size()));
            }
            if (name == "disconnect_all") {
                s.connections.clear();
                return Value::nil();
            }
            if (name == "get_name") {
                return Value::fromStr(s.name);
            }
            fail("signal has no method '" + name + "'", line);
        }
        if (obj.type == Value::Type::Object) {
            std::shared_ptr<const ClassInfo> owner;
            if (const Value* m = obj.instance->klass->findMethod(name, &owner)) {
                Value defCls;
                defCls.type = Value::Type::Class;
                defCls.klass = std::const_pointer_cast<ClassInfo>(owner);
                return invoke(*m, args, line, &obj, &defCls);
            }
            // Fall back to a callable stored in a field (e.g. obj.on_hit = func(){...}; obj.on_hit()).
            if (Value* f = obj.instance->findField(name)) {
                if (f->type == Value::Type::Func || f->type == Value::Type::Native ||
                    f->type == Value::Type::Class) {
                    return invoke(*f, args, line);
                }
            }
            fail("object of class '" + obj.instance->klass->name + "' has no method '" + name + "'", line);
        }
        if (obj.type == Value::Type::Class) {
            if (name == "new") {
                return construct(obj, args, line);
            }
            fail("class '" + obj.klass->name + "' has no static method '" + name + "'", line);
        }
        if (obj.type == Value::Type::NativeObject) {
            auto sp = obj.nobj->handle.lock();
            if (!sp) {
                fail("called method '" + name + "' on a freed '" + obj.nobj->cls->name + "' object", line);
            }
            if (const auto* m = obj.nobj->cls->findMethod(name)) {
                return (*m)(sp.get(), args);
            }
            // A registered property may hold a callable value.
            if (const auto* p = obj.nobj->cls->findProperty(name)) {
                Value fn = p->get(sp.get());
                if (fn.type == Value::Type::Func || fn.type == Value::Type::Native) {
                    return invoke(fn, args, line);
                }
            }
            fail("native '" + obj.nobj->cls->name + "' has no method '" + name + "'", line);
        }
        if (obj.type == Value::Type::Array) {
            auto& a = *obj.array;
            if (name == "size" || name == "length") return Value::fromNum(static_cast<double>(a.size()));
            if (name == "append" || name == "push_back") { a.push_back(args.empty() ? Value::nil() : args[0]); return Value::nil(); }
            if (name == "pop_back") { if (a.empty()) return Value::nil(); Value v = a.back(); a.pop_back(); return v; }
            if (name == "clear") { a.clear(); return Value::nil(); }
            if (name == "has") { for (const Value& e : a) if (e.equals(args.empty() ? Value::nil() : args[0])) return Value::fromBool(true); return Value::fromBool(false); }
            if (name == "find") { for (size_t i = 0; i < a.size(); ++i) if (a[i].equals(args.empty() ? Value::nil() : args[0])) return Value::fromNum(static_cast<double>(i)); return Value::fromNum(-1.0); }
            if (name == "join") { std::string sep = args.empty() ? "" : args[0].toString(); std::string s; for (size_t i = 0; i < a.size(); ++i) { if (i) s += sep; s += a[i].toString(); } return Value::fromStr(s); }
            if (name == "reverse") { std::reverse(a.begin(), a.end()); return Value::nil(); }
            if (name == "slice") {
                long from = args.size() > 0 ? static_cast<long>(args[0].number) : 0;
                long to = args.size() > 1 ? static_cast<long>(args[1].number) : static_cast<long>(a.size());
                const long n = static_cast<long>(a.size());
                if (from < 0) from += n;
                if (to < 0) to += n;
                from = std::max<long>(0, std::min(from, n)); to = std::max<long>(0, std::min(to, n));
                Value r = Value::newArray();
                for (long i = from; i < to; ++i) r.array->push_back(a[static_cast<size_t>(i)]);
                return r;
            }
            // ---- higher-order (SC4): each takes a callable (script func / lambda / native) ----
            if (name == "map") {
                if (args.empty()) fail("map expects a function", line);
                Value r = Value::newArray();
                for (Value& e : a) { std::vector<Value> ca{e}; r.array->push_back(invoke(args[0], ca, line)); }
                return r;
            }
            if (name == "filter") {
                if (args.empty()) fail("filter expects a function", line);
                Value r = Value::newArray();
                for (Value& e : a) { std::vector<Value> ca{e}; if (invoke(args[0], ca, line).isTruthy()) r.array->push_back(e); }
                return r;
            }
            if (name == "reduce") {
                if (args.empty()) fail("reduce expects a function", line);
                Value acc = args.size() > 1 ? args[1] : (a.empty() ? Value::nil() : a[0]);
                size_t start = args.size() > 1 ? 0 : 1;
                for (size_t i = start; i < a.size(); ++i) { std::vector<Value> ca{acc, a[i]}; acc = invoke(args[0], ca, line); }
                return acc;
            }
            if (name == "any") {
                if (args.empty()) fail("any expects a function", line);
                for (Value& e : a) { std::vector<Value> ca{e}; if (invoke(args[0], ca, line).isTruthy()) return Value::fromBool(true); }
                return Value::fromBool(false);
            }
            if (name == "all") {
                if (args.empty()) fail("all expects a function", line);
                for (Value& e : a) { std::vector<Value> ca{e}; if (!invoke(args[0], ca, line).isTruthy()) return Value::fromBool(false); }
                return Value::fromBool(true);
            }
            if (name == "sort") {
                // Stable sort by natural order: numbers ascending, then strings, then everything
                // else grouped by type tag. No comparator (use sort_custom for that).
                std::stable_sort(a.begin(), a.end(), [](const Value& x, const Value& y) {
                    if (x.type != y.type) return static_cast<int>(x.type) < static_cast<int>(y.type);
                    if (x.type == Value::Type::Num) return x.number < y.number;
                    if (x.type == Value::Type::Str) return x.str < y.str;
                    if (x.type == Value::Type::Bool) return !x.boolean && y.boolean;
                    return false;
                });
                return Value::nil();
            }
            if (name == "sort_custom") {
                if (args.empty()) fail("sort_custom expects a comparator", line);
                Value cmp = args[0];
                // Comparator returns truthy when x should come before y. Insertion sort keeps it
                // stable and calls the (interpreted) comparator a bounded number of times.
                for (size_t i = 1; i < a.size(); ++i) {
                    Value key = a[i];
                    long j = static_cast<long>(i) - 1;
                    while (j >= 0) {
                        std::vector<Value> ca{key, a[static_cast<size_t>(j)]};
                        if (!invoke(cmp, ca, line).isTruthy()) break;
                        a[static_cast<size_t>(j + 1)] = a[static_cast<size_t>(j)];
                        --j;
                    }
                    a[static_cast<size_t>(j + 1)] = key;
                }
                return Value::nil();
            }
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
            if (name == "substr") {
                long from = args.size() > 0 ? static_cast<long>(args[0].number) : 0;
                const long slen = args.size() > 1 ? static_cast<long>(args[1].number) : -1;
                if (from < 0) from = 0;
                if (from > static_cast<long>(obj.str.size())) from = static_cast<long>(obj.str.size());
                return Value::fromStr(slen < 0 ? obj.str.substr(static_cast<size_t>(from))
                                               : obj.str.substr(static_cast<size_t>(from), static_cast<size_t>(slen)));
            }
            if (name == "find") { const std::string p = args.empty() ? "" : args[0].toString(); const size_t pos = obj.str.find(p); return Value::fromNum(pos == std::string::npos ? -1.0 : static_cast<double>(pos)); }
            if (name == "begins_with") { const std::string p = args.empty() ? "" : args[0].toString(); return Value::fromBool(obj.str.rfind(p, 0) == 0); }
            if (name == "ends_with") { const std::string p = args.empty() ? "" : args[0].toString(); return Value::fromBool(p.size() <= obj.str.size() && obj.str.compare(obj.str.size() - p.size(), p.size(), p) == 0); }
            if (name == "replace") {
                const std::string from = args.size() > 0 ? args[0].toString() : "";
                const std::string to = args.size() > 1 ? args[1].toString() : "";
                if (from.empty()) return obj;
                std::string s = obj.str;
                size_t pos = 0;
                while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
                return Value::fromStr(s);
            }
            if (name == "split") {
                const std::string sep = args.empty() ? " " : args[0].toString();
                Value r = Value::newArray();
                if (sep.empty()) { for (char c : obj.str) r.array->push_back(Value::fromStr(std::string(1, c))); return r; }
                size_t prev = 0, pos;
                while ((pos = obj.str.find(sep, prev)) != std::string::npos) { r.array->push_back(Value::fromStr(obj.str.substr(prev, pos - prev))); prev = pos + sep.size(); }
                r.array->push_back(Value::fromStr(obj.str.substr(prev)));
                return r;
            }
            if (name == "strip" || name == "strip_edges") {
                const size_t a0 = obj.str.find_first_not_of(" \t\r\n");
                if (a0 == std::string::npos) return Value::fromStr("");
                const size_t b0 = obj.str.find_last_not_of(" \t\r\n");
                return Value::fromStr(obj.str.substr(a0, b0 - a0 + 1));
            }
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
            if (obj.type == Value::Type::Object) {
                if (Value* f = obj.instance->findField(e.str)) {
                    return *f;
                }
                // A method read (without calling) yields a bound callable capturing `self`.
                std::shared_ptr<const ClassInfo> owner;
                if (const Value* m = obj.instance->klass->findMethod(e.str, &owner)) {
                    return makeBoundMethod(obj, *m, owner);
                }
                fail("object of class '" + obj.instance->klass->name + "' has no property '" + e.str + "'", e.line);
            }
            if (obj.type == Value::Type::Class) {
                // Class-level access: a method reference (e.g. for passing around) or `new` sentinel.
                std::shared_ptr<const ClassInfo> owner;
                if (const Value* m = obj.klass->findMethod(e.str, &owner)) {
                    return *m;
                }
                fail("class '" + obj.klass->name + "' has no static member '" + e.str + "'", e.line);
            }
            if (obj.type == Value::Type::NativeObject) {
                auto sp = obj.nobj->handle.lock();
                if (!sp) {
                    fail("read property '" + e.str + "' on a freed '" + obj.nobj->cls->name + "' object", e.line);
                }
                if (const auto* p = obj.nobj->cls->findProperty(e.str)) {
                    return p->get(sp.get());
                }
                fail("native '" + obj.nobj->cls->name + "' has no property '" + e.str + "'", e.line);
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
        case Expr::K::Lambda: {
            // Anonymous function expression. Build its FuncDef once (cached on the AST node so a
            // lambda inside a loop doesn't leak a new def per iteration), then capture the current
            // scope so the closure sees the surrounding locals.
            if (!e.cachedDef) {
                auto def = std::make_shared<FuncDef>();
                def->name = "<lambda>";
                def->params = e.params;
                def->body = &e.body;
                e.cachedDef = def; // stays alive as long as the AST node does
            }
            Value v;
            v.type = Value::Type::Func;
            v.func = e.cachedDef.get();
            v.closure = env.shared_from_this(); // capture the defining scope (real closure)
            return v;
        }
        case Expr::K::SignalLit:
            return Value::newSignal(e.str); // fresh per-instance signal (from `signal name;`)
        case Expr::K::Call: {
            // Method call:  obj.method(args)
            if (e.callee->kind == Expr::K::Get) {
                // super.method(args) — dispatch starting one level above the defining class (SC5).
                if (e.callee->lhs->kind == Expr::K::Var && e.callee->lhs->str == "super") {
                    const Value* selfV = env.get("self");
                    const Value* defV = env.get("__defclass__");
                    if (!selfV || !defV || defV->type != Value::Type::Class) {
                        fail("'super' can only be used inside a method", e.line);
                    }
                    const std::shared_ptr<ClassInfo>& defC = defV->klass;
                    if (!defC || !defC->super) {
                        fail("'super' has no base class in '" + (defC ? defC->name : std::string()) + "'", e.line);
                    }
                    std::vector<Value> args;
                    args.reserve(e.args.size());
                    for (const auto& a : e.args) {
                        args.push_back(eval(*a, env));
                    }
                    std::shared_ptr<const ClassInfo> owner;
                    const Value* m = defC->super->findMethod(e.callee->str, &owner);
                    if (!m) {
                        fail("superclass has no method '" + e.callee->str + "'", e.line);
                    }
                    Value newDef;
                    newDef.type = Value::Type::Class;
                    newDef.klass = std::const_pointer_cast<ClassInfo>(owner);
                    return invoke(*m, args, e.line, selfV, &newDef);
                }
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
            if (obj.type == Value::Type::Object) {
                if (Value* f = obj.instance->findField(target.str)) {
                    *f = v;
                } else {
                    obj.instance->fields.emplace_back(target.str, v); // allow dynamic field creation
                }
                return v;
            }
            if (obj.type == Value::Type::NativeObject) {
                auto sp = obj.nobj->handle.lock();
                if (!sp) {
                    fail("set property '" + target.str + "' on a freed '" + obj.nobj->cls->name + "' object", e.line);
                }
                const auto* p = obj.nobj->cls->findProperty(target.str);
                if (!p) {
                    fail("native '" + obj.nobj->cls->name + "' has no property '" + target.str + "'", e.line);
                }
                if (!p->set) {
                    fail("property '" + target.str + "' of '" + obj.nobj->cls->name + "' is read-only", e.line);
                }
                p->set(sp.get(), v);
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
