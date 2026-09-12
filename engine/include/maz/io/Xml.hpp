#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace maz::io {

// Pull-style XML reader — Godot's XMLParser: a forward, streaming tokenizer that hands back one node at
// a time (`read()` advances; the accessors describe the node just read) instead of building a DOM tree
// in memory. That is the shape you want for reading big or foreign documents — a Tiled `.tmx` tilemap, an
// SVG path set, a COLLADA model, an RSS feed, an app config in XML — where you walk the stream and pull
// out the handful of elements you care about. Maz had text formats (JSON, CSV, .tres/PrefabText) and
// binary (Serialize, ResourcePack) but no XML at all, so any XML-shaped asset was unreadable.
//
// The parser recognises elements (`<a x="1">`), self-closing elements (`<br/>`), end tags (`</a>`), text
// runs, comments (`<!-- … -->`), CDATA (`<![CDATA[ … ]]>`), and processing / declaration nodes
// (`<?xml … ?>`). Attribute values and text runs are entity-decoded (`&lt; &gt; &amp; &quot; &apos;` and
// numeric `&#NN;` / `&#xHH;`, UTF-8 encoded). `depth()` reports the count of currently-open ancestor
// elements so a caller can indent or scope without tracking a stack by hand. Header-only, no allocation
// beyond the node's own strings, deterministic — it unit-tests exactly and drives a golden.
//
// Scope note (honest): this is a well-formed-input pull parser, not a validator. It does not check tag
// nesting/matching, resolve namespaces, expand DTD/custom entities, or enforce a schema; malformed input
// (an unterminated tag or quote) sets the error flag and stops rather than recovering. Those remain
// follow-ups; a full validating/DOM parser is out of scope for a header-only module.

class XmlParser {
public:
    enum class NodeType { None, Element, ElementEnd, Text, Comment, CData, Declaration };

    // Point the parser at a document and reset to the start. Returns true (kept for symmetry with a
    // future file loader).
    bool parse(const std::string& text) {
        m_buf = text;
        m_pos = 0;
        m_type = NodeType::None;
        m_name.clear();
        m_attrs.clear();
        m_empty = false;
        m_depth = 0;
        m_reportDepth = 0;
        m_error = false;
        m_errorText.clear();
        return true;
    }

    // Advance to the next node. Returns true if a node was read, false at end-of-document or on error.
    bool read() {
        m_attrs.clear();
        m_empty = false;
        m_name.clear();
        m_reportDepth = m_depth; // text-like and block nodes sit at the current open depth
        if (m_error || m_pos >= m_buf.size()) {
            m_type = NodeType::None;
            return false;
        }

        if (m_buf[m_pos] == '<') {
            return readTag();
        }
        return readText();
    }

    NodeType nodeType() const { return m_type; }

    // Element / ElementEnd tag name. (Empty for text-like nodes; use nodeData() there.)
    const std::string& nodeName() const { return m_name; }

    // Text / Comment / CData content (entity-decoded for Text). Shares storage with nodeName().
    const std::string& nodeData() const { return m_name; }

    // True when the current Element is self-closing (`<br/>`) — it has no matching end tag.
    bool isEmpty() const { return m_empty; }

    // Depth of the current node: 0 at the root element, 1 for its direct children, and the matching end
    // tag reports the same depth as its start tag.
    int depth() const { return m_reportDepth; }

    std::size_t attributeCount() const { return m_attrs.size(); }
    const std::string& attributeName(std::size_t i) const { return m_attrs[i].first; }
    const std::string& attributeValue(std::size_t i) const { return m_attrs[i].second; }

    bool hasAttribute(const std::string& name) const {
        for (const auto& a : m_attrs) {
            if (a.first == name) {
                return true;
            }
        }
        return false;
    }

    // Attribute value by name, or `def` if absent.
    std::string getAttribute(const std::string& name, const std::string& def = std::string()) const {
        for (const auto& a : m_attrs) {
            if (a.first == name) {
                return a.second;
            }
        }
        return def;
    }

