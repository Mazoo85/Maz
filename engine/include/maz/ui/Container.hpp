#pragma once

#include "maz/ui/Rect.hpp"

#include <cstddef>
#include <vector>

namespace maz::ui {

// Auto-layout containers — Godot's Container controls (BoxContainer / GridContainer / MarginContainer /
// CenterContainer). M86's LayoutNode already gives an anchor tree with a *simple* box mode where every
// `expand` child grabs an equal slice of the leftover space and the cross axis always fills. Godot's real
// container model is richer, and that richness is what you actually need to build a resizable UI:
//
//   * per-axis SIZE FLAGS — a child independently chooses, for its horizontal and its vertical axis,
//     whether to Fill the cell, Expand (grab leftover main-axis space), or Shrink to its minimum and sit
//     at the Begin / Center / End of the cell;
//   * STRETCH RATIOS — two expanding children with ratios 1 and 3 split the leftover 1:3, not 50/50;
//   * a real GRID — N columns, column widths driven by the widest cell in each column, expanding columns
//     sharing the leftover, so a form of label/field pairs lines up;
//   * BOTTOM-UP minimum size — a container reports the min size it needs from its children, so nested
//     containers (a VBox of HBoxes) size correctly.
//
// All pure rectangle math (no renderer, no Font) operating on ui::Rect, so it unit-tests headlessly and
// the results are deterministic. The app owns the Control structs; each layout call writes their `rect`.

// How a control uses the space its container assigns it, on one axis.
enum class SizeFlag {
    Fill,         // occupy the whole assigned extent on this axis (default)
    Expand,       // Fill, and additionally grab a share of the container's leftover MAIN-axis space
    ShrinkBegin,  // keep minimum size, sit at the start of the assigned extent
    ShrinkCenter, // keep minimum size, centered in the assigned extent
    ShrinkEnd,    // keep minimum size, at the end of the assigned extent
};

struct Control {
    float minW = 0.0f, minH = 0.0f;         // minimum size this control needs
    SizeFlag hFlag = SizeFlag::Fill;        // horizontal size flag
    SizeFlag vFlag = SizeFlag::Fill;        // vertical size flag
    float stretch = 1.0f;                   // stretch ratio when this axis's flag is Expand
    Rect rect;                              // computed output rectangle
};

namespace detail {

struct Span {
    float pos = 0.0f;
    float size = 0.0f;
};

// Position one control within a cross-axis extent [origin, origin+extent] given its min size + flag.
// Fill/Expand stretch to the whole extent; the Shrink flags keep the min size and anchor it.
inline Span placeCross(float origin, float extent, float minSize, SizeFlag flag) {
    switch (flag) {
    case SizeFlag::Fill:
    case SizeFlag::Expand:
        return {origin, extent};
    case SizeFlag::ShrinkBegin:
        return {origin, minSize};
    case SizeFlag::ShrinkCenter:
        return {origin + (extent - minSize) * 0.5f, minSize};
    case SizeFlag::ShrinkEnd:
        return {origin + (extent - minSize), minSize};
    }
    return {origin, extent};
}

// Distribute controls along a MAIN axis of length `extent` starting at `origin`, `sep` between each.
// Only Expand children grow beyond their min; leftover is split by stretch ratio. If nothing expands,
// content packs at the start and any surplus stays empty at the end (Godot's Begin alignment).
inline std::vector<Span> distributeMain(const std::vector<float>& mins,
                                        const std::vector<SizeFlag>& flags,
                                        const std::vector<float>& stretches, float origin, float extent,
                                        float sep) {
    const std::size_t n = mins.size();
    std::vector<Span> out(n);
    if (n == 0) {
        return out;
    }
    float totalMin = sep * static_cast<float>(n - 1);
    for (float m : mins) {
        totalMin += m;
    }
    float leftover = extent - totalMin;
    if (leftover < 0.0f) {
        leftover = 0.0f;
    }
    float stretchSum = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        if (flags[i] == SizeFlag::Expand) {
            stretchSum += stretches[i];
        }
    }
    float cursor = origin;
    for (std::size_t i = 0; i < n; ++i) {
        float size = mins[i];
        if (flags[i] == SizeFlag::Expand && stretchSum > 0.0f) {
            size += leftover * (stretches[i] / stretchSum);
        }
        out[i] = {cursor, size};
        cursor += size + sep;
    }
    return out;
}

} // namespace detail

