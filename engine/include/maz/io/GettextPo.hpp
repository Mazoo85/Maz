#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

// maz::io gettext PO catalog — the industry-standard translation format, alongside Maz's existing
// CSV tables. A .po file pairs each source string (msgid) with its translation (msgstr), optionally
// under a disambiguating context (msgctxt) and with plural variants (msgid_plural + msgstr[n]).
// Crucially, PO carries a per-language PLURAL RULE — a little C expression over `n` that selects
// which plural form to use (English has 2; Polish/Arabic/Russian have 3-6 with non-trivial rules).
// This module parses PO text and answers gettext/ngettext/pgettext/npgettext lookups, evaluating the
// language's own plural rule so counts render correctly everywhere. Pure text + a small integer
// expression evaluator (the C subset gettext uses: n, literals, % * / + -, comparisons, && || !,
// and ?:) — no I/O in the core, so it unit-tests headlessly. This is Godot's Translation/PO support.
namespace maz::io {

// Evaluate a gettext plural-rule expression for a given n. Integer semantics throughout; booleans
// are 0/1. Returns 0 on a parse error (the safe "first form" default).
class PluralRule {
  public:
    PluralRule() = default;
    explicit PluralRule(std::string expr) : m_expr(std::move(expr)) {}
    void set(std::string expr) { m_expr = std::move(expr); }
    const std::string& expr() const { return m_expr; }

    long eval(long n) const {
        if (m_expr.empty()) {
            return n != 1 ? 1 : 0; // gettext default (English-like) when no rule is given
        }
        Parser p{m_expr, 0, n, false};
        const long v = p.ternary();
        return p.error ? 0 : v;
    }

  private:
    struct Parser {
        const std::string& s;
        size_t i;
        long n;
        bool error;

        void skip() {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
                ++i;
            }
        }
        bool match(const char* tok) {
            skip();
            const size_t len = std::char_traits<char>::length(tok);
            if (s.compare(i, len, tok) == 0) {
                i += len;
                return true;
            }
            return false;
        }
        char peek() {
            skip();
            return i < s.size() ? s[i] : '\0';
        }

        long ternary() {
            const long cond = logor();
            skip();
            if (i < s.size() && s[i] == '?') {
                ++i;
                const long a = ternary();
                skip();
                if (i < s.size() && s[i] == ':') {
                    ++i;
                } else {
                    error = true;
                }
                const long b = ternary();
                return cond != 0 ? a : b;
            }
            return cond;
        }
        long logor() {
            long v = logand();
            while (match("||")) {
                const long r = logand();
                v = (v != 0 || r != 0) ? 1 : 0;
            }
            return v;
        }
        long logand() {
            long v = equality();
            while (match("&&")) {
                const long r = equality();
                v = (v != 0 && r != 0) ? 1 : 0;
            }
            return v;
        }
        long equality() {
            long v = relational();
            for (;;) {
                if (match("==")) {
                    v = (v == relational()) ? 1 : 0;
                } else if (match("!=")) {
                    v = (v != relational()) ? 1 : 0;
                } else {
                    break;
                }
            }
            return v;
        }
        long relational() {
            long v = additive();
            for (;;) {
                if (match("<=")) {
                    v = (v <= additive()) ? 1 : 0;
                } else if (match(">=")) {
                    v = (v >= additive()) ? 1 : 0;
                } else if (match("<")) {
                    v = (v < additive()) ? 1 : 0;
                } else if (match(">")) {
                    v = (v > additive()) ? 1 : 0;
                } else {
                    break;
                }
            }
            return v;
        }
        long additive() {
            long v = term();
            for (;;) {
                const char c = peek();
                if (c == '+') {
                    ++i;
                    v += term();
                } else if (c == '-') {
                    ++i;
                    v -= term();
                } else {
                    break;
                }
            }
            return v;
        }
        long term() {
            long v = unary();
            for (;;) {
                const char c = peek();
                if (c == '*') {
                    ++i;
                    v *= unary();
                } else if (c == '/') {
                    ++i;
                    const long r = unary();
                    v = r != 0 ? v / r : 0;
                } else if (c == '%') {
                    ++i;
                    const long r = unary();
                    v = r != 0 ? v % r : 0;
                } else {
                    break;
                }
            }
            return v;
        }
        long unary() {
            skip();
            if (i < s.size() && s[i] == '!' && !(i + 1 < s.size() && s[i + 1] == '=')) {
                ++i;
                return unary() == 0 ? 1 : 0;
            }
            if (i < s.size() && s[i] == '-') {
                ++i;
                return -unary();
            }
            return primary();
        }
        long primary() {
            skip();
            if (i < s.size() && s[i] == '(') {
                ++i;
                const long v = ternary();
                skip();
                if (i < s.size() && s[i] == ')') {
                    ++i;
                } else {
                    error = true;
                }
                return v;
            }
            if (i < s.size() && s[i] == 'n') {
                ++i;
                return n;
            }
            if (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                long v = 0;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    v = v * 10 + (s[i] - '0');
                    ++i;
                }
                return v;
            }
            error = true;
            return 0;
        }
    };

    std::string m_expr;
};

