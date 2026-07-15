#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "maz/scene/SceneTree.hpp"

// maz::scene text (de)serialization — Maz's answer to Godot's `.tscn` scene files. A whole node
// tree (structure + per-node transform, visibility, groups, and script class reference) round-trips
// to a small, human-readable, line-based text format. Scripts live in the script program (load them
// with SceneTree::loadScripts before loadTree); the scene only references classes by name.
//
//   std::string text = scene::saveTree(tree);
//   SceneTree other;
//   other.loadScripts(programSource);
//   scene::loadTree(other, text);   // rebuilds the identical hierarchy, re-attaches scripts
namespace maz::scene {

namespace detail {

// Format a number compactly: integers without a decimal point, otherwise %g.
inline std::string num(double v) {
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
        char b[32];
        std::snprintf(b, sizeof(b), "%lld", static_cast<long long>(v));
        return b;
    }
    char b[32];
    std::snprintf(b, sizeof(b), "%g", v);
    return b;
}

// The path of a node relative to the root ("" for the root, "Player/Weapon" for a grandchild).
inline std::string pathOf(const SceneNode& node) {
    std::vector<std::string> parts;
    for (const SceneNode* c = &node; c && c->parent(); c = c->parent()) {
        parts.push_back(c->name());
    }
    std::string p;
    for (size_t i = parts.size(); i-- > 0;) {
        if (!p.empty())
            p += "/";
        p += parts[i];
    }
    return p;
}

// Extract a field value from a serialized line. Every key is space-prefixed (" key=") so "x" won't
// match inside "sx". Quoted values return the text between quotes; bare values run to the next
// space.
inline std::string field(const std::string& line, const std::string& key) {
    const std::string k = " " + key + "=";
    size_t p = line.find(k);
    if (p == std::string::npos)
        return "";
    p += k.size();
    if (p < line.size() && line[p] == '"') {
        const size_t end = line.find('"', p + 1);
        return line.substr(p + 1, end == std::string::npos ? std::string::npos : end - (p + 1));
    }
    const size_t end = line.find(' ', p);
    return line.substr(p, end == std::string::npos ? std::string::npos : end - p);
}

inline std::string writeNode(const SceneNode& node) {
    const script::Node2D& t = node.local();
    std::string groups;
    for (const auto& g : node.groups()) {
        if (!groups.empty())
            groups += ";";
        groups += g;
    }
    std::string out = "node path=\"" + pathOf(node) + "\"";
    out += " name=\"" + node.name() + "\"";
    out += " x=" + num(t.x) + " y=" + num(t.y) + " rot=" + num(t.rotation);
    out += " sx=" + num(t.scaleX) + " sy=" + num(t.scaleY);
    out += " vis=" + std::string(t.visible ? "1" : "0");
    out += " groups=\"" + groups + "\"";
    out += " script=\"" + node.scriptClass() + "\"";
    return out;
}

} // namespace detail

// Serialize a whole tree to text (depth-first, parents before children).
inline std::string saveTree(SceneTree& tree) {
    std::string out = "maz_scene v1\n";
    std::function<void(const SceneNode&)> walk = [&](const SceneNode& n) {
        out += detail::writeNode(n);
        out += "\n";
        for (const auto& c : n.children()) {
            walk(*c);
        }
    };
    walk(tree.root());
    return out;
}

// Rebuild a tree from text into `tree` (freshly constructed, scripts preloaded). Returns false if
// the header is missing. Unknown script classes are skipped (the node is still created).
inline bool loadTree(SceneTree& tree, const std::string& text) {
    size_t pos = 0;
    bool sawHeader = false;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        if (line.empty())
            continue;
        if (!sawHeader) {
            if (line.rfind("maz_scene", 0) != 0)
                return false;
            sawHeader = true;
            continue;
        }
        if (line.rfind("node ", 0) != 0)
            continue;

        const std::string path = detail::field(line, "path");
        const std::string name = detail::field(line, "name");
        SceneNode* node = nullptr;
        if (path.empty()) {
            node = &tree.root(); // the root line sets the root's own transform
            if (!name.empty())
                node->setName(name);
        } else {
            const size_t slash = path.rfind('/');
            const std::string parentPath = slash == std::string::npos ? "" : path.substr(0, slash);
            SceneNode* parent = tree.findNode(parentPath);
            if (!parent)
                continue; // malformed / out-of-order; skip
            const std::string leaf =
                name.empty() ? path.substr(slash == std::string::npos ? 0 : slash + 1) : name;
            node = tree.createChild(*parent, leaf);
        }

        script::Node2D& t = node->local();
        auto toNum = [](const std::string& s, double d) {
            return s.empty() ? d : std::strtod(s.c_str(), nullptr);
        };
        t.x = toNum(detail::field(line, "x"), 0);
        t.y = toNum(detail::field(line, "y"), 0);
        t.rotation = toNum(detail::field(line, "rot"), 0);
        t.scaleX = toNum(detail::field(line, "sx"), 1);
        t.scaleY = toNum(detail::field(line, "sy"), 1);
        t.visible = detail::field(line, "vis") != "0";

        const std::string groups = detail::field(line, "groups");
        size_t gp = 0;
        while (gp < groups.size()) {
            const size_t sep = groups.find(';', gp);
            std::string g =
                groups.substr(gp, sep == std::string::npos ? std::string::npos : sep - gp);
            if (!g.empty())
                node->addToGroup(g);
            if (sep == std::string::npos)
                break;
            gp = sep + 1;
        }

        const std::string script = detail::field(line, "script");
        if (!script.empty()) {
            tree.attachScript(*node, script); // best-effort; unknown class is silently skipped
        }
    }
    return sawHeader;
}

} // namespace maz::scene