// --- Box containers -------------------------------------------------------------------------------

// Lay children left-to-right in `area`. Main axis = x (Expand grows width, split by stretch); each
// child's height comes from its vertical flag (Fill → area height, Shrink → minH anchored).
inline void hbox(const Rect& area, const std::vector<Control*>& kids, float sep = 0.0f) {
    std::vector<float> mins, stretches;
    std::vector<SizeFlag> flags;
    mins.reserve(kids.size());
    stretches.reserve(kids.size());
    flags.reserve(kids.size());
    for (const Control* c : kids) {
        mins.push_back(c->minW);
        flags.push_back(c->hFlag);
        stretches.push_back(c->stretch);
    }
    const std::vector<detail::Span> spans = detail::distributeMain(mins, flags, stretches, area.x, area.w, sep);
    for (std::size_t i = 0; i < kids.size(); ++i) {
        const detail::Span cross = detail::placeCross(area.y, area.h, kids[i]->minH, kids[i]->vFlag);
        kids[i]->rect = {spans[i].pos, cross.pos, spans[i].size, cross.size};
    }
}

// Lay children top-to-bottom in `area`. Main axis = y; each child's width comes from its horizontal flag.
inline void vbox(const Rect& area, const std::vector<Control*>& kids, float sep = 0.0f) {
    std::vector<float> mins, stretches;
    std::vector<SizeFlag> flags;
    mins.reserve(kids.size());
    stretches.reserve(kids.size());
    flags.reserve(kids.size());
    for (const Control* c : kids) {
        mins.push_back(c->minH);
        flags.push_back(c->vFlag);
        stretches.push_back(c->stretch);
    }
    const std::vector<detail::Span> spans = detail::distributeMain(mins, flags, stretches, area.y, area.h, sep);
    for (std::size_t i = 0; i < kids.size(); ++i) {
        const detail::Span cross = detail::placeCross(area.x, area.w, kids[i]->minW, kids[i]->hFlag);
        kids[i]->rect = {cross.pos, spans[i].pos, cross.size, spans[i].size};
    }
}

// --- Grid container -------------------------------------------------------------------------------

// Lay children into `columns` columns, row by row (child i → column i%columns, row i/columns). Each
// column is as wide as its widest child's minW, each row as tall as its tallest child's minH; columns
// containing an h-Expand child (and rows with a v-Expand child) share the leftover space equally. Within
// its cell each child is placed by its per-axis size flag (Fill fills the cell, Shrink anchors the min).
inline void grid(const Rect& area, const std::vector<Control*>& kids, int columns, float hsep = 0.0f,
                 float vsep = 0.0f) {
    if (columns < 1) {
        columns = 1;
    }
    const std::size_t n = kids.size();
    if (n == 0) {
        return;
    }
    const std::size_t cols = static_cast<std::size_t>(columns);
    const std::size_t rows = (n + cols - 1) / cols;

    std::vector<float> colW(cols, 0.0f), rowH(rows, 0.0f);
    std::vector<bool> colExpand(cols, false), rowExpand(rows, false);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t c = i % cols;
        const std::size_t r = i / cols;
        if (kids[i]->minW > colW[c]) {
            colW[c] = kids[i]->minW;
        }
        if (kids[i]->minH > rowH[r]) {
            rowH[r] = kids[i]->minH;
        }
        if (kids[i]->hFlag == SizeFlag::Expand) {
            colExpand[c] = true;
        }
        if (kids[i]->vFlag == SizeFlag::Expand) {
            rowExpand[r] = true;
        }
    }

    float sumW = hsep * static_cast<float>(cols - 1);
    for (float w : colW) {
        sumW += w;
    }
    float sumH = vsep * static_cast<float>(rows - 1);
    for (float h : rowH) {
        sumH += h;
    }
    int nExpandCols = 0, nExpandRows = 0;
    for (bool e : colExpand) {
        nExpandCols += e ? 1 : 0;
    }
    for (bool e : rowExpand) {
        nExpandRows += e ? 1 : 0;
    }
    float extraW = area.w - sumW;
    float extraH = area.h - sumH;
    if (extraW > 0.0f && nExpandCols > 0) {
        const float share = extraW / static_cast<float>(nExpandCols);
        for (std::size_t c = 0; c < cols; ++c) {
            if (colExpand[c]) {
                colW[c] += share;
            }
        }
    }
    if (extraH > 0.0f && nExpandRows > 0) {
        const float share = extraH / static_cast<float>(nExpandRows);
        for (std::size_t r = 0; r < rows; ++r) {
            if (rowExpand[r]) {
                rowH[r] += share;
            }
        }
    }

    std::vector<float> colX(cols, 0.0f), rowY(rows, 0.0f);
    float cx = area.x;
    for (std::size_t c = 0; c < cols; ++c) {
        colX[c] = cx;
        cx += colW[c] + hsep;
    }
    float cy = area.y;
    for (std::size_t r = 0; r < rows; ++r) {
        rowY[r] = cy;
        cy += rowH[r] + vsep;
    }

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t c = i % cols;
        const std::size_t r = i / cols;
        const detail::Span h = detail::placeCross(colX[c], colW[c], kids[i]->minW, kids[i]->hFlag);
        const detail::Span v = detail::placeCross(rowY[r], rowH[r], kids[i]->minH, kids[i]->vFlag);
        kids[i]->rect = {h.pos, v.pos, h.size, v.size};
    }
}

