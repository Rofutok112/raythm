#include "title/title_play_create_feature.h"

#include <chrono>
#include <ctime>
#include <optional>
#include <thread>
#include <utility>

#include "network/auth_client.h"
#include "network/server_environment.h"
#include "services/content_authorization_service.h"
#include "title/local_content_database.h"

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

std::optional<std::string> current_user_id_for_server(const std::string& server_url) {
    const std::optional<auth::session> session = auth::load_saved_session();
    if (!session.has_value() ||
        auth::normalize_server_url(session->server_url) != server_url ||
        session->user.id.empty()) {
        return std::nullopt;
    }
    return session->user.id;
}

long long now_unix_seconds() {
    return static_cast<long long>(std::time(nullptr));
}

content_authorization_service::content_type permission_type_for(
    local_content_database::remote_content_type type) {
    return type == local_content_database::remote_content_type::chart
        ? content_authorization_service::content_type::chart
        : content_authorization_service::content_type::song;
}

std::optional<bool> usable_cached_permission_hint(
    const local_content_database::account_permission& permission,
    local_content_database::remote_content_type type,
    const std::string& server_url,
    const std::string& remote_id,
    const std::string& user_id) {
    const content_authorization_service::permission_entry entry{
        .key = {
            .server_url = permission.server_url,
            .type = permission_type_for(permission.type),
            .remote_id = permission.remote_id,
            .user_id = permission.user_id,
        },
        .can_edit = permission.can_edit,
        .fetched_at_unix_seconds = permission.fetched_at_unix_seconds,
    };
    const content_authorization_service::permission_key current{
        .server_url = server_url,
        .type = permission_type_for(type),
        .remote_id = remote_id,
        .user_id = user_id,
    };
    return content_authorization_service::can_use_cached_permission(entry, current, now_unix_seconds())
        ? permission.can_edit
        : std::nullopt;
}

std::optional<bool> song_permission_hint_for(const std::string& server_url,
                                             const std::optional<std::string>& user_id,
                                             const title_create_tools_model::bindings& bindings) {
    if (!user_id.has_value() || !bindings.song.has_value() || bindings.song->remote_song_id.empty()) {
        return std::nullopt;
    }
    const auto permission = local_content_database::find_account_permission(
        local_content_database::remote_content_type::song,
        server_url,
        bindings.song->remote_song_id,
        *user_id);
    return permission.has_value()
        ? usable_cached_permission_hint(*permission,
                                        local_content_database::remote_content_type::song,
                                        server_url,
                                        bindings.song->remote_song_id,
                                        *user_id)
        : std::nullopt;
}

std::optional<bool> chart_permission_hint_for(const std::string& server_url,
                                              const std::optional<std::string>& user_id,
                                              const title_create_tools_model::bindings& bindings) {
    if (!user_id.has_value() || !bindings.chart.has_value() || bindings.chart->remote_chart_id.empty()) {
        return std::nullopt;
    }
    const auto permission = local_content_database::find_account_permission(
        local_content_database::remote_content_type::chart,
        server_url,
        bindings.chart->remote_chart_id,
        *user_id);
    return permission.has_value()
        ? usable_cached_permission_hint(*permission,
                                        local_content_database::remote_content_type::chart,
                                        server_url,
                                        bindings.chart->remote_chart_id,
                                        *user_id)
        : std::nullopt;
}

std::string create_permission_refresh_key(const std::string& server_url,
                                          const std::string& user_id,
                                          const title_create_tools_model::bindings& bindings) {
    const std::string remote_song_id =
        bindings.song.has_value() ? bindings.song->remote_song_id : "";
    const std::string remote_chart_id =
        bindings.chart.has_value() ? bindings.chart->remote_chart_id : "";
    return server_url + "\n" + user_id + "\n" + remote_song_id + "\n" + remote_chart_id;
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
    media_coordinator_.reset();
    create_tools_model_ = {};
    create_tools_bindings_ = {};
    create_tools_binding_cache_valid_ = false;
    create_permission_refresh_in_progress_ = false;
    create_permission_refresh_key_.clear();
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
    state_.filter.include_chartless_songs = true;
    capture_current_selection();
    refresh_create_tools_model(true);
    sync_selection_media(audio_controller, media_context::create, true);
}

void title_play_create_feature::request_catalog_reload(std::string preferred_song_id,
                                                       std::string preferred_chart_id,
                                                       title_catalog::reload_policy policy) {
    data_controller_.request_catalog_reload(state_, std::move(preferred_song_id),
                                            std::move(preferred_chart_id),
                                            policy.calculate_missing_levels);
}

