#pragma once

#include "maz/render/Renderer.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace maz::ui {

// Tree / TreeItem — Godot's hierarchical list control, the backbone of its scene dock, inspector, and
// FileSystem dock. A TreeItem carries text, an optional id + colour, a `collapsed` flag, and child items;
// the Tree FLATTENS the currently-expanded items into an ordered list of visible ROWS, each tagged with
// its depth (for indentation) and whether it has children (so the view draws a fold arrow). Folding a
// branch hides its whole subtree in one flag. It's a pure data structure + depth-first traversal —
// deterministic and GPU-free — so it unit-tests headless and a renderer just draws the rows it returns.

struct TreeItem {
    std::string text;
    int id = -1;                                    // caller payload
    bool collapsed = false;                         // subtree hidden when true
    render::Color color{0.85f, 0.88f, 0.95f, 1.0f}; // row tint
    std::vector<std::unique_ptr<TreeItem>> children;

    // Append a child and return a STABLE reference to it (children are heap-owned, so the reference and
    // any TreeItem* survive later sibling insertions — safe to keep while building the tree).
    TreeItem& addChild(std::string t, int childId = -1) {
        auto c = std::make_unique<TreeItem>();
        c->text = std::move(t);
        c->id = childId;
        children.push_back(std::move(c));
        return *children.back();
    }

    bool hasChildren() const { return !children.empty(); }
};

// One flattened visible row: the item, its indentation depth, whether it has children, and its fold state.
struct TreeRow {
    const TreeItem* item = nullptr;
    int depth = 0;
    bool hasChildren = false;
    bool collapsed = false;
};

class Tree {
public:
    TreeItem root; // invisible root; its children are the top-level rows
    int selected = -1; // selected index into visibleRows(), or -1

    // Add a top-level item.
    TreeItem& add(std::string t, int id = -1) { return root.addChild(std::move(t), id); }

    // Depth-first list of visible rows (root excluded; a collapsed item's subtree is skipped).
    std::vector<TreeRow> visibleRows() const {
        std::vector<TreeRow> rows;
        for (const auto& c : root.children) {
            collect(*c, 0, rows);
        }
        return rows;
    }

    std::size_t visibleCount() const {
        std::size_t n = 0;
        for (const auto& c : root.children) {
            n += countVisible(*c);
        }
        return n;
    }

private:
    static void collect(const TreeItem& it, int depth, std::vector<TreeRow>& rows) {
        rows.push_back(TreeRow{&it, depth, it.hasChildren(), it.collapsed});
        if (!it.collapsed) {
            for (const auto& c : it.children) {
                collect(*c, depth + 1, rows);
            }
        }
    }

    static std::size_t countVisible(const TreeItem& it) {
        std::size_t n = 1;
        if (!it.collapsed) {
            for (const auto& c : it.children) {
                n += countVisible(*c);
            }
        }
        return n;
    }
};

} // namespace maz::ui
