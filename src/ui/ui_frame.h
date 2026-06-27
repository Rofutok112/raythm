#pragma once

#include "ui_draw.h"

namespace ui {

struct frame_options {
    bool reset_hit_regions = true;
    bool reset_draw_queue = true;
};

inline void begin_frame(frame_options options = {}) {
    if (options.reset_hit_regions) {
        begin_hit_regions();
    }
    if (options.reset_draw_queue) {
        begin_draw_queue();
    }
}

inline void end_frame() {
    flush_draw_queue();
}

inline void begin_input_frame() {
    begin_frame({.reset_hit_regions = true, .reset_draw_queue = false});
}

inline void begin_render_frame() {
    begin_frame({.reset_hit_regions = false, .reset_draw_queue = true});
}

inline void end_render_frame() {
    end_frame();
}

}  // namespace ui