void title_play_create_feature::poll_catalog_reload(title_audio_controller& audio_controller,
                                                    bool play_mode_active,
                                                    bool create_mode_active) {
    const title_play_data_controller::catalog_poll_result result =
        data_controller_.poll_catalog_reload(state_);
    if (!result.completed) {
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
    media_coordinator_.request_ranking_reload(state_, data_controller_);
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

song_select::ranking_load_controller::load_status title_play_create_feature::ranking_status() const {
    return data_controller_.ranking_status();
}

void title_play_create_feature::poll_transfer(const cross_callbacks& callbacks, bool sync_media_on_reload) {
    transfer_controller_.poll(state_, make_transfer_callbacks(callbacks), sync_media_on_reload);
}

bool title_play_create_feature::poll_create_upload(bool sync_media_on_apply) {
    if (!data_controller_.poll_create_upload(state_).refresh_catalog) {
        return false;
    }
    capture_current_selection();
    request_catalog_reload(preferred_song_id_,
                           preferred_chart_id_,
                           title_catalog::policy_for(title_catalog::reload_mode::import_completed,
                                                     sync_media_on_apply));
    return true;
}

void title_play_create_feature::cancel_confirmation() {
    transfer_controller_.cancel_confirmation(state_);
}

void title_play_create_feature::draw_or_apply_confirmation(title_audio_controller& audio_controller,
                                                           const cross_callbacks& callbacks,
                                                           bool sync_media_on_reload) {
    transfer_controller_.draw_or_apply_confirmation(
        state_, audio_controller, make_transfer_callbacks(callbacks), sync_media_on_reload);
}

void title_play_create_feature::update_play(scene_manager& manager,
                                            title_audio_controller& audio_controller,
                                            float anim_t,
                                            Rectangle origin_rect,
                                            float dt,
                                            const play_update_callbacks& callbacks) {
    state_.filter.include_chartless_songs = false;
    title_play_mode_controller::update(
        manager,
        state_,
        audio_controller,
        transfer_controller_,
        anim_t,
        origin_rect,
        dt,
        {
            .enter_home = callbacks.enter_home,
            .sync_media = [this, &audio_controller]() {
                sync_selection_media(audio_controller, media_context::play);
            },
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
                                              title_audio_controller& audio_controller,
                                              float anim_t,
                                              Rectangle origin_rect,
                                              float dt,
                                              const cross_callbacks& cross,
                                              const create_update_callbacks& callbacks) {
    state_.filter.include_chartless_songs = true;
    poll_create_permission_refresh();
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
            .sync_preview = [this, &audio_controller]() {
                sync_selection_media(audio_controller, media_context::create);
            },
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
                request_catalog_reload(
                    song_id,
                    chart_id,
                    title_catalog::policy_for(title_catalog::reload_mode::transfer_completed,
                                              sync_media_on_apply));
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

    const std::optional<std::string> current_user_id = current_user_id_for_server(server_url);
    const std::optional<bool> song_permission_hint =
        song_permission_hint_for(server_url, current_user_id, create_tools_bindings_);
    const std::optional<bool> chart_permission_hint =
        chart_permission_hint_for(server_url, current_user_id, create_tools_bindings_);
    create_tools_model_ = title_create_tools_model::build({
        .song = song,
        .chart = chart,
        .server_url = server_url,
        .online_status_checking = state_.catalog_loading,
        .upload_bindings = create_tools_bindings_,
        .song_permission_hint = song_permission_hint,
        .chart_permission_hint = chart_permission_hint,
    });
    request_create_permission_refresh(server_url,
                                      current_user_id,
                                      create_tools_bindings_,
                                      song_permission_hint,
                                      chart_permission_hint);
}

void title_play_create_feature::request_create_permission_refresh(
    const std::string& server_url,
    const std::optional<std::string>& current_user_id,
    const title_create_tools_model::bindings& bindings,
    const std::optional<bool>& song_permission_hint,
    const std::optional<bool>& chart_permission_hint) {
    if (server_url.empty() || !current_user_id.has_value() || create_permission_refresh_in_progress_) {
        return;
    }
    const bool refresh_song =
        bindings.song.has_value() &&
        !bindings.song->remote_song_id.empty() &&
        !song_permission_hint.has_value();
    const bool refresh_chart =
        bindings.chart.has_value() &&
        !bindings.chart->remote_chart_id.empty() &&
        !chart_permission_hint.has_value();
    if (!refresh_song && !refresh_chart) {
        return;
    }

    const std::string key = create_permission_refresh_key(server_url, *current_user_id, bindings);
    if (key.empty() || key == create_permission_refresh_key_) {
        return;
    }
    create_permission_refresh_key_ = key;
    create_permission_refresh_in_progress_ = true;

    const std::string remote_song_id = refresh_song ? bindings.song->remote_song_id : "";
    const std::string remote_chart_id = refresh_chart ? bindings.chart->remote_chart_id : "";
    create_permission_refresh_future_ = std::async(std::launch::async, [remote_song_id, remote_chart_id]() {
        bool refreshed = false;
        if (!remote_song_id.empty()) {
            refreshed = title_create_upload::refresh_song_edit_permission(remote_song_id) || refreshed;
        }
        if (!remote_chart_id.empty()) {
            refreshed = title_create_upload::refresh_chart_edit_permission(remote_chart_id) || refreshed;
        }
        return refreshed;
    });
}

void title_play_create_feature::poll_create_permission_refresh() {
    if (!create_permission_refresh_in_progress_) {
        return;
    }
    if (create_permission_refresh_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }
    bool refreshed = false;
    try {
        refreshed = create_permission_refresh_future_.get();
    } catch (...) {
    }
    create_permission_refresh_in_progress_ = false;
    if (refreshed) {
        refresh_create_tools_model(true);
    }
}

void title_play_create_feature::sync_selection_media(
    title_audio_controller& audio_controller,
    title_selection_media_coordinator::context active_context,
    bool force) {
    media_coordinator_.sync_current(state_, audio_controller, data_controller_, active_context, force);
}
