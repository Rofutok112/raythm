#pragma once

#include <optional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "data_models.h"
#include "network/auth_client.h"
#include "online_content_identity.h"
#include "play_mods.h"
#include "ranking_service.h"
#include "raylib.h"
#include "shared/scene_fade.h"
#include "ui_text_input.h"

namespace song_select {

struct chart_option {
    std::string path;
    chart_meta meta;
    content_kind kind = content_kind::local;
    storage_policy storage = storage_policy::plain_workspace;
    verification_state verification = verification_state::unchecked;
    content_status status = content_status::local;
    content_status source_status = content_status::local;
    std::optional<online_content::chart_identity> online_identity;
    std::vector<online_content::chart_identity> remote_links;
    int local_note_offset_ms = 0;
    std::optional<rank> best_local_rank;
    std::optional<int> best_local_score;
    int note_count = 0;
    float min_bpm = 0.0f;
    float max_bpm = 0.0f;
};

struct song_entry {
    song_data song;
    content_kind kind = content_kind::local;
    storage_policy storage = storage_policy::plain_workspace;
    verification_state verification = verification_state::unchecked;
    content_status status = content_status::local;
    content_status source_status = content_status::local;
    std::optional<online_content::song_identity> online_identity;
    std::vector<chart_option> charts;
};

struct catalog_data {
    std::vector<song_entry> songs;
    std::vector<std::string> load_errors;
};

class jacket_cache {
public:
    struct pending_texture {
        std::vector<unsigned char> bytes;
        std::string file_type;
    };

    jacket_cache() = default;
    ~jacket_cache();

    jacket_cache(const jacket_cache&) = delete;
    jacket_cache& operator=(const jacket_cache&) = delete;

    const Texture2D* get(const song_data& song);
    void poll();
    void clear();

private:
    struct cache_entry {
        Texture2D texture{};
        std::future<pending_texture> future;
        bool loaded = false;
        bool missing = false;
        bool requested = false;
    };

    std::unordered_map<std::string, cache_entry> entries_;
};

enum class context_menu_target {
    none,
    list_background,
    song,
    chart,
};

enum class chart_source_filter {
    all,
    official,
    community,
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
    ranking_service::source best_source = ranking_service::source::local;
    std::optional<ranking_service::entry> best_entry;
    std::string best_chart_id;
    bool best_loaded = false;
    bool source_dropdown_open = false;
    float scroll_y = 0.0f;
    float scroll_y_target = 0.0f;
    float reveal_anim = 0.0f;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
};

struct auth_state {
    bool logged_in = false;
    std::string server_url;
    std::string email;
    std::string display_name;
    std::string avatar_url;
    bool email_verified = false;
    std::vector<auth::external_link> external_links;
};

enum class login_dialog_mode {
    login,
    signup,
    verify,
};

struct login_dialog_state {
    bool open = false;
    login_dialog_mode mode = login_dialog_mode::login;
    float open_anim = 0.0f;
    auth::verification_purpose verification = auth::verification_purpose::none;
    std::string verification_email;
    ui::text_input_state display_name_input;
    ui::text_input_state email_input;
    ui::text_input_state password_input;
    ui::text_input_state password_confirmation_input;
    ui::text_input_state verification_code_input;
};

struct catalog_state {
    std::vector<song_entry> songs;
    std::vector<std::string> load_errors;
    std::shared_ptr<jacket_cache> jackets;
    bool catalog_loading = false;
    bool catalog_loaded_once = false;
};

struct selection_state {
    int selected_song_index = 0;
    int difficulty_index = 0;
    bool selected_song_expanded = true;
    float selected_song_expand_t = 1.0f;
};

struct filter_state {
    ui::text_input_state play_search_input;
    chart_source_filter chart_source = chart_source_filter::all;
    bool play_filter_modal_open = false;
    bool play_mod_modal_open = false;
    play_mods mods;
    bool multiplayer_queueable_only = false;
    std::string multiplayer_queue_server_url;
    int chart_key_filter = 0;
    float chart_min_level = 0.0f;
    float chart_max_level = 99.0f;
    bool chart_level_filter_dragging = false;
    bool chart_level_filter_dragging_min = false;
};

struct scroll_state {
    float scroll_y = 0.0f;
    float scroll_y_target = 0.0f;
    float chart_scroll_y = 0.0f;
    float chart_scroll_y_target = 0.0f;
    float embedded_chart_scroll_y = 0.0f;
    float embedded_chart_scroll_y_target = 0.0f;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
};

struct preview_state {
    bool preview_bar_dragging = false;
    bool preview_bar_resume_after_drag = false;
    double preview_bar_drag_position_seconds = 0.0;
    float song_change_anim_t = 0.0f;
    float chart_change_anim_t = 0.0f;
    scene_fade scene_fade_in{scene_fade::direction::in, 0.3f, 0.65f};
    std::optional<recent_result_offset> recent_result_offset;
};

struct dialog_state {
    context_menu_state context_menu;
    confirmation_dialog_state confirmation_dialog;
};

struct auth_ui_state {
    auth_state auth;
    login_dialog_state login_dialog;
};

struct state {
    state();
    state(const state& other);
    state& operator=(const state& other);

