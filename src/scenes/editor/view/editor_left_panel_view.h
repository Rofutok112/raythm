#pragma once

#include <optional>
#include <string>

#include "editor/editor_scene_types.h"
#include "ui_text_input.h"

struct editor_left_panel_view_model {
    const char* song_title = "";
    bool has_file = false;
    bool is_dirty = false;
    metadata_panel_state* metadata_panel = nullptr;
    const char* current_key_mode_label = "";
    int current_offset_ms = 0;
    int note_count = 0;
    const std::string* load_error = nullptr;
    double now = 0.0;
};

struct editor_left_panel_view_result {
    ui::text_input_result difficulty_result;
    ui::text_input_result author_result;
    bool key_count_left_clicked = false;
    bool key_count_right_clicked = false;
};

class editor_left_panel_view final {
public:
    static editor_left_panel_view_result draw(const editor_left_panel_view_model& model);
};
