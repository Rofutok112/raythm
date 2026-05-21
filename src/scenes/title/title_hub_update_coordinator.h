#pragma once

#include <functional>
#include <string>

class title_hub_update_coordinator {
public:
    struct callbacks {
        std::function<void()> poll_auth;
        std::function<void()> poll_play_catalog_reload;
        std::function<void()> poll_play_transfer;
        std::function<void()> poll_play_ranking_reload;
        std::function<void()> poll_scoring_ruleset_warm;
        std::function<void()> poll_create_upload;
        std::function<bool()> poll_profile_content_changed;
        std::function<void()> refresh_auth_state;
        std::function<void()> reload_online_catalog;
        std::function<void()> request_play_catalog_reload;
        std::function<void()> poll_online_song_page;
        std::function<void()> poll_online_chart_page;
        std::function<void()> poll_online_owned;
        std::function<void()> close_profile_if_logged_out;
        std::function<bool()> poll_online_download;
        std::function<std::string()> selected_online_song_id;
        std::function<void(std::string song_id)> request_downloaded_play_catalog_reload;
        std::function<bool()> poll_online_catalog;
        std::function<void()> select_online_preview_song;
    };

    static void poll_feature_work(bool online_mode_active, const callbacks& callbacks);
};