    catalog_state catalog;
    selection_state selection;
    filter_state filter;
    scroll_state scroll;
    preview_state preview;
    dialog_state dialog;
    ranking_panel_state ranking_panel;
    auth_ui_state auth_ui;

    std::vector<song_entry>& songs;
    std::vector<std::string>& load_errors;
    std::shared_ptr<jacket_cache>& jackets;
    bool& catalog_loading;
    bool& catalog_loaded_once;
    int& selected_song_index;
    int& difficulty_index;
    float& scroll_y;
    float& scroll_y_target;
    float& chart_scroll_y;
    float& chart_scroll_y_target;
    float& embedded_chart_scroll_y;
    float& embedded_chart_scroll_y_target;
    bool& selected_song_expanded;
    float& selected_song_expand_t;
    ui::text_input_state& play_search_input;
    chart_source_filter& chart_source;
    bool& play_filter_modal_open;
    bool& play_mod_modal_open;
    play_mods& mods;
    int& chart_key_filter;
    float& chart_min_level;
    float& chart_max_level;
    bool& chart_level_filter_dragging;
    bool& chart_level_filter_dragging_min;
    bool& preview_bar_dragging;
    bool& preview_bar_resume_after_drag;
    double& preview_bar_drag_position_seconds;
    float& song_change_anim_t;
    float& chart_change_anim_t;
    scene_fade& scene_fade_in;
    bool& scrollbar_dragging;
    float& scrollbar_drag_offset;
    context_menu_state& context_menu;
    confirmation_dialog_state& confirmation_dialog;
    std::optional<recent_result_offset>& recent_result_offset;
    auth_state& auth;
    login_dialog_state& login_dialog;
};

const song_entry* selected_song(const state& state);
std::vector<int> filtered_song_indices(const state& state);
std::vector<const chart_option*> filtered_charts_for_selected_song(const state& state);
const chart_option* selected_chart_for(const state& state, const std::vector<const chart_option*>& filtered);

void reset_for_enter(state& state);
void tick_animations(state& state, float dt);
void apply_catalog(state& state, catalog_data catalog,
                   const std::string& preferred_song_id = "",
                   const std::string& preferred_chart_id = "");
bool apply_song_selection(state& state, int song_index, int chart_index = 0);
bool apply_chart_filters(state& state,
                         chart_source_filter source,
                         int key_filter,
                         float min_level,
                         float max_level);
bool clear_chart_filters(state& state);

void open_song_context_menu(state& state, int song_index, Rectangle rect);
void open_chart_context_menu(state& state, int song_index, int chart_index, Rectangle rect);
void open_list_background_context_menu(state& state, Rectangle rect);
void close_context_menu(state& state);
void queue_status_message(state& state, std::string message, bool is_error);

float expanded_row_height(const state& state, int song_index);
float song_list_content_top();
float song_list_first_item_y(const state& state);
float content_height(const state& state);
std::string fallback_song_id_after_song_delete(const state& state, int song_index);
std::string fallback_chart_id_after_chart_delete(const state& state, int song_index, int chart_index);

}  // namespace song_select
