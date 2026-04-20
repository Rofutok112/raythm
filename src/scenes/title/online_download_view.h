#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"
#include "ui_notice.h"
#include "ui_text_input.h"

namespace title_online_view {

enum class catalog_mode {
    official,
    community,
};

enum class requested_action {
    none,
    primary,
    open_local,
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
};

class jacket_cache {
public:
    jacket_cache() = default;
    ~jacket_cache();

    jacket_cache(const jacket_cache&) = delete;
    jacket_cache& operator=(const jacket_cache&) = delete;

    const Texture2D* get(const song_data& song);
    void clear();

private:
    struct cache_entry {
        Texture2D texture{};
        bool loaded = false;
        bool missing = false;
    };

    std::unordered_map<std::string, cache_entry> entries_;
};

struct state {
    std::vector<song_entry_state> official_songs;
    std::vector<song_entry_state> community_songs;
    catalog_mode mode = catalog_mode::official;
    int official_selected_song_index = 0;
    int community_selected_song_index = 0;
    int official_selected_chart_index = 0;
    int community_selected_chart_index = 0;
    float song_scroll_y = 0.0f;
    float song_scroll_y_target = 0.0f;
    float chart_scroll_y = 0.0f;
    float chart_scroll_y_target = 0.0f;
    ui::text_input_state search_input;
    ui::notice_queue notices;
    jacket_cache jackets;
};

struct layout {
    Rectangle back_rect;
    Rectangle official_tab_rect;
    Rectangle community_tab_rect;
    Rectangle search_rect;
    Rectangle song_lane_rect;
    Rectangle detail_rect;
    Rectangle hero_jacket_rect;
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
void on_enter(state& state, song_select::preview_controller& preview_controller);
void on_exit(state& state);

const song_entry_state* selected_song(const state& state);
const chart_entry_state* selected_chart(const state& state);
const song_select::song_entry* preview_song(const state& state);
bool can_open_local(const state& state);
std::string selected_song_id(const state& state);

layout make_layout(float anim_t, Rectangle origin_rect);
update_result update(state& state, float anim_t, Rectangle origin_rect, float dt);
void draw(state& state, float anim_t, Rectangle origin_rect);

}  // namespace title_online_view