    bool hasError() const { return m_error; }
    const std::string& errorText() const { return m_errorText; }

private:
    static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

    void fail(const char* msg) {
        m_error = true;
        m_errorText = msg;
        m_type = NodeType::None;
    }

    // Append a Unicode code point as UTF-8.
    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // Replace the five predefined entities and numeric character references; unknown entities pass
    // through literally.
    static std::string decodeEntities(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (std::size_t i = 0; i < s.size();) {
            if (s[i] == '&') {
                const std::size_t sc = s.find(';', i + 1);
                if (sc != std::string::npos && sc - i <= 12) {
                    const std::string e = s.substr(i + 1, sc - i - 1);
                    if (e == "lt") {
                        out += '<';
                    } else if (e == "gt") {
                        out += '>';
                    } else if (e == "amp") {
                        out += '&';
                    } else if (e == "quot") {
                        out += '"';
                    } else if (e == "apos") {
                        out += '\'';
                    } else if (!e.empty() && e[0] == '#') {
                        uint32_t cp = 0;
                        bool ok = e.size() > 1;
                        if (e.size() > 2 && (e[1] == 'x' || e[1] == 'X')) {
                            for (std::size_t k = 2; k < e.size(); ++k) {
                                const char c = e[k];
                                cp <<= 4;
                                if (c >= '0' && c <= '9') {
                                    cp |= static_cast<uint32_t>(c - '0');
                                } else if (c >= 'a' && c <= 'f') {
                                    cp |= static_cast<uint32_t>(c - 'a' + 10);
                                } else if (c >= 'A' && c <= 'F') {
                                    cp |= static_cast<uint32_t>(c - 'A' + 10);
                                } else {
                                    ok = false;
                                    break;
                                }
                            }
                        } else {
                            for (std::size_t k = 1; k < e.size(); ++k) {
                                const char c = e[k];
                                if (c < '0' || c > '9') {
                                    ok = false;
                                    break;
                                }
                                cp = cp * 10 + static_cast<uint32_t>(c - '0');
                            }
                        }
                        if (ok) {
                            appendUtf8(out, cp);
                        } else {
                            out += '&';
                            out += e;
                            out += ';';
                        }
                    } else {
                        out += '&';
                        out += e;
                        out += ';';
                    }
                    i = sc + 1;
                    continue;
                }
            }
            out += s[i];
            ++i;
        }
        return out;
    }

    bool readText() {
        const std::size_t start = m_pos;
        while (m_pos < m_buf.size() && m_buf[m_pos] != '<') {
            ++m_pos;
        }
        m_type = NodeType::Text;
        m_name = decodeEntities(m_buf.substr(start, m_pos - start));
        return true;
    }

