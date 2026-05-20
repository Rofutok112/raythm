#include "title/title_hub_update_coordinator.h"

#include <utility>

void title_hub_update_coordinator::poll_feature_work(bool online_mode_active,
                                                     const callbacks& callbacks) {
    callbacks.poll_auth();
    callbacks.poll_play_catalog_reload();
    callbacks.poll_play_transfer();
    callbacks.poll_play_ranking_reload();
    callbacks.poll_scoring_ruleset_warm();
    callbacks.poll_create_upload();

    if (callbacks.poll_profile_content_changed()) {
        callbacks.refresh_auth_state();
        callbacks.reload_online_catalog();
        callbacks.request_play_catalog_reload();
    }

    callbacks.poll_online_song_page();
    callbacks.poll_online_chart_page();
    callbacks.poll_online_owned();
    callbacks.close_profile_if_logged_out();

    if (callbacks.poll_online_download()) {
        callbacks.request_downloaded_play_catalog_reload(callbacks.selected_online_song_id());
    }
    if (callbacks.poll_online_catalog() && online_mode_active) {
        callbacks.select_online_preview_song();
    }
}
