#include <txui/widgets/Widget.hpp>
#include <algorithm>
#include <cassert>

namespace txui {

void Widget::add_child(Ref<Widget> child) noexcept {
    if (!child) return;
    
    // Uniqueness check: cannot add a child that is already a child of this parent
    if (child->m_parent == this) {
        // Technically could be a 'move to back' operation, but Gate 1 strictly fails on duplicate add
        assert(false && "Widget uniqueness violation: Widget is already a child of this parent");
        return;
    }

    // Cycle detection: cannot add an ancestor as a child
    Widget* current = this;
    while (current != nullptr) {
        if (current == child.get()) {
            assert(false && "Widget cycle violation: Cannot add an ancestor as a child");
            return;
        }
        current = current->m_parent;
    }
    
    // Remove from previous parent if necessary (Reparenting)
    if (child->m_parent != nullptr) {
        child->m_parent->remove_child(child.get());
    }

    child->m_parent = this;
    m_children.push_back(std::move(child));
    mark_needs_measure();
}

void Widget::remove_child(Widget* child) noexcept {
    if (child == nullptr) return;

    auto it = std::find_if(m_children.begin(), m_children.end(), 
        [child](const Ref<Widget>& ref) { return ref.get() == child; });
        
    if (it != m_children.end()) {
        (*it)->m_parent = nullptr;
        m_children.erase(it);
        mark_needs_measure();
    }
}

void Widget::remove_all_children() noexcept {
    for (auto& child : m_children) {
        child->m_parent = nullptr;
    }
    m_children.clear();
    mark_needs_measure();
}

void Widget::mark_needs_measure() noexcept {
    if (m_needs_measure) return; // Already marked
    m_needs_measure = true;
    m_needs_layout = true;
    m_needs_paint = true;
    
    if (m_parent != nullptr) {
        m_parent->mark_needs_measure();
    }
}

void Widget::mark_needs_layout() noexcept {
    if (m_needs_layout) return;
    m_needs_layout = true;
    m_needs_paint = true;

    if (m_parent != nullptr) {
        m_parent->mark_needs_layout();
    }
}

void Widget::mark_needs_paint() noexcept {
    if (m_needs_paint) return;
    m_needs_paint = true;

    if (m_parent != nullptr) {
        m_parent->mark_needs_paint();
    }
}

void Widget::measure(const Constraints& constraints) noexcept {
    if (!m_needs_measure) return;

    m_desired_size = constraints.constrain(measure_override(constraints));
    m_needs_measure = false;
}

void Widget::layout(const Rect& frame) noexcept {
    if (!m_needs_layout && m_frame == frame) return;

    m_frame = frame;
    layout_override(frame);
    m_needs_layout = false;
}

void Widget::paint(Painter& painter) const noexcept {
    if (!m_needs_paint) {
        // In a real retained-mode renderer, we'd reuse the previous DisplayList/CommandList here.
        // For now, since Painter is immediate-mode command recording, we MUST always repaint 
        // to rebuild the command buffer for the current frame.
        // The dirty flags will be used correctly when we integrate rendering caching.
    }

    // Save painter state (transform/clip)
    // (No state saving required yet since Painter doesn't have it and frames are absolute)
    
    paint_override(painter);
    
    // (No restore needed either)
    
    // m_needs_paint cannot be mutated since `paint` is `const`, and rendering caching handles dirty flags later.
}

void Widget::paint_children(Painter& painter) const noexcept {
    for (const auto& child : m_children) {
        child->paint(painter);
    }
}

bool Widget::handle_event(const Event& event) noexcept {
    // Propagate to children backwards (top-most first)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if ((*it)->handle_event(event)) {
            return true;
        }
    }
    return false;
}

void Widget::layout_override(const Rect& frame) noexcept {
    // Default layout_override just positions children at the top-left with their desired size
    for (auto& child : m_children) {
        child->layout(Rect(frame.left(), frame.top(), child->desired_size().width, child->desired_size().height));
    }
}

} // namespace txui
