#include "ui_clip.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ui_coord.h"

namespace ui {
namespace {

Rectangle intersect_rectangles(Rectangle a, Rectangle b) {
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    return {
        left,
        top,
        std::max(0.0f, right - left),
        std::max(0.0f, bottom - top)
    };
}

std::vector<Rectangle>& clip_rect_stack() {
    static std::vector<Rectangle> stack;
    return stack;
}

void apply_clip_rect(Rectangle rect) {
    begin_scissor_rect(rect);
}

}  // namespace

void push_clip_rect(Rectangle rect) {
    std::vector<Rectangle>& stack = clip_rect_stack();
    const Rectangle merged = stack.empty() ? rect : intersect_rectangles(stack.back(), rect);

    if (!stack.empty()) {
        EndScissorMode();
    }

    stack.push_back(merged);
    apply_clip_rect(merged);
}

void pop_clip_rect() {
    std::vector<Rectangle>& stack = clip_rect_stack();
    if (stack.empty()) {
        return;
    }

    EndScissorMode();
    stack.pop_back();

    if (!stack.empty()) {
        apply_clip_rect(stack.back());
    }
}

bool has_clip_rect() {
    return !clip_rect_stack().empty();
}

Rectangle current_clip_rect() {
    const std::vector<Rectangle>& stack = clip_rect_stack();
    if (stack.empty()) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    return stack.back();
}

scoped_clip_rect::scoped_clip_rect(Rectangle rect) : active_(true) {
    push_clip_rect(rect);
}

scoped_clip_rect::~scoped_clip_rect() {
    if (active_) {
        pop_clip_rect();
    }
}

scoped_clip_rect::scoped_clip_rect(scoped_clip_rect&& other) noexcept
    : active_(std::exchange(other.active_, false)) {
}

scoped_clip_rect& scoped_clip_rect::operator=(scoped_clip_rect&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (active_) {
        pop_clip_rect();
    }

    active_ = std::exchange(other.active_, false);
    return *this;
}

}  // namespace ui
