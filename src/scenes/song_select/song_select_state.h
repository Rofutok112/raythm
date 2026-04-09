#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "ranking_service.h"
#include "raylib.h"
#include "shared/scene_fade.h"

namespace song_select {

struct chart_option {
    std::string path;
    chart_meta meta;
    content_source source = content_source::official;
    bool can_delete = false;
    int local_note_offset_ms = 0;
    std::optional<rank> best_local_rank;
};

struct song_entry {
    song_data song;
    std::vector<chart_option> charts;
};

struct catalog_data {
    std::vector<song_entry> songs;
    std::vector<std::string> load_errors;
};

enum class context_menu_target {
    none,
    list_background,
    song,
    chart,
};

enum class context_menu_section {
    root,
    song,
    chart,
    mv,
};

enum class pending_confirmation_action {
    none,
    delete_song,
    delete_chart,
    delete_mv,
    overwrite_song_import,
    overwrite_chart_import,
};

struct context_menu_state {
    bool open = false;
    context_menu_target target = context_menu_target::none;
    context_menu_section section = context_menu_section::root;
    int song_index = -1;
    int chart_index = -1;
    Rectangle rect = {};
};

struct confirmation_dialog_state {
    bool open = false;
    pending_confirmation_action action = pending_confirmation_action::none;
    int song_index = -1;
    int chart_index = -1;
    bool suppress_initial_pointer_cancel = false;
    std::string title;
    std::string message;
    std::string hint;
    std::string confirm_label = "CONFIRM";
};

struct recent_result_offset {
    std::string song_id;
    std::string chart_id;
    float avg_offset_ms = 0.0f;
};

struct ranking_panel_state {
    ranking_service::source selected_source = ranking_service::source::local;
    ranking_service::listing listing;
    bool source_dropdown_open = false;
    float scroll_y = 0.0f;
    float scroll_y_target = 0.0f;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
};

struct state {
    std::vector<song_entry> songs;
    std::vector<std::string> load_errors;
    int selected_song_index = 0;
    int difficulty_index = 0;
    float scroll_y = 0.0f;
    float scroll_y_target = 0.0f;
    float song_change_anim_t = 0.0f;
    float chart_change_anim_t = 0.0f;
    scene_fade scene_fade_in{scene_fade::direction::in, 0.3f, 0.65f};
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    context_menu_state context_menu;
    confirmation_dialog_state confirmation_dialog;
    std::string status_message;
    bool status_message_is_error = false;
    std::optional<recent_result_offset> recent_result_offset;
    ranking_panel_state ranking_panel;
};

const song_entry* selected_song(const state& state);
std::vector<const chart_option*> filtered_charts_for_selected_song(const state& state);
const chart_option* selected_chart_for(const state& state, const std::vector<const chart_option*>& filtered);

void reset_for_enter(state& state);
void tick_animations(state& state, float dt);
void apply_catalog(state& state, catalog_data catalog,
                   const std::string& preferred_song_id = "",
                   const std::string& preferred_chart_id = "");
bool apply_song_selection(state& state, int song_index, int chart_index = 0);

void open_song_context_menu(state& state, int song_index, Rectangle rect);
void open_chart_context_menu(state& state, int song_index, int chart_index, Rectangle rect);
void open_list_background_context_menu(state& state, Rectangle rect);
void close_context_menu(state& state);
void queue_status_message(state& state, std::string message, bool is_error);

float expanded_row_height(const state& state, int song_index);
float content_height(const state& state);
std::string fallback_song_id_after_song_delete(const state& state, int song_index);
std::string fallback_chart_id_after_chart_delete(const state& state, int song_index, int chart_index);

}  // namespace song_select
