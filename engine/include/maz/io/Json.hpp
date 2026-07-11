#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace maz::io {

// JSON value + parser + serializer: the engine's human-readable data format, alongside the binary
// Serialize backbone. Where ByteWriter/Reader is for fast, compact save games, this is for the files
// a person (or a tool) edits: configs, tuning tables, and data-driven scenes/levels. A JsonValue is a
// tagged union over the six JSON types (null / bool / number / string / array / object); objects keep
// insertion order so a round-trip is stable and diff-friendly. parse() is a hand-written recursive
// descent scanner that is bounds-checked and never throws — malformed input yields a null value plus a
// human-readable error (line/column), so loading untrusted data fails cleanly. dump() re-serializes,
// optionally pretty-printed. Header-only, zero dependencies beyond the standard library.

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array = std::vector<JsonValue>;
    // Insertion-ordered object: a parallel key list preserves author order (std::map would re-sort).
    struct Object {
        std::vector<std::pair<std::string, JsonValue>> items;

        JsonValue* find(const std::string& key);
        const JsonValue* find(const std::string& key) const;
        JsonValue& operator[](const std::string& key);  // inserts a null if absent
        bool contains(const std::string& key) const { return find(key) != nullptr; }
        size_t size() const { return items.size(); }
    };

    // --- construction -----------------------------------------------------------------------------
    JsonValue() : m_type(Type::Null) {}
    JsonValue(std::nullptr_t) : m_type(Type::Null) {}
    JsonValue(bool b) : m_type(Type::Bool), m_bool(b) {}
    JsonValue(int n) : m_type(Type::Number), m_num(static_cast<double>(n)) {}
    JsonValue(int64_t n) : m_type(Type::Number), m_num(static_cast<double>(n)) {}
    JsonValue(double n) : m_type(Type::Number), m_num(n) {}
    JsonValue(const char* s) : m_type(Type::String), m_str(s) {}
    JsonValue(std::string s) : m_type(Type::String), m_str(std::move(s)) {}
    JsonValue(Array a) : m_type(Type::Array), m_arr(std::move(a)) {}
    JsonValue(Object o) : m_type(Type::Object), m_obj(std::move(o)) {}

    static JsonValue array() { return JsonValue(Array{}); }
    static JsonValue object() { return JsonValue(Object{}); }

    // --- type queries -----------------------------------------------------------------------------
    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }
    bool isBool() const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray() const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    // --- typed accessors with a caller-supplied default (never throw) ------------------------------
    bool asBool(bool def = false) const { return m_type == Type::Bool ? m_bool : def; }
    double asNumber(double def = 0.0) const { return m_type == Type::Number ? m_num : def; }
    float asFloat(float def = 0.0f) const {
        return m_type == Type::Number ? static_cast<float>(m_num) : def;
    }
    int asInt(int def = 0) const {
        return m_type == Type::Number ? static_cast<int>(m_num) : def;
    }
    const std::string& asString(const std::string& def = s_emptyStr) const {
        return m_type == Type::String ? m_str : def;
    }

    // --- container access -------------------------------------------------------------------------
    const Array& items() const { return m_arr; }  // valid when isArray()
    Array& items() { return m_arr; }
    const Object& fields() const { return m_obj; }  // valid when isObject()
    Object& fields() { return m_obj; }
    size_t size() const {
        return m_type == Type::Array ? m_arr.size() : m_type == Type::Object ? m_obj.size() : 0;
    }

    // Object member lookup: returns a null sentinel when absent or not an object, so chained reads
    // like doc["player"]["hp"].asInt(100) never crash on missing keys.
    const JsonValue& operator[](const std::string& key) const {
        if (m_type == Type::Object) {
            if (const JsonValue* v = m_obj.find(key)) return *v;
        }
        return s_null;
    }
    // Array index: bounds-checked, returns the null sentinel when out of range.
    const JsonValue& operator[](size_t i) const {
        return (m_type == Type::Array && i < m_arr.size()) ? m_arr[i] : s_null;
    }

    // Mutating builders (for constructing documents in code).
    void push_back(JsonValue v) {
        if (m_type != Type::Array) {
            m_type = Type::Array;
            m_arr.clear();
        }
        m_arr.push_back(std::move(v));
    }
    JsonValue& set(const std::string& key, JsonValue v) {
        if (m_type != Type::Object) {
            m_type = Type::Object;
            m_obj = Object{};
        }
        JsonValue& slot = m_obj[key];
        slot = std::move(v);
        return slot;
    }

    // --- serialization ----------------------------------------------------------------------------
    // dump() to a string; indent > 0 pretty-prints with that many spaces per level.
    std::string dump(int indent = 0) const {
        std::string out;
        writeTo(out, indent, 0);
        return out;
    }