    // Consume `<…>` starting at m_pos (which points at '<').
    bool readTag() {
        // Comment / CDATA / declaration blocks.
        if (m_buf.compare(m_pos, 4, "<!--") == 0) {
            const std::size_t end = m_buf.find("-->", m_pos + 4);
            if (end == std::string::npos) {
                fail("unterminated comment");
                return false;
            }
            m_type = NodeType::Comment;
            m_name = m_buf.substr(m_pos + 4, end - (m_pos + 4));
            m_pos = end + 3;
            return true;
        }
        if (m_buf.compare(m_pos, 9, "<![CDATA[") == 0) {
            const std::size_t end = m_buf.find("]]>", m_pos + 9);
            if (end == std::string::npos) {
                fail("unterminated CDATA");
                return false;
            }
            m_type = NodeType::CData;
            m_name = m_buf.substr(m_pos + 9, end - (m_pos + 9)); // CDATA is verbatim, no entity decode
            m_pos = end + 3;
            return true;
        }
        if (m_pos + 1 < m_buf.size() && (m_buf[m_pos + 1] == '?' || m_buf[m_pos + 1] == '!')) {
            // Processing instruction / declaration / DOCTYPE — read to the closing '>'.
            const std::size_t end = m_buf.find('>', m_pos);
            if (end == std::string::npos) {
                fail("unterminated declaration");
                return false;
            }
            m_type = NodeType::Declaration;
            std::size_t contentLen = end - (m_pos + 2);
            if (contentLen > 0 && m_buf[end - 1] == '?') {
                --contentLen; // drop the closing '?' of a <? … ?> processing instruction
            }
            m_name = m_buf.substr(m_pos + 2, contentLen);
            m_pos = end + 1;
            return true;
        }

        // End tag: </name>
        if (m_pos + 1 < m_buf.size() && m_buf[m_pos + 1] == '/') {
            std::size_t p = m_pos + 2;
            const std::size_t nameStart = p;
            while (p < m_buf.size() && m_buf[p] != '>' && !isSpace(m_buf[p])) {
                ++p;
            }
            m_name = m_buf.substr(nameStart, p - nameStart);
            const std::size_t end = m_buf.find('>', p);
            if (end == std::string::npos) {
                fail("unterminated end tag");
                return false;
            }
            m_pos = end + 1;
            m_type = NodeType::ElementEnd;
            if (m_depth > 0) {
                --m_depth;
            }
            m_reportDepth = m_depth; // an end tag reports the level of its matching start tag
            return true;
        }

        // Start element: <name attr="v" …> or <name …/>
        std::size_t p = m_pos + 1;
        const std::size_t nameStart = p;
        while (p < m_buf.size() && m_buf[p] != '>' && m_buf[p] != '/' && !isSpace(m_buf[p])) {
            ++p;
        }
        if (p == nameStart) {
            fail("empty element name");
            return false;
        }
        m_name = m_buf.substr(nameStart, p - nameStart);
        m_type = NodeType::Element;

        // Attributes.
        for (;;) {
            while (p < m_buf.size() && isSpace(m_buf[p])) {
                ++p;
            }
            if (p >= m_buf.size()) {
                fail("unterminated start tag");
                return false;
            }
            if (m_buf[p] == '/') {
                m_empty = true;
                ++p;
                if (p >= m_buf.size() || m_buf[p] != '>') {
                    fail("malformed self-closing tag");
                    return false;
                }
                ++p;
                break;
            }
            if (m_buf[p] == '>') {
                ++p;
                break;
            }
            // attribute name
            const std::size_t aStart = p;
            while (p < m_buf.size() && m_buf[p] != '=' && m_buf[p] != '>' && m_buf[p] != '/' &&
                   !isSpace(m_buf[p])) {
                ++p;
            }
            const std::string aName = m_buf.substr(aStart, p - aStart);
            while (p < m_buf.size() && isSpace(m_buf[p])) {
                ++p;
            }
            std::string aValue;
            if (p < m_buf.size() && m_buf[p] == '=') {
                ++p;
                while (p < m_buf.size() && isSpace(m_buf[p])) {
                    ++p;
                }
                if (p >= m_buf.size() || (m_buf[p] != '"' && m_buf[p] != '\'')) {
                    fail("attribute value not quoted");
                    return false;
                }
                const char quote = m_buf[p];
                ++p;
                const std::size_t vStart = p;
                while (p < m_buf.size() && m_buf[p] != quote) {
                    ++p;
                }
                if (p >= m_buf.size()) {
                    fail("unterminated attribute value");
                    return false;
                }
                aValue = decodeEntities(m_buf.substr(vStart, p - vStart));
                ++p; // closing quote
            }
            m_attrs.emplace_back(aName, aValue);
        }

        m_pos = p;
        m_reportDepth = m_depth; // the element itself sits at the current depth …
        if (!m_empty) {
            ++m_depth; // … and its children are one level deeper
        }
        return true;
    }

    std::string m_buf;
    std::size_t m_pos = 0;
    NodeType m_type = NodeType::None;
    std::string m_name;
    std::vector<std::pair<std::string, std::string>> m_attrs;
    bool m_empty = false;
    int m_depth = 0;
    int m_reportDepth = 0;
    bool m_error = false;
    std::string m_errorText;
};

} // namespace maz::io
