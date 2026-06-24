#include "title/title_play_create_feature.h"

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "song_select/song_select_confirmation_dialog.h"

namespace {

using media_context = title_selection_media_coordinator::context;

media_context media_context_for(bool play_mode_active, bool create_mode_active) {
    if (play_mode_active) {
        return media_context::play;
    }
    if (create_mode_active) {
        return media_context::create;
    }
    return media_context::none;
}

void apply_create_catalog_filter(song_select::state& state) {
    state.filter.include_chartless_songs = true;
    state.filter.multiplayer_queueable_only = false;
    state.filter.multiplayer_queue_server_url.clear();
    state.play_mod_modal_open = false;
    state.chart_level_filter_dragging = false;
    state.chart_level_filter_dragging_min = false;
}

}  // namespace

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

const title_create_tools_model::view_model& title_play_create_feature::create_tools_model() const {
    return create_tools_model_;
}

void title_play_create_feature::reset() {
    song_select::reset_for_enter(state_);
    data_controller_.reset(state_);
    media_coordinator_.reset(state_);
    create_tools_model_ = {};
    create_tools_bindings_ = {};
    create_tools_binding_cache_valid_ = false;
    create_tools_binding_server_url_.clear();
    create_tools_binding_song_id_.clear();
    create_tools_binding_chart_id_.clear();
}

void title_play_create_feature::on_exit() {
    transfer_controller_.on_exit();
}

void title_play_create_feature::on_enter_play(bool multiplayer_chart_pick_active,
                                              const std::string& multiplayer_server_url,
                                              title_audio_controller& audio_controller) {
    state_.filter.include_chartless_songs = false;
    state_.filter.multiplayer_queueable_only = multiplayer_chart_pick_active;
    state_.filter.multiplayer_queue_server_url = multiplayer_chart_pick_active ? multiplayer_server_url : "";
    sync_selection_media(audio_controller, media_context::play, true);
}

void title_play_create_feature::on_enter_create(title_audio_controller& audio_controller) {
    apply_create_catalog_filter(state_);
    capture_current_selection();
    refresh_create_tools_model(true);
    sync_selection_media(audio_controller, media_context::create, true);
}

void title_play_create_feature::request_catalog_reload(std::string preferred_song_id,
                                                       std::string preferred_chart_id,
                                                       title_catalog::reload_policy policy) {
    data_controller_.request_catalog_reload(state_, std::move(preferred_song_id),
                                            std::move(preferred_chart_id),
                                            policy.calculate_missing_levels,
                                            policy.preserve_current_selection);
}

void title_play_create_feature::poll_catalog_reload(title_audio_controller& audio_controller,
                                                    bool play_mode_active,
                                                    bool create_mode_active) {
    const title_play_data_controller::catalog_poll_result result =
        data_controller_.poll_catalog_reload(state_);
    if (!result.completed) {
        return;
    }
    if (result.stale) {
        return;
    }

    if (create_mode_active) {
        refresh_create_tools_model(true);
    }

    if (result.selection_changed) {
        sync_selection_media(audio_controller, media_context_for(play_mode_active, create_mode_active), true);
    }
}

void title_play_create_feature::request_ranking_reload() {
    media_coordinator_.request_ranking_reload(state_);
}

