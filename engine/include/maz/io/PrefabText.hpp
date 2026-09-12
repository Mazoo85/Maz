#pragma once

#include "maz/scene/Prefab.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace maz::io {

// Text resource save/load for prefabs — Godot's .tscn / .tres text format. M125 gave prefabs an in-memory
// template + instancing; this makes them a DISK RESOURCE you can read, diff, and version-control as plain
// text (the whole reason Godot's scene files are text). A prefab tree serializes to a sequence of
// `[node name="…" parent="…"]` sections, each followed by typed `key = TYPE values` property lines, and
// parses straight back into an identical tree. Deterministic + round-trip-stable; pure string work, no I/O
// device, so it unit-tests headlessly (the app owns any actual file read/write).

namespace detail {

inline std::string fmtF(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    return buf;
}

// Serialize one property value to "TYPE v0 v1 …".
inline std::string propToText(const scene::PropValue& p) {
    using T = scene::PropValue::Type;
    switch (p.type) {
    case T::Float:
        return "float " + fmtF(p.f);
    case T::Int:
        return "int " + std::to_string(p.i);
    case T::Bool:
        return std::string("bool ") + (p.b ? "true" : "false");
    case T::Vec2:
        return "vec2 " + fmtF(p.v2.x) + " " + fmtF(p.v2.y);
    case T::Color:
        return "color " + fmtF(p.color.x) + " " + fmtF(p.color.y) + " " + fmtF(p.color.z) + " " +
               fmtF(p.color.w);
    case T::Text:
        return "text \"" + p.text + "\"";
    }
    return "float 0";
}

// Split `s` into whitespace-separated tokens.
inline std::vector<std::string> tokens(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        std::size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t') {
            ++j;
        }
        if (j > i) {
            out.push_back(s.substr(i, j - i));
        }
        i = j;
    }
    return out;
}

// Parse a "TYPE v0 v1 …" value line back into a PropValue. Returns false on an unknown/short form.
inline bool textToProp(const std::string& s, scene::PropValue& out) {
    const std::size_t sp = s.find(' ');
    if (sp == std::string::npos) {
        return false;
    }
    const std::string type = s.substr(0, sp);
    const std::string rest = s.substr(sp + 1);
    if (type == "float") {
        out = scene::PropValue::makeFloat(std::strtof(rest.c_str(), nullptr));
    } else if (type == "int") {
        out = scene::PropValue::makeInt(static_cast<int>(std::strtol(rest.c_str(), nullptr, 10)));
    } else if (type == "bool") {
        out = scene::PropValue::makeBool(rest == "true");
    } else if (type == "vec2") {
        const std::vector<std::string> t = tokens(rest);
        if (t.size() < 2) {
            return false;
        }
        out = scene::PropValue::makeVec2(
            math::vec2(std::strtof(t[0].c_str(), nullptr), std::strtof(t[1].c_str(), nullptr)));
    } else if (type == "color") {
        const std::vector<std::string> t = tokens(rest);
        if (t.size() < 4) {
            return false;
        }
        out = scene::PropValue::makeColor(math::vec4(std::strtof(t[0].c_str(), nullptr),
                                                     std::strtof(t[1].c_str(), nullptr),
                                                     std::strtof(t[2].c_str(), nullptr),
                                                     std::strtof(t[3].c_str(), nullptr)));
    } else if (type == "text") {
        std::string v = rest;
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
            v = v.substr(1, v.size() - 2);
        }
        out = scene::PropValue::makeText(v);
    } else {
        return false;
    }
    return true;
}

inline void writeNode(std::string& out, const scene::PrefabNode& node, const std::string& parentPath) {
    out += "[node name=\"" + node.name + "\"";
    if (!parentPath.empty()) {
        out += " parent=\"" + parentPath + "\"";
    }
    out += "]\n";
    for (const auto& kv : node.props) {
        out += kv.first + " = " + propToText(kv.second) + "\n";
    }
    out += "\n";
    // Path of THIS node as seen by its children: "." for the root, else the path below the root.
    const std::string thisPath =
        parentPath.empty() ? "." : (parentPath == "." ? node.name : parentPath + "/" + node.name);
    for (const scene::PrefabNode& c : node.children) {
        writeNode(out, c, thisPath);
    }
}

// Extract the value of attribute `key="…"` from a `[node …]` header line.
inline std::string attr(const std::string& line, const std::string& key) {
    const std::string needle = key + "=\"";
    const std::size_t k = line.find(needle);
    if (k == std::string::npos) {
        return "";
    }
    const std::size_t start = k + needle.size();
    const std::size_t end = line.find('"', start);
    if (end == std::string::npos) {
        return "";
    }
    return line.substr(start, end - start);
}

} // namespace detail

// Serialize a prefab to Godot-.tscn-style text.
inline std::string savePrefabText(const scene::Prefab& prefab) {
    std::string out;
    detail::writeNode(out, prefab.root, "");
    return out;
}

// Parse .tscn-style text back into a prefab. Returns false if no root node was found.
inline bool loadPrefabText(const std::string& text, scene::Prefab& out) {
    out = scene::Prefab{};
    bool haveRoot = false;
    scene::PrefabNode* cur = nullptr;

    std::size_t i = 0;
    while (i <= text.size()) {
        const std::size_t nl = text.find('\n', i);
        const std::string line = text.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
        i = (nl == std::string::npos) ? text.size() + 1 : nl + 1;

        if (line.empty()) {
            continue;
        }
        if (line[0] == '[') {
            const std::string name = detail::attr(line, "name");
            const std::string parent = detail::attr(line, "parent");
            if (parent.empty()) {
                out.root.name = name;
                out.root.children.clear();
                out.root.props.clear();
                cur = &out.root;
                haveRoot = true;
            } else {
                scene::PrefabNode* p = (parent == ".") ? &out.root : scene::findNode(out.root, parent);
                if (!p) {
                    cur = nullptr;
                    continue;
                }
                scene::PrefabNode child;
                child.name = name;
                p->children.push_back(child);
                cur = &p->children.back();
            }
            continue;
        }
        // Property line: "key = TYPE values".
        const std::size_t eq = line.find(" = ");
        if (eq != std::string::npos && cur) {
            const std::string key = line.substr(0, eq);
            scene::PropValue val;
            if (detail::textToProp(line.substr(eq + 3), val)) {
                scene::setProp(cur->props, key, val);
            }
        }
    }
    return haveRoot;
}

} // namespace maz::io
