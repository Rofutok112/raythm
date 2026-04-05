#pragma once

#include <span>

#include "ui_draw.h"

struct editor_header_view_model {
    const char* playback_status = "";
    bool audio_loaded = false;
    const char* offset_label = "";
    bool waveform_visible = true;
    std::span<const char* const> snap_labels = {};
    int snap_index = 0;
    bool snap_dropdown_open = false;
};

struct editor_header_view_result {
    bool offset_left_clicked = false;
    bool offset_right_clicked = false;
    bool waveform_toggled = false;
    int snap_index_clicked = -1;
    bool snap_dropdown_toggled = false;
    bool snap_dropdown_close_requested = false;
};

class editor_header_view final {
public:
    static editor_header_view_result draw(const editor_header_view_model& model, Rectangle snap_menu_rect);
};
