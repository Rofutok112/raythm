//
// Created by rento on 2026/03/08.
//

#include "ui_renderer.h"

void ui_renderer::render(const ui_render_queue &q) {
    for (const auto &cmd : q.get_render_commands()) {
        switch (cmd.kind) {
            case render_command::type::rect :
                DrawRectangleRec(cmd.bounds, cmd.color);
                break;
            case render_command::type::text :
                break;
            case render_command::type::circle :
                break;
            default:
                break;
        }
    }
}
