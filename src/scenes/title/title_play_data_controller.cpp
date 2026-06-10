#include "title/title_play_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "song_select/song_select_navigation.h"
#include "ui_notice.h"

void title_play_data_controller::reset(song_select::state& state) {
    data_controller_.reset(state);
    catalog_sync_media_on_apply_ = false;
    queued_catalog_sync_media_on_apply_ = false;

    scoring_ruleset_loading_ = false;
    upload_in_progress_ = false;
}

bool title_play_data_controller::catalog_loading() const {
    return data_controller_.catalog_loading();
}

load_progress title_play_data_controller::catalog_progress() const {
    return data_controller_.catalog_progress();
}

bool title_play_data_controller::scoring_ruleset_loading() const {
    return scoring_ruleset_loading_;
}

bool title_play_data_controller::upload_in_progress() const {
    return upload_in_progress_;
}

void title_play_data_controller::request_catalog_reload(song_select::state& state,
                                                        std::string preferred_song_id,
                                                        std::string preferred_chart_id,
                                                        title_catalog::reload_policy policy) {
    if (data_controller_.catalog_loading()) {
        queued_catalog_sync_media_on_apply_ =
            queued_catalog_sync_media_on_apply_ || policy.sync_media_on_apply;
    } else {
        catalog_sync_media_on_apply_ = policy.sync_media_on_apply;
    }

    data_controller_.request_catalog_reload(
        state,
        song_select::catalog_reload_request{std::move(preferred_song_id),
                                            std::move(preferred_chart_id),
                                            policy.calculate_missing_levels});
}

title_play_data_controller::catalog_poll_result title_play_data_controller::poll_catalog_reload(
    song_select::state& state) {
    catalog_poll_result result;
    const song_select::catalog_reload_result reload = data_controller_.poll_catalog_reload(state);
    if (!reload.completed) {
        return result;
    }

    result.completed = true;
    result.sync_selection_media = catalog_sync_media_on_apply_ && reload.selection_changed;
    catalog_sync_media_on_apply_ = false;

    if (reload.queued_reload_started) {
        catalog_sync_media_on_apply_ = queued_catalog_sync_media_on_apply_;
        queued_catalog_sync_media_on_apply_ = false;
    }

    return result;
}

void title_play_data_controller::request_ranking_reload(song_select::state& state) {
    data_controller_.request_ranking_reload(state);
}

void title_play_data_controller::poll_ranking_reload(song_select::state& state) {
    data_controller_.poll_ranking_reload(state);
}

void title_play_data_controller::request_scoring_ruleset_warm(bool force_refresh) {
    if (scoring_ruleset_loading_) {
        return;
    }

    scoring_ruleset_loading_ = true;
    std::promise<bool> promise;
    scoring_ruleset_future_ = promise.get_future();
    std::thread([promise = std::move(promise), force_refresh]() mutable {
        try {
            promise.set_value(ranking_service::warm_scoring_ruleset_cache(force_refresh));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool title_play_data_controller::poll_scoring_ruleset_warm() {
    if (!scoring_ruleset_loading_) {
        return false;
    }
    if (scoring_ruleset_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    bool warmed = false;
    try {
        warmed = scoring_ruleset_future_.get();
    } catch (...) {
    }
    scoring_ruleset_loading_ = false;
    return warmed;
}

void title_play_data_controller::start_song_upload(const song_select::song_entry& song) {
    if (upload_in_progress_) {
        ui::notify("Upload already in progress.", ui::notice_tone::info, 1.8f);
        return;
    }

    upload_in_progress_ = true;
    ui::notify("Uploading song...", ui::notice_tone::info, 1.8f);
    std::promise<title_create_upload::upload_result> promise;
    upload_future_ = promise.get_future();
    std::thread([promise = std::move(promise), song]() mutable {
        try {
            promise.set_value(title_create_upload::upload_song(song));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_play_data_controller::start_chart_upload(const song_select::song_entry& song,
                                                    const song_select::chart_option& chart) {
    if (upload_in_progress_) {
        ui::notify("Upload already in progress.", ui::notice_tone::info, 1.8f);
        return;
    }

    upload_in_progress_ = true;
    ui::notify("Uploading chart...", ui::notice_tone::info, 1.8f);
    std::promise<title_create_upload::upload_result> promise;
    upload_future_ = promise.get_future();
    std::thread([promise = std::move(promise), song, chart]() mutable {
        try {
            promise.set_value(title_create_upload::upload_chart(song, chart));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

title_play_data_controller::upload_poll_result title_play_data_controller::poll_create_upload(
    song_select::state& state) {
    upload_poll_result poll_result;
    if (!upload_in_progress_) {
        return poll_result;
    }
    if (upload_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return poll_result;
    }

    title_create_upload::upload_result result;
    try {
        result = upload_future_.get();
    } catch (const std::exception& ex) {
        result.success = false;
        result.message = ex.what();
    } catch (...) {
        result.success = false;
        result.message = "Upload failed.";
    }
    upload_in_progress_ = false;
    song_select::queue_status_message(state, result.message, !result.success);
    poll_result.refresh_catalog = result.success && result.should_refresh_online_catalog;
    return poll_result;
}

void title_play_data_controller::start_catalog_load(song_select::state& state,
                                                    std::string preferred_song_id,
                                                    std::string preferred_chart_id,
                                                    title_catalog::reload_policy policy) {
    request_catalog_reload(state,
                           std::move(preferred_song_id),
                           std::move(preferred_chart_id),
                           policy);
}
