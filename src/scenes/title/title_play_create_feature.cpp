#include "title/title_play_create_feature.h"

#include <utility>

#include "ranking_service.h"
#include "title/play_session_controller.h"

song_select::state& title_play_create_feature::state() {
    return state_;
}

const song_select::state& title_play_create_feature::state() const {
    return state_;
}

title_play_transfer_controller& title_play_create_feature::transfer_controller() {
    return transfer_controller_;
}

const title_play_transfer_controller& title_play_create_feature::transfer_controller() const {
    return transfer_controller_;
}

void title_play_create_feature::reset() {
    song_select::reset_for_enter(state_);
    data_controller_.reset(state_);
}

void title_play_create_feature::on_exit() {
    transfer_controller_.on_exit();
}

void title_play_create_feature::on_enter_play(bool multiplayer_chart_pick_active,
                                             const std::string& multiplayer_server_url,
                                             song_select::preview_controller& preview_controller) {
    state_.ranking_panel.selected_source = ranking_service::source::online;
    state_.filter.multiplayer_queueable_only = multiplayer_chart_pick_active;
    state_.filter.multiplayer_queue_server_url = multiplayer_chart_pick_active ? multiplayer_server_url : "";
    sync_play_media(preview_controller);
}

void title_play_create_feature::on_enter_create(song_select::preview_controller& preview_controller) {
    capture_current_selection();
    request_catalog_reload(preferred_song_id_, preferred_chart_id_, false, true);
    sync_create_preview(preview_controller);
}

void title_play_create_feature::request_catalog_reload(std::string preferred_song_id,
                                                       std::string preferred_chart_id,
                                                       bool sync_media_on_apply,
                                                       bool calculate_missing_levels) {
    data_controller_.request_catalog_reload(state_, std::move(preferred_song_id),
                                            std::move(preferred_chart_id),
                                            sync_media_on_apply, calculate_missing_levels);
}

void title_play_create_feature::poll_catalog_reload(song_select::preview_controller& preview_controller,
                                                   bool play_mode_active,
                                                   bool create_mode_active) {
    const title_play_data_controller::catalog_poll_result result =
        data_controller_.poll_catalog_reload(state_, play_mode_active, create_mode_active);
    if (result.sync_play_media) {
        sync_play_media(preview_controller);
    } else if (result.sync_create_preview) {
        sync_create_preview(preview_controller);
    }
}

void title_play_create_feature::request_ranking_reload() {
    data_controller_.request_ranking_reload(state_);
}

void title_play_create_feature::poll_ranking_reload() {
    data_controller_.poll_ranking_reload(state_);
}

void title_play_create_feature::request_scoring_ruleset_warm(bool force_refresh) {
    data_controller_.request_scoring_ruleset_warm(force_refresh);
}

bool title_play_create_feature::poll_scoring_ruleset_warm() {
    return data_controller_.poll_scoring_ruleset_warm();
}

bool title_play_create_feature::catalog_loading() const {
    return data_controller_.catalog_loading();
}

bool title_play_create_feature::scoring_ruleset_loading() const {
    return data_controller_.scoring_ruleset_loading();
}

bool title_play_create_feature::upload_in_progress() const {
    return data_controller_.upload_in_progress();
}

bool title_play_create_feature::busy() const {
    return transfer_controller_.busy();
}

void title_play_create_feature::poll_transfer(const cross_callbacks& callbacks, bool sync_media_on_reload) {
    transfer_controller_.poll(state_, make_transfer_callbacks(callbacks), sync_media_on_reload);
}

bool title_play_create_feature::poll_create_upload(bool sync_media_on_apply) {
    if (!data_controller_.poll_create_upload(state_).refresh_catalog) {
        return false;
    }
    capture_current_selection();
    request_catalog_reload(preferred_song_id_, preferred_chart_id_, sync_media_on_apply, true);
    return true;
}

void title_play_create_feature::cancel_confirmation() {
    transfer_controller_.cancel_confirmation(state_);
}

void title_play_create_feature::draw_or_apply_confirmation(song_select::preview_controller& preview_controller,
                                                          const cross_callbacks& callbacks,
                                                          bool sync_media_on_reload) {
    transfer_controller_.draw_or_apply_confirmation(
        state_, preview_controller, make_transfer_callbacks(callbacks), sync_media_on_reload);
}

