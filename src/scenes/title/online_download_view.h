#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"
#include "title/online_download_remote_client.h"
#include "ui_notice.h"
#include "ui_text_input.h"

namespace title_online_view {

enum class catalog_mode {
    official,
    community,
    owned,
};

enum class requested_action {
    none,
    primary,
    open_local,
    restart_preview,
    stop_preview,
};

struct chart_entry_state {
    song_select::chart_option chart;
    bool installed = false;
    bool update_available = false;
};

struct song_entry_state {
    song_select::song_entry song;
    std::vector<chart_entry_state> charts;
    bool installed = false;
    bool update_available = false;
    bool charts_loaded = false;
    bool charts_loading = false;
    bool charts_has_more = false;
    bool charts_failed = false;
    int next_chart_page = 1;
};

struct catalog_load_result {
    std::vector<song_entry_state> official_songs;
    std::vector<song_entry_state> community_songs;
    std::vector<song_entry_state> owned_songs;
    std::vector<song_select::song_entry> local_songs;
    std::string catalog_server_url;
    std::string catalog_status_message;
    bool catalog_request_failed = false;
    bool official_has_more = false;
    bool community_has_more = false;
};

struct download_song_result {
    bool success = false;
    std::string message;
    std::string song_id;
};

struct download_progress_state {
    std::atomic<int> total_steps{0};
    std::atomic<int> completed_steps{0};
    std::atomic<size_t> current_bytes{0};
    std::atomic<size_t> current_total_bytes{0};
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

struct state {
    std::vector<song_entry_state> official_songs;
    std::vector<song_entry_state> community_songs;
    std::vector<song_entry_state> owned_songs;
    std::vector<song_select::song_entry> local_songs;
    catalog_mode mode = catalog_mode::official;
    int official_selected_song_index = 0;
    int community_selected_song_index = 0;
    int owned_selected_song_index = 0;
    int official_selected_chart_index = 0;
    int community_selected_chart_index = 0;
    int owned_selected_chart_index = 0;
    float song_scroll_y = 0.0f;
    float song_scroll_y_target = 0.0f;
    float chart_scroll_y = 0.0f;
    float chart_scroll_y_target = 0.0f;
    ui::text_input_state search_input;
    ui::notice_queue notices;
    jacket_cache jackets;
    std::future<catalog_load_result> catalog_future;
    std::future<remote_song_page_fetch_result> song_page_future;
    std::future<remote_chart_page_fetch_result> chart_page_future;
    std::future<std::vector<song_entry_state>> owned_future;
    std::future<download_song_result> download_future;
    std::shared_ptr<download_progress_state> download_progress;
    bool catalog_loading = false;
    bool catalog_loaded_once = false;
    bool official_has_more = false;
    bool community_has_more = false;
    int official_next_page = 1;
    int community_next_page = 1;
    bool song_page_loading = false;
    catalog_mode song_page_mode = catalog_mode::official;
    bool chart_page_loading = false;
    catalog_mode chart_page_mode = catalog_mode::official;
    std::string chart_page_song_id;
    bool owned_loading = false;
    bool owned_loaded_once = false;
    bool download_in_progress = false;
    std::string catalog_server_url;
    std::string catalog_status_message;
    bool catalog_request_failed = false;
    bool detail_open = false;
    float detail_transition = 0.0f;
};

struct layout {
    Rectangle back_rect;
    Rectangle official_tab_rect;
    Rectangle community_tab_rect;
    Rectangle owned_tab_rect;
    Rectangle search_rect;
    Rectangle content_rect;
    Rectangle song_grid_rect;
    Rectangle detail_left_rect;
    Rectangle detail_right_rect;
    Rectangle hero_jacket_rect;
    Rectangle preview_bar_rect;
    Rectangle preview_play_rect;
    Rectangle preview_stop_rect;
    Rectangle chart_list_rect;
    Rectangle primary_action_rect;
    Rectangle open_local_rect;
};

struct update_result {
    bool back_requested = false;
    bool song_selection_changed = false;
    bool chart_selection_changed = false;
    requested_action action = requested_action::none;
};

void reload_catalog(state& state);
bool poll_catalog(state& state);
bool poll_song_page(state& state);
bool poll_chart_page(state& state);
bool poll_owned(state& state);
void request_next_song_page(state& state, catalog_mode mode);
void request_charts_for_selected_song(state& state);
void start_download(state& state);
bool poll_download(state& state);
void mark_song_removed(state& state, const std::string& song_id);
void on_enter(state& state, song_select::preview_controller& preview_controller);
void on_exit(state& state);

const song_entry_state* selected_song(const state& state);
const chart_entry_state* selected_chart(const state& state);
const song_select::song_entry* preview_song(const state& state);
bool needs_download(const song_entry_state& song);
bool can_open_local(const state& state);
std::string selected_song_id(const state& state);

layout make_layout(float anim_t, Rectangle origin_rect);
update_result update(state& state, float anim_t, Rectangle origin_rect, float dt);
void draw(state& state, float anim_t, Rectangle origin_rect);

}  // namespace title_online_view
