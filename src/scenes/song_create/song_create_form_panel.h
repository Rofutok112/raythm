#pragma once

#include <functional>
#include <string>
#include <vector>

#include "data_models.h"
#include "raylib.h"
#include "shared/square_image_picker.h"
#include "song_create/song_create_tag_editor.h"
#include "ui_draw.h"
#include "ui_text_input.h"

namespace song_create::form_panel {

struct state_refs {
    ui::text_input_state& title_input;
    ui::text_input_state& artist_input;
    ui::text_input_state& genre_search_input;
    ui::text_input_state& keyword_input;
    ui::text_input_state& audio_path_input;
    ui::text_input_state& jacket_path_input;
    ui::text_input_state& preview_ms_input;
    const std::vector<std::string>& selected_genres;
    const std::vector<std::string>& selected_keywords;
    square_image_picker::state& jacket_picker;
    std::string& error;
};

struct callbacks {
    std::function<bool(Rectangle)> draw_timing_summary;
};

struct config {
    float form_x = 0.0f;
    float form_width = 0.0f;
    float form_start_y = 0.0f;
    float row_height = 63.0f;
    float row_gap = 9.0f;
    float text_input_label_width = 180.0f;
    ui::draw_layer layer = ui::draw_layer::base;
    bool edit_mode = false;
};

struct result {
    tag_editor::genre_selector_result genre_selector;
    tag_editor::keyword_editor_result keyword_editor;
    bool timing_summary_open_requested = false;
    bool browse_audio_requested = false;
    bool browse_jacket_requested = false;
    bool submit_requested = false;
    bool cancel_requested = false;
};

result draw(state_refs state, const callbacks& actions, const config& view_config);

}  // namespace song_create::form_panel
