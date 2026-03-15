//
// Created by rento on 2026/03/08.
//

#include "button.h"

#include "ui_render_queue.h"

button::button(Rectangle rect, Color color) : color_(color) {
    bounds = rect;
}

void button::set_color(Color color) {
    color_ = color;
}

Color button::color() const {
    return color_;
}

void button::build_render_data(ui_render_queue &queue) const {
    queue.enqueue({
        .kind = render_command::type::rect,
        .bounds = bounds,
        .color = color_
    });
}