// --- Single-child containers ----------------------------------------------------------------------

// Inset `area` by per-side margins; the child fills the remaining rectangle (Godot MarginContainer).
inline void margin(const Rect& area, Control& child, float left, float top, float right, float bottom) {
    child.rect = {area.x + left, area.y + top, area.w - left - right, area.h - top - bottom};
}

// Center the child at its minimum size within `area` (Godot CenterContainer).
inline void center(const Rect& area, Control& child) {
    child.rect = {area.centerX() - child.minW * 0.5f, area.centerY() - child.minH * 0.5f, child.minW,
                  child.minH};
}

// --- Minimum-size helpers (bottom-up) -------------------------------------------------------------

// Combined min size an HBox needs: sum of child widths + separators, tallest child height.
inline void hboxMinSize(const std::vector<Control*>& kids, float sep, float& outW, float& outH) {
    outW = kids.empty() ? 0.0f : sep * static_cast<float>(kids.size() - 1);
    outH = 0.0f;
    for (const Control* c : kids) {
        outW += c->minW;
        if (c->minH > outH) {
            outH = c->minH;
        }
    }
}

// Combined min size a VBox needs: widest child, sum of heights + separators.
inline void vboxMinSize(const std::vector<Control*>& kids, float sep, float& outW, float& outH) {
    outW = 0.0f;
    outH = kids.empty() ? 0.0f : sep * static_cast<float>(kids.size() - 1);
    for (const Control* c : kids) {
        if (c->minW > outW) {
            outW = c->minW;
        }
        outH += c->minH;
    }
}

// Combined min size a Grid needs: sum of per-column widest + hsep, sum of per-row tallest + vsep.
inline void gridMinSize(const std::vector<Control*>& kids, int columns, float hsep, float vsep,
                        float& outW, float& outH) {
    if (columns < 1) {
        columns = 1;
    }
    const std::size_t n = kids.size();
    if (n == 0) {
        outW = 0.0f;
        outH = 0.0f;
        return;
    }
    const std::size_t cols = static_cast<std::size_t>(columns);
    const std::size_t rows = (n + cols - 1) / cols;
    std::vector<float> colW(cols, 0.0f), rowH(rows, 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t c = i % cols;
        const std::size_t r = i / cols;
        if (kids[i]->minW > colW[c]) {
            colW[c] = kids[i]->minW;
        }
        if (kids[i]->minH > rowH[r]) {
            rowH[r] = kids[i]->minH;
        }
    }
    outW = hsep * static_cast<float>(cols - 1);
    for (float w : colW) {
        outW += w;
    }
    outH = vsep * static_cast<float>(rows - 1);
    for (float h : rowH) {
        outH += h;
    }
}

} // namespace maz::ui