void title_play_create_feature::poll_ranking_reload() {
    media_coordinator_.poll_ranking_reload(state_);
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

load_progress title_play_create_feature::catalog_progress() const {
    return data_controller_.catalog_progress();
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

title_selection_media_snapshot title_play_create_feature::media_snapshot(
    const title_audio_controller& audio_controller) const {
    return media_coordinator_.media_snapshot(state_, audio_controller);
}

void title_play_create_feature::poll_transfer(const cross_callbacks& callbacks) {
    transfer_controller_.poll(state_, make_transfer_callbacks(callbacks));
}

bool title_play_create_feature::poll_create_upload() {
    const title_play_data_controller::upload_poll_result upload = data_controller_.poll_create_upload(state_);
    if (!upload.refresh_catalog) {
        return false;
    }
    capture_current_selection();
    request_catalog_reload(preferred_song_id_,
                           preferred_chart_id_,
                           title_catalog::policy_for(title_catalog::reload_mode::import_completed));
    return true;
}

void title_play_create_feature::cancel_confirmation() {
    transfer_controller_.cancel_confirmation(state_);
}

void title_play_create_feature::draw_or_apply_confirmation(title_audio_controller& audio_controller,
                                                           const cross_callbacks& callbacks) {
    if (state_.confirmation_dialog.open &&
        (state_.confirmation_dialog.action == song_select::pending_confirmation_action::upload_song ||
         state_.confirmation_dialog.action == song_select::pending_confirmation_action::upload_chart)) {
        draw_or_apply_upload_confirmation();
        return;
    }
    transfer_controller_.draw_or_apply_confirmation(
        state_, audio_controller, make_transfer_callbacks(callbacks));
}

title_play_create_feature::play_update_result title_play_create_feature::update_play(
                                            scene_manager& manager,
                                            title_audio_controller& audio_controller,
                                            float anim_t,
                                            Rectangle origin_rect,
                                            float dt) {
    state_.filter.include_chartless_songs = false;
    const title_selection_media_snapshot media = media_snapshot(audio_controller);
    const title_play_mode_controller::update_result result = title_play_mode_controller::update(
        manager,
        state_,
        audio_controller,
        transfer_controller_,
        media,
        anim_t,
        origin_rect,
        dt,
        {
            .sync_media = [this, &audio_controller]() {
                sync_selection_media(audio_controller, media_context::play);
            },
            .request_ranking_reload = [this]() { request_ranking_reload(); },
        });
    if (result.title_command.type == title::command_type::open_update_catalog) {
        const song_select::song_entry* song = song_select::selected_song(state_);
        if (song == nullptr) {
            return {};
        }
        std::string chart_id;
        const auto filtered = song_select::filtered_charts_for_selected_song(state_);
        const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
        if (result.update_catalog_include_chart && chart != nullptr) {
            chart_id = chart->meta.chart_id;
        }
        return {
            .title_command = title::command::open_update_catalog(song->song.meta.song_id, chart_id),
        };
    }
    return {
        .title_command = result.title_command,
    };
}

void title_play_create_feature::update_create(scene_manager& manager,
                                              title_audio_controller& audio_controller,
                                              float anim_t,
                                              Rectangle origin_rect,
                                              float dt,
                                              const cross_callbacks& cross,
                                              const create_update_callbacks& callbacks) {
    apply_create_catalog_filter(state_);
    refresh_create_tools_model();
    title_create_mode_controller::update(
        manager,
        state_,
        transfer_controller_,
        anim_t,
        origin_rect,
        dt,
        create_tools_model_,
        {
            .enter_home = callbacks.enter_home,
            .sync_media = [this, &audio_controller]() {
                sync_selection_media(audio_controller, media_context::create);
            },
            .transfer_callbacks = [this, &cross]() {
                return make_transfer_callbacks(cross);
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
            [this](const std::string& song_id, const std::string& chart_id) {
                request_catalog_reload(
                    song_id,
                    chart_id,
                    title_catalog::policy_for(title_catalog::reload_mode::transfer_completed));
            },
    };
}

void title_play_create_feature::refresh_create_tools_model(bool force_bindings) {
    const song_select::song_entry* song = song_select::selected_song(state_);
    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
    const std::string server_url = server_environment::normalize_url(state_.auth.server_url);
    const std::string song_id = song != nullptr ? song->song.meta.song_id : "";
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    const bool binding_target_changed =
        force_bindings ||
        !create_tools_binding_cache_valid_ ||
        create_tools_binding_server_url_ != server_url ||
        create_tools_binding_song_id_ != song_id ||
        create_tools_binding_chart_id_ != chart_id;

    if (binding_target_changed) {
        create_tools_bindings_ = {};
        if (!server_url.empty() && (song != nullptr || chart != nullptr)) {
            const local_content_index::snapshot index = local_content_index::load_snapshot();
            create_tools_bindings_ = title_create_tools_model::resolve_bindings(index, song, chart, server_url);
        }
        create_tools_binding_cache_valid_ = true;
        create_tools_binding_server_url_ = server_url;
        create_tools_binding_song_id_ = song_id;
        create_tools_binding_chart_id_ = chart_id;
    }

    create_tools_model_ = title_create_tools_model::build({
        .song = song,
        .chart = chart,
        .server_url = server_url,
        .online_status_checking = state_.catalog_loading,
        .upload_bindings = create_tools_bindings_,
    });
}

void title_play_create_feature::sync_selection_media(
    title_audio_controller& audio_controller,
    title_selection_media_coordinator::context active_context,
    bool force) {
    media_coordinator_.sync_current(state_, audio_controller, active_context, force);
}

void title_play_create_feature::draw_or_apply_upload_confirmation() {
    const song_select::pending_confirmation_action action = state_.confirmation_dialog.action;
    const song_select::confirmation_command command = song_select::draw_confirmation_dialog(state_);
    if (command == song_select::confirmation_command::none) {
        return;
    }
    if (command == song_select::confirmation_command::cancel) {
        state_.confirmation_dialog = {};
        return;
    }

    state_.confirmation_dialog = {};
    refresh_create_tools_model(true);
    const song_select::song_entry* song = song_select::selected_song(state_);
    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
    if (action == song_select::pending_confirmation_action::upload_song) {
        if (song == nullptr) {
            song_select::queue_status_message(state_, "Select a song to upload.", true);
        } else if (!create_tools_model_.song_upload_enabled) {
            song_select::queue_status_message(state_, "This song cannot be submitted right now.", true);
        } else {
            data_controller_.start_song_upload(*song);
        }
        return;
    }
    if (action == song_select::pending_confirmation_action::upload_chart) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(state_, "Select a chart to upload.", true);
        } else if (!create_tools_model_.chart_upload_enabled) {
            song_select::queue_status_message(state_, "Upload the song before submitting this chart.", true);
        } else {
            data_controller_.start_chart_upload(*song, *chart);
        }
    }
}
