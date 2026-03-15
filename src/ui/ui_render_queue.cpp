//
// Created by rento on 2026/03/08.
//

#include "ui_render_queue.h"

void ui_render_queue::enqueue(const render_command &cmd) {
    commands_.push_back(cmd);
}

void ui_render_queue::clear() {
    commands_.clear();
}

const std::pmr::vector<render_command> &ui_render_queue::get_render_commands() const {
    return commands_;
}
