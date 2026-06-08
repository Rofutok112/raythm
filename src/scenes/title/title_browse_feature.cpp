#include "title/title_browse_feature.h"

title_online_view::state& title_browse_feature::state() {
    return state_;
}

const title_online_view::state& title_browse_feature::state() const {
    return state_;
}

void title_browse_feature::on_enter(title_audio_controller& audio_controller) {
    title_online_view::on_enter(state_, data_controller_, audio_controller);
}

void title_browse_feature::on_exit() {
    title_online_view::on_exit(state_);
}

void title_browse_feature::request_reload(bool preserve_view) {
    title_online_view::reload_catalog(state_, data_controller_, preserve_view);
}

void title_browse_feature::select_local_update_target(const std::string& local_song_id,
                                                      const std::string& local_chart_id,
                                                      bool open_detail) {
    title_online_view::select_local_update_target(
        state_, data_controller_, local_song_id, local_chart_id, open_detail);
}

void title_browse_feature::mark_song_removed(const std::string& song_id) {
    title_online_view::mark_song_removed(state_, song_id);
}

void title_browse_feature::start_chart_download_by_remote_id(const std::string& remote_song_id,
                                                            const std::string& remote_chart_id) {
    title_online_view::start_chart_download_by_remote_id(
        state_, data_controller_, remote_song_id, remote_chart_id);
}

const song_select::song_entry* title_browse_feature::preview_song() const {
    return title_online_view::preview_song(state_);
}

std::string title_browse_feature::selected_song_id() const {
    return title_online_view::selected_song_id(state_);
}

void title_browse_feature::update(float anim_t,
                                  Rectangle origin_rect,
                                  float dt,
                                  title_audio_controller& audio_controller,
                                  const update_callbacks& callbacks) {
    title_online_mode_controller::update(
        state_, data_controller_, audio_controller, anim_t, origin_rect, dt, callbacks.online);
}

title_browse_feature::poll_result title_browse_feature::poll(bool active) {
    poll_result result;
    title_online_view::poll_song_page(state_, data_controller_);
    title_online_view::poll_chart_page(state_, data_controller_);
    title_online_view::poll_owned(state_, data_controller_);
    result.downloaded_content = title_online_view::poll_download(state_, data_controller_);
    if (result.downloaded_content) {
        result.downloaded_song_id = title_online_view::selected_song_id(state_);
    }
    result.catalog_refreshed = title_online_view::poll_catalog(state_, data_controller_);
    result.select_preview_song = active && result.catalog_refreshed;
    return result;
}

void title_browse_feature::draw(const title_audio_controller& audio_controller, float anim_t, Rectangle origin_rect) {
    title_online_view::draw(state_, audio_controller, anim_t, origin_rect);
}