class PoCatalog {
  public:
    // Parse PO text into the catalog. Returns true if at least one entry (beyond the header) landed.
    bool parse(const std::string& text) {
        m_entries.clear();
        m_nplurals = 2;
        m_rule.set("");

        std::string ctxt;
        std::string id;
        std::string idPlural;
        std::vector<std::string> strs;
        bool haveId = false;
        int pendingField = kNone; // which field the next bare "..." continuation appends to
        int pendingIndex = 0;

        auto flush = [&]() {
            if (haveId) {
                Entry e;
                e.plural = idPlural;
                e.strs = strs;
                m_entries[makeKey(ctxt, id)] = e;
                if (ctxt.empty() && id.empty() && !strs.empty()) {
                    parseHeader(strs[0]);
                }
            }
            ctxt.clear();
            id.clear();
            idPlural.clear();
            strs.clear();
            haveId = false;
            pendingField = kNone;
            pendingIndex = 0;
        };

        std::string line;
        size_t pos = 0;
        while (pos <= text.size()) {
            const size_t nl = text.find('\n', pos);
            line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = nl == std::string::npos ? text.size() + 1 : nl + 1;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::string trimmed = ltrim(line);
            if (trimmed.empty()) {
                flush();
                continue;
            }
            if (trimmed[0] == '#') {
                continue; // comment
            }
            if (starts(trimmed, "msgctxt")) {
                ctxt = unquoteFirst(trimmed);
                pendingField = kCtxt;
            } else if (starts(trimmed, "msgid_plural")) {
                idPlural = unquoteFirst(trimmed);
                pendingField = kPlural;
            } else if (starts(trimmed, "msgid")) {
                id = unquoteFirst(trimmed);
                haveId = true;
                pendingField = kId;
            } else if (starts(trimmed, "msgstr[")) {
                const size_t rb = trimmed.find(']');
                const int idx = rb != std::string::npos
                                    ? std::atoi(trimmed.substr(7, rb - 7).c_str())
                                    : 0;
                if (static_cast<int>(strs.size()) <= idx) {
                    strs.resize(static_cast<size_t>(idx) + 1);
                }
                strs[static_cast<size_t>(idx)] = unquoteFirst(trimmed);
                pendingField = kStr;
                pendingIndex = idx;
            } else if (starts(trimmed, "msgstr")) {
                if (strs.empty()) {
                    strs.emplace_back();
                }
                strs[0] = unquoteFirst(trimmed);
                pendingField = kStr;
                pendingIndex = 0;
            } else if (trimmed[0] == '"') {
                // Continuation line: append to the last field.
                const std::string more = unquote(trimmed);
                switch (pendingField) {
                    case kCtxt: ctxt += more; break;
                    case kId: id += more; break;
                    case kPlural: idPlural += more; break;
                    case kStr:
                        if (static_cast<size_t>(pendingIndex) < strs.size()) {
                            strs[static_cast<size_t>(pendingIndex)] += more;
                        }
                        break;
                    default: break;
                }
            }
        }
        flush();

        // The header entry (empty msgid) doesn't count as translatable content.
        return m_entries.size() > (m_entries.count(makeKey("", "")) ? 1u : 0u);
    }

    int nplurals() const { return m_nplurals; }
    const PluralRule& rule() const { return m_rule; }
    size_t size() const { return m_entries.size(); }

