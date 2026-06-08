#pragma once

#include <functional>
#include <string>

#include "load_progress.h"
#include "song_select/song_select_state.h"
#include "title/catalog_reload_policy.h"

namespace title_startup_controller {

struct state {
    bool loading = false;
    bool catalog_requested = false;
    bool fonts_preload_started = false;
    bool fonts_preloaded = false;
    bool scoring_requested = false;
    bool load_complete = false;
    bool load_failed = false;
    float progress_visual = 0.0f;
    float catalog_progress = 0.0f;
    std::string loading_message;
};

struct update_context {
    song_select::state& play_state;
    std::string preferred_song_id;
    std::string preferred_chart_id;
    bool sync_media_on_catalog_apply = false;
    std::string& home_status_message;
    std::function<void(std::string, std::string, title_catalog::reload_policy)> request_play_catalog_reload;
    std::function<bool()> play_catalog_loading;
    std::function<void()> reload_online_catalog;
    std::function<void()> restore_auth;
    std::function<void(bool)> request_scoring_ruleset_warm;
    std::function<bool()> scoring_ruleset_loading;
    std::function<load_progress()> catalog_progress;
};

void reset(state& startup);
void update(state& startup, const update_context& context);
void draw_loading(state& startup, float dt);
void draw_status(const state& startup);

}  // namespace title_startup_controller
