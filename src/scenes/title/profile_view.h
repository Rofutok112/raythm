#pragma once

#include <array>
#include <string>
#include <vector>

#include "network/auth_client.h"
#include "raylib.h"
#include "shared/square_image_picker.h"
#include "song_select/song_select_state.h"
#include "ui_draw.h"
#include "ui_text_input.h"

namespace title_profile_view {

enum class tab {
    overview,
    activity,
    best_rc,
    songs,
    charts,
    settings,
};

enum class delete_target {
    none,
    account,
    song,
    chart,
};

enum class command_type {
    none,
    close,
    select_tab,
    begin_delete,
    cancel_delete,
    delete_account,
    delete_song,
    delete_chart,
    save_external_links,
    change_avatar,
    remove_avatar,
};

struct command {
    command_type type = command_type::none;
    std::string id;
    std::string password;
    std::vector<auth::external_link> external_links;
    tab selected_tab = tab::overview;
    delete_target pending_delete = delete_target::none;
    std::string delete_label;
};

struct scroll_offsets {
    float activity = 0.0f;
    float best_rating = 0.0f;
    float songs = 0.0f;
    float charts = 0.0f;
};

struct input_result {
    command action;
    bool scroll_changed = false;
    scroll_offsets scroll;
    bool release_background_close_suppression = false;
};

struct activity_item {
    std::string song_title;
    std::string artist;
    std::string genre;
    std::string difficulty_name;
    std::string local_summary;
    std::string online_summary;
    float play_rating = 0.0f;
    float rating_contribution = 0.0f;
    float rating_contribution_percent = 0.0f;
};

struct load_result {
    auth::my_uploads_result uploads;
    auth::profile_rankings_result rankings;
    std::vector<activity_item> activity;
    std::vector<activity_item> best_rating_records;
    std::vector<activity_item> first_place_records;
};

struct state {
    bool open = false;
    bool closing = false;
    bool loading = false;
    bool deleting = false;
    bool saving_links = false;
    bool saving_avatar = false;
    bool loaded_once = false;
    bool settings_links_initialized = false;
    bool suppress_background_close_until_release = false;
    float open_anim = 0.0f;
    tab selected_tab = tab::overview;
    float activity_scroll = 0.0f;
    float best_rating_scroll = 0.0f;
    float song_scroll = 0.0f;
    float chart_scroll = 0.0f;
    auth::my_uploads_result uploads;
    auth::profile_rankings_result rankings;
    std::vector<activity_item> activity;
    std::vector<activity_item> best_rating_records;
    std::vector<activity_item> first_place_records;
    delete_target pending_delete = delete_target::none;
    std::string pending_id;
    std::string pending_label;
    ui::text_input_state delete_password_input;
    std::array<ui::text_input_state, 3> link_label_inputs;
    std::array<ui::text_input_state, 3> link_url_inputs;
};

Rectangle bounds();
void open(state& profile);
void close(state& profile);
[[nodiscard]] scroll_offsets clamped_scroll_offsets(const state& profile);
[[nodiscard]] input_result update(const state& profile, bool request_active);
void draw(state& profile,
          const song_select::auth_state& auth_state,
          square_image_picker::state& avatar_picker,
          bool request_active,
          ui::draw_layer layer = ui::draw_layer::modal,
          bool draw_backdrop = true);

}  // namespace title_profile_view
