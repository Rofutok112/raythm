#pragma once

#include <functional>
#include <string>
#include <vector>

#include "data_models.h"
#include "raylib.h"
#include "shared/square_image_picker.h"
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
    std::vector<std::string>& selected_genres;
    std::vector<std::string>& selected_keywords;
    square_image_picker::state& jacket_picker;
    std::string& error;
};

struct callbacks {
    std::function<void(Rectangle)> draw_timing_summary;
    std::function<std::string()> browse_audio;
    std::function<void()> browse_jacket;
    std::function<bool()> submit;
    std::function<void()> cancel;
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
    bool created_song = false;
};

result draw(state_refs state, const callbacks& actions, const config& view_config);

}  // namespace song_create::form_panel
