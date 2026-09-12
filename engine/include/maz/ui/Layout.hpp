#pragma once

#include "maz/ui/Rect.hpp"

#include <vector>

namespace maz::ui {

// Retained UI layout — anchors + containers, modeled on Godot's Control system. Maz already had
// immediate-mode widgets, but every position was a hand-typed pixel coordinate that broke at a
// different resolution. This adds the missing piece: a layout tree that computes screen rects
// responsively.
//
// Two ways a node is placed:
//   * Anchors + offsets (Godot's model): anchorMin/anchorMax are fractions [0..1] of the PARENT rect
//     for each edge; L/T/R/B offsets are pixel margins from those anchored points. So (0,0,1,1) with
//     zero offsets fills the parent; (0,0,1,0)+offsets makes a top bar of fixed height that stretches
//     to any width; (0.5,0.5,0.5,0.5) pins a fixed-size box to the center.
//   * Container modes (HBox / VBox / Center) that arrange children automatically: fixed-size children
//     keep their min size, `expand` children share the leftover space, `spacing` sits between them and
//     `pad` insets the container. This is how you build responsive toolbars, lists, and dialogs.
//
// layout() walks the tree from a root rect (usually the framebuffer) and fills every node's `rect`.
// Non-owning children pointers — the app owns the nodes. Header-only, math-only (no renderer dep).

class LayoutNode {
public:
    enum class Mode { Anchor, HBox, VBox, Center };

    Mode mode = Mode::Anchor;

    // Anchor placement (used when the PARENT is in Anchor mode).
    float ax0 = 0.0f, ay0 = 0.0f, ax1 = 0.0f, ay1 = 0.0f; // anchors (fractions of parent)
    float ol = 0.0f, ot = 0.0f, orr = 0.0f, ob = 0.0f;    // offsets (pixels)

    // Box / center placement (used when the PARENT is a container).
    float minW = 0.0f, minH = 0.0f; // desired size along the relevant axis
    bool expand = false;            // grab leftover space in a box
    float spacing = 0.0f;           // gap between children (container)
    float pad = 0.0f;               // inner padding (container)

    Rect rect;                              // computed output
    std::vector<LayoutNode*> children;      // non-owning

    LayoutNode() = default;
    LayoutNode(Mode m) : mode(m) {}

    LayoutNode& add(LayoutNode* child) {
        children.push_back(child);
        return *this;
    }

    // --- anchor helpers ---------------------------------------------------------------------------
    void setAnchors(float x0, float y0, float x1, float y1) {
        ax0 = x0;
        ay0 = y0;
        ax1 = x1;
        ay1 = y1;
    }
    void setOffsets(float l, float t, float r, float b) {
        ol = l;
        ot = t;
        orr = r;
        ob = b;
    }
    // Fill the parent (optionally inset by a uniform margin).
    void fill(float margin = 0.0f) {
        setAnchors(0, 0, 1, 1);
        setOffsets(margin, margin, -margin, -margin);
    }
    // A top bar of fixed height spanning the parent's width.
    void anchorTop(float height, float margin = 0.0f) {
        setAnchors(0, 0, 1, 0);
        setOffsets(margin, margin, -margin, margin + height);
    }
    // A left sidebar of fixed width spanning the parent's height.
    void anchorLeft(float width, float margin = 0.0f) {
        setAnchors(0, 0, 0, 1);
        setOffsets(margin, margin, margin + width, -margin);
    }

    // Compute this node's `rect` from a parent rect under the anchor model.
    Rect anchoredIn(const Rect& p) const {
        const float l = p.x + p.w * ax0 + ol;
        const float t = p.y + p.h * ay0 + ot;
        const float r = p.x + p.w * ax1 + orr;
        const float b = p.y + p.h * ay1 + ob;
        return Rect{l, t, r - l, b - t};
    }

    // Set the root rect and arrange the whole subtree.
    void layout(const Rect& rootRect) {
        rect = rootRect;
        arrange();
    }

private:
    // Position children according to this node's mode, then recurse.
    void arrange() {
        const Rect inner{rect.x + pad, rect.y + pad, rect.w - 2.0f * pad, rect.h - 2.0f * pad};
        switch (mode) {
        case Mode::Anchor:
            for (LayoutNode* c : children) c->rect = c->anchoredIn(inner);
            break;
        case Mode::Center:
            for (LayoutNode* c : children) {
                c->rect = Rect{inner.x + (inner.w - c->minW) * 0.5f,
                               inner.y + (inner.h - c->minH) * 0.5f, c->minW, c->minH};
            }
            break;
        case Mode::HBox: {
            const int n = static_cast<int>(children.size());
            if (n > 0) {
                const float totalSpacing = spacing * static_cast<float>(n - 1);
                float fixed = 0.0f;
                int expanders = 0;
                for (LayoutNode* c : children) {
                    if (c->expand)
                        ++expanders;
                    else
                        fixed += c->minW;
                }
                const float leftover = inner.w - totalSpacing - fixed;
                const float per = expanders > 0 ? (leftover > 0.0f ? leftover : 0.0f) /
                                                      static_cast<float>(expanders)
                                                : 0.0f;
                float x = inner.x;
                for (LayoutNode* c : children) {
                    const float w = c->expand ? per : c->minW;
                    c->rect = Rect{x, inner.y, w, inner.h};
                    x += w + spacing;
                }
            }
            break;
        }
        case Mode::VBox: {
            const int n = static_cast<int>(children.size());
            if (n > 0) {
                const float totalSpacing = spacing * static_cast<float>(n - 1);
                float fixed = 0.0f;
                int expanders = 0;
                for (LayoutNode* c : children) {
                    if (c->expand)
                        ++expanders;
                    else
                        fixed += c->minH;
                }
                const float leftover = inner.h - totalSpacing - fixed;
                const float per = expanders > 0 ? (leftover > 0.0f ? leftover : 0.0f) /
                                                      static_cast<float>(expanders)
                                                : 0.0f;
                float y = inner.y;
                for (LayoutNode* c : children) {
                    const float h = c->expand ? per : c->minH;
                    c->rect = Rect{inner.x, y, inner.w, h};
                    y += h + spacing;
                }
            }
            break;
        }
        }
        for (LayoutNode* c : children) c->arrange();
    }
};

} // namespace maz::ui
