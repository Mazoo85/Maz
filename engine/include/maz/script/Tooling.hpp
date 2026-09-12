#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// maz::script::tooling — editor/IDE tooling for the engine's GDScript-style language: a document
// OUTLINE (the symbols a script declares — classes, functions, signals, variables) and CONTEXT-AWARE
// AUTOCOMPLETE (the candidate identifiers at a cursor position). This is the "deepen the script VM
// toward GDScript-grade tooling" side of the roadmap's scripting gap: the data an editor needs to draw
// a symbol tree, offer completions, and show a function's parameters in a tooltip.
//
// It uses its own ERROR-TOLERANT scanner rather than the VM's lex() (which bails on the first bad
// character): editors must work on half-typed, not-yet-valid code, so the scanner skips what it can't
// classify and keeps going. The token grammar mirrors the VM lexer exactly — `#` and `//` line
// comments, "..." strings, numbers, identifiers, and single-character punctuation — so the symbols it
// reports match what the VM would actually parse. Pure text analysis, no evaluation, unit-testable.
namespace maz::script::tooling {

// The language keywords (mirrors the VM lexer's keyword table). Offered as completions and excluded
// from being reported as user symbols.
inline const std::vector<std::string>& keywords() {
    static const std::vector<std::string> k = {
        "var",  "func",     "class",    "extends", "signal", "import", "if",  "else",
        "while", "for",     "in",       "return",  "break",  "continue", "and", "or",
        "true",  "false",   "nil",      "print"};
    return k;
}

inline bool isKeyword(const std::string& s) {
    const auto& k = keywords();
    return std::find(k.begin(), k.end(), s) != k.end();
}

enum class SymbolKind { Variable, Function, Class, Signal, Parameter, Keyword, Builtin };

// One declared symbol, for an outline/symbol-tree panel.
struct DocSymbol {
    std::string name;
    SymbolKind kind = SymbolKind::Variable;
    int line = 1;
    std::string container;            // enclosing class name; empty when top-level
    std::vector<std::string> params;  // parameter names, for functions
};

// A completion candidate at the cursor.
struct Completion {
    std::string label;
    SymbolKind kind = SymbolKind::Variable;
    bool operator==(const Completion& o) const { return label == o.label && kind == o.kind; }
};

namespace detail {

inline bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}
inline bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

enum class TkKind { Ident, Punct, String, Number };

struct Tk {
    TkKind kind = TkKind::Ident;
    std::string text; // identifier text, or the single punctuation character
    int line = 1;
    size_t pos = 0;   // byte offset of the token start
};

// Error-tolerant tokenizer: mirrors the VM lexer's lexical rules but never aborts.
inline std::vector<Tk> scan(const std::string& src) {
    std::vector<Tk> out;
    int line = 1;
    size_t i = 0;
    const size_t n = src.size();
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
        if (c == '#' || (c == '/' && i + 1 < n && src[i + 1] == '/')) {
            while (i < n && src[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (c == '"') {
            const size_t start = i;
            ++i;
            // Tolerant: an unterminated string ends at the newline, so later lines still scan.
            while (i < n && src[i] != '"' && src[i] != '\n') {
                if (src[i] == '\\' && i + 1 < n && src[i + 1] != '\n') {
                    ++i;
                }
                ++i;
            }
            if (i < n && src[i] == '"') {
                ++i; // closing quote (absent when unterminated — leave the newline for line counting)
            }
            out.push_back(Tk{TkKind::String, std::string(), line, start});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            const size_t start = i;
            while (i < n && (std::isdigit(static_cast<unsigned char>(src[i])) != 0 || src[i] == '.')) {
                ++i;
            }
            out.push_back(Tk{TkKind::Number, src.substr(start, i - start), line, start});
            continue;
        }
        if (isIdentStart(c)) {
            const size_t start = i;
            while (i < n && isIdentChar(src[i])) {
                ++i;
            }
            out.push_back(Tk{TkKind::Ident, src.substr(start, i - start), line, start});
            continue;
        }
        // Any single character (punctuation or otherwise) becomes a one-char punct token.
        out.push_back(Tk{TkKind::Punct, std::string(1, c), line, i});
        ++i;
    }
    return out;
}

// Collect declared symbols from a token stream. Shared by documentSymbols and completion (which scans
// only the tokens before the cursor). Tracks one level of class container via brace depth.
inline std::vector<DocSymbol> collectSymbols(const std::vector<Tk>& t) {
    std::vector<DocSymbol> syms;
    int depth = 0;
    std::string curClass;
    int curClassDepth = -1;

    auto isKw = [](const Tk& tk, const char* w) {
        return tk.kind == TkKind::Ident && tk.text == w;
    };

    for (size_t i = 0; i < t.size(); ++i) {
        const Tk& tk = t[i];
        if (tk.kind == TkKind::Punct && tk.text == "{") {
            ++depth;
            continue;
        }
        if (tk.kind == TkKind::Punct && tk.text == "}") {
            --depth;
            if (curClassDepth >= 0 && depth < curClassDepth) {
                curClass.clear();
                curClassDepth = -1;
            }
            continue;
        }
        const bool hasNext = i + 1 < t.size();
        if (isKw(tk, "class") && hasNext && t[i + 1].kind == TkKind::Ident) {
            syms.push_back(DocSymbol{t[i + 1].text, SymbolKind::Class, t[i + 1].line, curClass, {}});
            curClass = t[i + 1].text;
            curClassDepth = depth + 1; // class body opens the next brace level
        } else if (isKw(tk, "func") && hasNext && t[i + 1].kind == TkKind::Ident) {
            DocSymbol s{t[i + 1].text, SymbolKind::Function, t[i + 1].line, curClass, {}};
            // Parameters: identifiers inside the (...) after the name, skipping ": Type" annotations.
            size_t j = i + 2;
            if (j < t.size() && t[j].kind == TkKind::Punct && t[j].text == "(") {
                ++j;
                bool expectName = true;
                int paren = 1;
                while (j < t.size() && paren > 0) {
                    const Tk& p = t[j];
                    if (p.kind == TkKind::Punct && p.text == "(") {
                        ++paren;
                    } else if (p.kind == TkKind::Punct && p.text == ")") {
                        --paren;
                    } else if (p.kind == TkKind::Punct && p.text == ",") {
                        expectName = true;
                    } else if (p.kind == TkKind::Punct && p.text == ":") {
                        expectName = false; // a type annotation follows; ignore it
                    } else if (p.kind == TkKind::Ident && expectName) {
                        s.params.push_back(p.text);
                        expectName = false;
                    }
                    ++j;
                }
            }
            syms.push_back(s);
        } else if (isKw(tk, "signal") && hasNext && t[i + 1].kind == TkKind::Ident) {
            syms.push_back(DocSymbol{t[i + 1].text, SymbolKind::Signal, t[i + 1].line, curClass, {}});
        } else if (isKw(tk, "var") && hasNext && t[i + 1].kind == TkKind::Ident) {
            syms.push_back(DocSymbol{t[i + 1].text, SymbolKind::Variable, t[i + 1].line, curClass, {}});
        }
    }
    return syms;
}

} // namespace detail

// Outline: every symbol a script declares (classes, functions with parameter lists, signals, vars),
// in source order, each tagged with its kind, line, and enclosing class (if any).
inline std::vector<DocSymbol> documentSymbols(const std::string& src) {
    return detail::collectSymbols(detail::scan(src));
}

// Find a declared function by name (for signature tooltips: its .params list is the signature).
inline const DocSymbol* findFunction(const std::vector<DocSymbol>& syms, const std::string& name) {
    for (const DocSymbol& s : syms) {
        if (s.kind == SymbolKind::Function && s.name == name) {
            return &s;
        }
    }
    return nullptr;
}

// The partial identifier immediately to the left of `offset` (the text being completed). Empty if the
// character before the cursor is not part of an identifier.
inline std::string prefixAt(const std::string& src, size_t offset) {
    if (offset > src.size()) {
        offset = src.size();
    }
    size_t start = offset;
    while (start > 0 && detail::isIdentChar(src[start - 1])) {
        --start;
    }
    return src.substr(start, offset - start);
}

// True when the cursor is completing a member access (the token before the partial identifier is '.').
inline bool isMemberAccessAt(const std::string& src, size_t offset) {
    if (offset > src.size()) {
        offset = src.size();
    }
    size_t k = offset;
    while (k > 0 && detail::isIdentChar(src[k - 1])) {
        --k;
    }
    while (k > 0 && (src[k - 1] == ' ' || src[k - 1] == '\t')) {
        --k;
    }
    return k > 0 && src[k - 1] == '.';
}

// Completion candidates at a byte offset, prefix-filtered and de-duplicated. On a member access
// (`foo.<cursor>`) it offers the members declared inside classes (best-effort without type inference).
// Otherwise it offers keywords, the `print` builtin, and every symbol declared before the cursor
// (including function parameters), so the list reflects what is in scope so far.
inline std::vector<Completion> completionsAt(const std::string& src, size_t offset) {
    if (offset > src.size()) {
        offset = src.size();
    }
    const std::string prefix = prefixAt(src, offset);
    const bool member = isMemberAccessAt(src, offset);

    auto matches = [&](const std::string& label) {
        return prefix.empty() || (label.size() >= prefix.size() &&
                                  label.compare(0, prefix.size(), prefix) == 0);
    };

    std::vector<Completion> out;
    auto add = [&](const std::string& label, SymbolKind kind) {
        if (!matches(label)) {
            return;
        }
        for (const Completion& c : out) {
            if (c.label == label) {
                return; // de-dup by label
            }
        }
        out.push_back(Completion{label, kind});
    };

    // Symbols declared before the cursor (scan only the leading tokens).
    const std::vector<detail::Tk> all = detail::scan(src);
    std::vector<detail::Tk> before;
    before.reserve(all.size());
    for (const detail::Tk& tk : all) {
        if (tk.pos < offset) {
            before.push_back(tk);
        }
    }
    const std::vector<DocSymbol> syms = detail::collectSymbols(before);

    if (member) {
        // Member access: offer the members declared inside any class (methods and fields).
        for (const DocSymbol& s : syms) {
            if (!s.container.empty() &&
                (s.kind == SymbolKind::Function || s.kind == SymbolKind::Variable)) {
                add(s.name, s.kind);
            }
        }
        return out;
    }

    for (const std::string& kw : keywords()) {
        add(kw, SymbolKind::Keyword);
    }
    add("print", SymbolKind::Builtin);
    for (const DocSymbol& s : syms) {
        add(s.name, s.kind);
        for (const std::string& p : s.params) {
            add(p, SymbolKind::Parameter);
        }
    }

    std::sort(out.begin(), out.end(),
              [](const Completion& a, const Completion& b) { return a.label < b.label; });
    return out;
}

} // namespace maz::script::tooling