private:
    static const JsonValue s_null;
    static const std::string s_emptyStr;

    void writeTo(std::string& out, int indent, int depth) const;
    static void writeEscaped(std::string& out, const std::string& s);

    Type m_type;
    bool m_bool = false;
    double m_num = 0.0;
    std::string m_str;
    Array m_arr;
    Object m_obj;
};

// Parse result: value + success flag + a human-readable error (with line/column) on failure.
struct JsonParseResult {
    JsonValue value;
    bool ok = false;
    std::string error;
    int line = 0;
    int column = 0;
};

JsonParseResult parseJson(const std::string& text);

// -------------------------------------------------------------------------------------------------
// Object helpers
// -------------------------------------------------------------------------------------------------
inline JsonValue* JsonValue::Object::find(const std::string& key) {
    for (auto& kv : items)
        if (kv.first == key) return &kv.second;
    return nullptr;
}
inline const JsonValue* JsonValue::Object::find(const std::string& key) const {
    for (const auto& kv : items)
        if (kv.first == key) return &kv.second;
    return nullptr;
}
inline JsonValue& JsonValue::Object::operator[](const std::string& key) {
    if (JsonValue* v = find(key)) return *v;
    items.emplace_back(key, JsonValue{});
    return items.back().second;
}

// -------------------------------------------------------------------------------------------------
// Serialization
// -------------------------------------------------------------------------------------------------
inline const JsonValue JsonValue::s_null{};
inline const std::string JsonValue::s_emptyStr{};

inline void JsonValue::writeEscaped(std::string& out, const std::string& s) {
    out += '"';
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Control character -> \u00XX.
                static const char* hex = "0123456789abcdef";
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

inline void JsonValue::writeTo(std::string& out, int indent, int depth) const {
    const bool pretty = indent > 0;
    switch (m_type) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += m_bool ? "true" : "false"; break;
    case Type::Number: {
        // Emit integral values without a trailing ".0"; otherwise a compact round-trippable form.
        if (m_num == static_cast<double>(static_cast<int64_t>(m_num)) && m_num >= -9.0e15 &&
            m_num <= 9.0e15) {
            out += std::to_string(static_cast<int64_t>(m_num));
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.17g", m_num);
            out += buf;
        }
        break;
    }
    case Type::String: writeEscaped(out, m_str); break;
    case Type::Array: {
        if (m_arr.empty()) {
            out += "[]";
            break;
        }
        out += '[';
        for (size_t i = 0; i < m_arr.size(); ++i) {
            if (pretty) {
                out += '\n';
                out.append(static_cast<size_t>(indent) * (static_cast<size_t>(depth) + 1), ' ');
            }
            m_arr[i].writeTo(out, indent, depth + 1);
            if (i + 1 < m_arr.size()) out += ',';
        }
        if (pretty) {
            out += '\n';
            out.append(static_cast<size_t>(indent) * static_cast<size_t>(depth), ' ');
        }
        out += ']';
        break;
    }
    case Type::Object: {
        if (m_obj.items.empty()) {
            out += "{}";
            break;
        }
        out += '{';
        for (size_t i = 0; i < m_obj.items.size(); ++i) {
            if (pretty) {
                out += '\n';
                out.append(static_cast<size_t>(indent) * (static_cast<size_t>(depth) + 1), ' ');
            }
            writeEscaped(out, m_obj.items[i].first);
            out += pretty ? ": " : ":";
            m_obj.items[i].second.writeTo(out, indent, depth + 1);
            if (i + 1 < m_obj.items.size()) out += ',';
        }
        if (pretty) {
            out += '\n';
            out.append(static_cast<size_t>(indent) * static_cast<size_t>(depth), ' ');
        }
        out += '}';
        break;
    }
    }
}

// -------------------------------------------------------------------------------------------------
// Parser (recursive descent, bounds-checked, never throws)
// -------------------------------------------------------------------------------------------------
namespace detail {

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : m_text(text) {}

    JsonParseResult run() {
        JsonParseResult r;
        skipWs();
        JsonValue v;
        if (!parseValue(v)) {
            r.ok = false;
            r.error = m_error;
            r.line = m_errLine;
            r.column = m_errCol;
            return r;
        }
        skipWs();
        if (m_pos != m_text.size()) {
            fail("trailing characters after JSON value");
            r.ok = false;
            r.error = m_error;
            r.line = m_errLine;
            r.column = m_errCol;
            return r;
        }
        r.value = std::move(v);
        r.ok = true;
        return r;
    }

private:
    const std::string& m_text;
    size_t m_pos = 0;
    std::string m_error;
    int m_errLine = 0;
    int m_errCol = 0;

    bool eof() const { return m_pos >= m_text.size(); }
    char peek() const { return m_text[m_pos]; }

