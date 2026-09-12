#pragma once

#include <cstdint>
#include <functional>
#include <string>

// maz::ui drag-and-drop — the coordinator behind Godot's Control drag/drop (get_drag_data /
// can_drop_data / drop_data). A drag begins at a source control carrying a typed payload; while it is
// in flight, potential targets are asked whether they accept it (a predicate, like can_drop_data);
// releasing over an accepting target delivers the payload (drop_data) and ends the drag, otherwise the
// drag continues or is cancelled. This models that state machine + payload; the widget layer supplies
// the hit-testing and the drag preview. Pure, header-only, deterministic — unit-tests exactly.
namespace maz::ui {

// The dragged value. Godot passes a Variant; this carries a type tag plus int/string/pointer slots,
// which covers the common cases (an item id, a resource path, a node pointer).
struct DragPayload {
    std::string type;      // caller-defined kind, e.g. "files", "tree_item", "color"
    int64_t id = 0;        // e.g. item index / node id
    std::string text;      // e.g. a path or label
    void* pointer = nullptr;
};

class DragAndDrop {
  public:
    using CanDrop = std::function<bool(const DragPayload&)>;
    using OnDrop = std::function<void(const DragPayload&)>;

    // Begin a drag from `source`. Fails if a drag is already in flight.
    bool beginDrag(int source, const DragPayload& payload) {
        if (m_dragging) {
            return false;
        }
        m_dragging = true;
        m_source = source;
        m_payload = payload;
        m_hover = -1;
        m_accepts = false;
        return true;
    }

    bool dragging() const { return m_dragging; }
    int source() const { return m_source; }
    const DragPayload& payload() const { return m_payload; }

    // Update which target the cursor is over and whether it would accept a drop (Godot polls
    // can_drop_data during the drag to drive the cursor/preview feedback). No-op if not dragging.
    void hover(int target, const CanDrop& canDrop) {
        if (!m_dragging) {
            return;
        }
        m_hover = target;
        m_accepts = canDrop ? canDrop(m_payload) : false;
    }
    int hoveredTarget() const { return m_hover; }
    bool hoverAccepts() const { return m_accepts; }

    // Release over `target`: if it accepts the payload, deliver it (onDrop) and end the drag, returning
    // true. Otherwise the drag stays in flight and this returns false.
    bool drop(int target, const CanDrop& canDrop, const OnDrop& onDrop) {
        if (!m_dragging) {
            return false;
        }
        if (canDrop && !canDrop(m_payload)) {
            return false;
        }
        if (onDrop) {
            onDrop(m_payload);
        }
        (void)target;
        end();
        return true;
    }

    // Abort the drag without delivering (e.g. Escape / released over nothing).
    void cancel() { end(); }

  private:
    void end() {
        m_dragging = false;
        m_source = -1;
        m_hover = -1;
        m_accepts = false;
        m_payload = DragPayload{};
    }

    bool m_dragging = false;
    int m_source = -1;
    int m_hover = -1;
    bool m_accepts = false;
    DragPayload m_payload;
};

} // namespace maz::ui