void title_play_create_feature::update_play(scene_manager& manager,
                                            song_select::preview_controller& preview_controller,
                                            float anim_t,
                                            Rectangle origin_rect,
                                            float dt,
                                            const play_update_callbacks& callbacks) {
    title_play_mode_controller::update(
        manager,
        state_,
        preview_controller,
        transfer_controller_,
        anim_t,
        origin_rect,
        dt,
        {
            .enter_home = callbacks.enter_home,
            .sync_media = [this, &preview_controller]() { sync_play_media(preview_controller); },
            .request_ranking_reload = [this]() { request_ranking_reload(); },
            .open_update_catalog = [this, &callbacks](bool include_chart) {
                const song_select::song_entry* song = song_select::selected_song(state_);
                if (song == nullptr || !callbacks.open_update_catalog) {
                    return;
                }
                std::string chart_id;
                const auto filtered = song_select::filtered_charts_for_selected_song(state_);
                const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
                if (include_chart && chart != nullptr) {
                    chart_id = chart->meta.chart_id;
                }
                callbacks.open_update_catalog(song->song.meta.song_id, chart_id);
            },
            .add_selected_to_multiplayer = callbacks.add_selected_to_multiplayer,
        });
}

void title_play_create_feature::update_create(scene_manager& manager,
                                              song_select::preview_controller& preview_controller,
                                              float anim_t,
                                              Rectangle origin_rect,
                                              float dt,
                                              const cross_callbacks& cross,
                                              const create_update_callbacks& callbacks) {
    title_create_mode_controller::update(
        manager,
        state_,
        transfer_controller_,
        anim_t,
        origin_rect,
        dt,
        {
            .enter_home = callbacks.enter_home,
            .sync_preview = [this, &preview_controller]() { sync_create_preview(preview_controller); },
            .start_song_upload = [this](const song_select::song_entry& song) {
                data_controller_.start_song_upload(song);
            },
            .start_chart_upload = [this](const song_select::song_entry& song,
                                         const song_select::chart_option& chart) {
                data_controller_.start_chart_upload(song, chart);
            },
            .transfer_callbacks = [this, &cross]() {
                return make_transfer_callbacks(cross);
            },
            .sync_media_on_transfer = []() {
                return true;
            },
            .upload_in_progress = [this]() {
                return upload_in_progress();
            },
        });
}

void title_play_create_feature::capture_current_selection() {
    const song_select::song_entry* song = song_select::selected_song(state_);
    if (song == nullptr) {
        return;
    }

    preferred_song_id_ = song->song.meta.song_id;
    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    if (const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered)) {
        preferred_chart_id_ = chart->meta.chart_id;
    } else {
        preferred_chart_id_.clear();
    }
}

const std::string& title_play_create_feature::preferred_song_id() const {
    return preferred_song_id_;
}

const std::string& title_play_create_feature::preferred_chart_id() const {
    return preferred_chart_id_;
}

void title_play_create_feature::set_preferred_selection(std::string song_id, std::string chart_id) {
    preferred_song_id_ = std::move(song_id);
    preferred_chart_id_ = std::move(chart_id);
}

title_play_transfer_controller::catalog_callbacks title_play_create_feature::make_transfer_callbacks(
    const cross_callbacks& callbacks) {
    return {
        .set_preferred_selection = [this](const std::string& song_id, const std::string& chart_id) {
            preferred_song_id_ = song_id;
            preferred_chart_id_ = chart_id;
        },
        .stop_preview = callbacks.stop_preview,
        .mark_online_song_removed = callbacks.mark_online_song_removed,
        .reload_online_catalog = callbacks.reload_online_catalog,
        .request_play_catalog_reload =
            [this](const std::string& song_id, const std::string& chart_id, bool sync_media_on_apply) {
                request_catalog_reload(song_id, chart_id, sync_media_on_apply);
            },
    };
}

void title_play_create_feature::sync_play_media(song_select::preview_controller& preview_controller) {
    title_play_session::sync_preview(state_, preview_controller);
    data_controller_.request_ranking_reload(state_);
}

void title_play_create_feature::sync_create_preview(song_select::preview_controller& preview_controller) {
    title_play_session::sync_preview(state_, preview_controller);
}