    // Which plural form index applies to count n (clamped into [0, nplurals-1]).
    int pluralIndex(long n) const {
        long idx = m_rule.eval(n);
        if (idx < 0) {
            idx = 0;
        }
        if (idx >= m_nplurals) {
            idx = m_nplurals - 1;
        }
        return static_cast<int>(idx);
    }

    // Plain lookup: translation of `id`, or `id` itself if untranslated.
    std::string gettext(const std::string& id) const { return lookup("", id, id); }

    // Contextual lookup.
    std::string pgettext(const std::string& ctxt, const std::string& id) const {
        return lookup(ctxt, id, id);
    }

    // Plural lookup: choose the form for n, falling back to English rules on the source strings.
    std::string ngettext(const std::string& id, const std::string& idPlural, long n) const {
        return lookupPlural("", id, idPlural, n);
    }
    std::string npgettext(const std::string& ctxt, const std::string& id,
                          const std::string& idPlural, long n) const {
        return lookupPlural(ctxt, id, idPlural, n);
    }

  private:
    enum { kNone, kCtxt, kId, kPlural, kStr };

    struct Entry {
        std::string plural;
        std::vector<std::string> strs;
    };

    static std::string makeKey(const std::string& ctxt, const std::string& id) {
        return ctxt.empty() ? id : (ctxt + std::string(1, '\x04') + id);
    }

    std::string lookup(const std::string& ctxt, const std::string& id,
                       const std::string& fallback) const {
        auto it = m_entries.find(makeKey(ctxt, id));
        if (it != m_entries.end() && !it->second.strs.empty() && !it->second.strs[0].empty()) {
            return it->second.strs[0];
        }
        return fallback;
    }

    std::string lookupPlural(const std::string& ctxt, const std::string& id,
                             const std::string& idPlural, long n) const {
        auto it = m_entries.find(makeKey(ctxt, id));
        const int idx = pluralIndex(n);
        if (it != m_entries.end() && static_cast<size_t>(idx) < it->second.strs.size() &&
            !it->second.strs[static_cast<size_t>(idx)].empty()) {
            return it->second.strs[static_cast<size_t>(idx)];
        }
        return n == 1 ? id : idPlural; // untranslated: English fallback
    }

    void parseHeader(const std::string& header) {
        // Look for "Plural-Forms: nplurals=N; plural=EXPR;" anywhere in the header text.
        const size_t pf = header.find("Plural-Forms:");
        if (pf == std::string::npos) {
            return;
        }
        const size_t np = header.find("nplurals=", pf);
        if (np != std::string::npos) {
            const int v = std::atoi(header.c_str() + np + 9);
            if (v >= 1) {
                m_nplurals = v;
            }
        }
        const size_t pl = header.find("plural=", pf);
        if (pl != std::string::npos) {
            size_t start = pl + 7;
            size_t end = header.find(';', start);
            if (end == std::string::npos) {
                end = header.find('\n', start);
            }
            if (end == std::string::npos) {
                end = header.size();
            }
            m_rule.set(trim(header.substr(start, end - start)));
        }
    }

    static bool starts(const std::string& s, const char* p) {
        return s.compare(0, std::char_traits<char>::length(p), p) == 0;
    }
    static std::string ltrim(const std::string& s) {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        return s.substr(i);
    }
    static std::string trim(const std::string& s) {
        std::string r = ltrim(s);
        while (!r.empty() && (r.back() == ' ' || r.back() == '\t')) {
            r.pop_back();
        }
        return r;
    }
    // Extract and unescape the FIRST quoted string on a line (after a keyword).
    static std::string unquoteFirst(const std::string& s) {
        const size_t q = s.find('"');
        return q == std::string::npos ? std::string() : unquote(s.substr(q));
    }
    // Unescape a token that begins with a quote; reads until the closing unescaped quote.
    static std::string unquote(const std::string& s) {
        std::string out;
        size_t i = 0;
        if (i < s.size() && s[i] == '"') {
            ++i;
        }
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                const char e = s[i + 1];
                switch (e) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    default: out.push_back(e); break;
                }
                i += 2;
            } else {
                out.push_back(s[i]);
                ++i;
            }
        }
        return out;
    }

    std::unordered_map<std::string, Entry> m_entries;
    PluralRule m_rule;
    int m_nplurals = 2;
};

} // namespace maz::io