    void lineCol(size_t pos, int& line, int& col) const {
        line = 1;
        col = 1;
        for (size_t i = 0; i < pos && i < m_text.size(); ++i) {
            if (m_text[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
    }
    bool fail(const std::string& msg) {
        if (m_error.empty()) {  // keep the first (deepest) error
            m_error = msg;
            lineCol(m_pos, m_errLine, m_errCol);
        }
        return false;
    }

    void skipWs() {
        while (!eof()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                ++m_pos;
            else
                break;
        }
    }

    bool parseValue(JsonValue& out) {
        if (eof()) return fail("unexpected end of input");
        char c = peek();
        switch (c) {
        case '{': return parseObject(out);
        case '[': return parseArray(out);
        case '"': {
            std::string s;
            if (!parseString(s)) return false;
            out = JsonValue(std::move(s));
            return true;
        }
        case 't':
        case 'f': return parseBool(out);
        case 'n': return parseNull(out);
        default:
            if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
            return fail("unexpected character");
        }
    }

    bool parseLiteral(const char* lit) {
        for (size_t i = 0; lit[i]; ++i) {
            if (eof() || peek() != lit[i]) return fail("invalid literal");
            ++m_pos;
        }
        return true;
    }
    bool parseNull(JsonValue& out) {
        if (!parseLiteral("null")) return false;
        out = JsonValue(nullptr);
        return true;
    }
    bool parseBool(JsonValue& out) {
        if (peek() == 't') {
            if (!parseLiteral("true")) return false;
            out = JsonValue(true);
        } else {
            if (!parseLiteral("false")) return false;
            out = JsonValue(false);
        }
        return true;
    }

    bool parseNumber(JsonValue& out) {
        size_t start = m_pos;
        if (!eof() && peek() == '-') ++m_pos;
        while (!eof() && peek() >= '0' && peek() <= '9') ++m_pos;
        if (!eof() && peek() == '.') {
            ++m_pos;
            while (!eof() && peek() >= '0' && peek() <= '9') ++m_pos;
        }
        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            ++m_pos;
            if (!eof() && (peek() == '+' || peek() == '-')) ++m_pos;
            while (!eof() && peek() >= '0' && peek() <= '9') ++m_pos;
        }
        if (m_pos == start) return fail("invalid number");
        std::string num = m_text.substr(start, m_pos - start);
        try {
            out = JsonValue(std::stod(num));
        } catch (...) {
            return fail("number out of range");
        }
        return true;
    }

    bool parseString(std::string& out) {
        if (eof() || peek() != '"') return fail("expected string");
        ++m_pos;  // opening quote
        while (!eof()) {
            char c = m_text[m_pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (eof()) return fail("unterminated escape");
                char e = m_text[m_pos++];
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (m_pos + 4 > m_text.size()) return fail("truncated \\u escape");
                    unsigned code = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = m_text[m_pos++];
                        code <<= 4;
                        if (h >= '0' && h <= '9')
                            code |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f')
                            code |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F')
                            code |= static_cast<unsigned>(h - 'A' + 10);
                        else
                            return fail("invalid \\u hex digit");
                    }
                    appendUtf8(out, code);
                    break;
                }
                default: return fail("invalid escape character");
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return fail("control character in string");
            } else {
                out += c;
            }
        }
        return fail("unterminated string");
    }

    static void appendUtf8(std::string& out, unsigned code) {
        // Encode a BMP code point (surrogate pairs are not decoded; each half emits its own bytes,
        // which is lossy for astral characters but never produces invalid output for the ASCII/BMP
        // data the engine's config/scene files use).
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    bool parseArray(JsonValue& out) {
        ++m_pos;  // '['
        JsonValue arr = JsonValue::array();
        skipWs();
        if (!eof() && peek() == ']') {
            ++m_pos;
            out = std::move(arr);
            return true;
        }
        while (true) {
            skipWs();
            JsonValue elem;
            if (!parseValue(elem)) return false;
            arr.items().push_back(std::move(elem));
            skipWs();
            if (eof()) return fail("unterminated array");
            char c = m_text[m_pos++];
            if (c == ']') break;
            if (c != ',') return fail("expected ',' or ']' in array");
        }
        out = std::move(arr);
        return true;
    }

    bool parseObject(JsonValue& out) {
        ++m_pos;  // '{'
        JsonValue obj = JsonValue::object();
        skipWs();
        if (!eof() && peek() == '}') {
            ++m_pos;
            out = std::move(obj);
            return true;
        }
        while (true) {
            skipWs();
            if (eof() || peek() != '"') return fail("expected string key in object");
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (eof() || m_text[m_pos] != ':') return fail("expected ':' after object key");
            ++m_pos;  // ':'
            skipWs();
            JsonValue val;
            if (!parseValue(val)) return false;
            obj.fields().items.emplace_back(std::move(key), std::move(val));
            skipWs();
            if (eof()) return fail("unterminated object");
            char c = m_text[m_pos++];
            if (c == '}') break;
            if (c != ',') return fail("expected ',' or '}' in object");
        }
        out = std::move(obj);
        return true;
    }
};

}  // namespace detail

inline JsonParseResult parseJson(const std::string& text) {
    return detail::JsonParser(text).run();
}

}  // namespace maz::io
