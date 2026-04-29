#include "title/title_play_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "song_select/song_select_navigation.h"
#include "ui_notice.h"

void title_play_data_controller::reset(song_select::state& state) {
    catalog_loading_ = false;
    catalog_reload_pending_ = false;
    catalog_sync_media_on_apply_ = false;
    queued_catalog_sync_media_on_apply_ = false;
    catalog_calculate_missing_levels_ = false;
    queued_catalog_calculate_missing_levels_ = false;
    catalog_song_id_.clear();
    catalog_chart_id_.clear();
    queued_catalog_song_id_.clear();
    queued_catalog_chart_id_.clear();

    ranking_loading_ = false;
    ranking_reload_pending_ = false;
    ranking_generation_ = 0;
    ranking_pending_generation_ = 0;
    state.ranking_panel.selected_source = ranking_service::source::local;

    scoring_ruleset_loading_ = false;
    upload_in_progress_ = false;
}

bool title_play_data_controller::upload_in_progress() const {
    return upload_in_progress_;
}

void title_play_data_controller::request_catalog_reload(song_select::state& state,
                                                        std::string preferred_song_id,
                                                        std::string preferred_chart_id,
                                                        bool sync_media_on_apply,
                                                        bool calculate_missing_levels) {
    if (catalog_loading_) {
        catalog_reload_pending_ = true;
        queued_catalog_song_id_ = std::move(preferred_song_id);
        queued_catalog_chart_id_ = std::move(preferred_chart_id);
        queued_catalog_sync_media_on_apply_ =
            queued_catalog_sync_media_on_apply_ || sync_media_on_apply;
        queued_catalog_calculate_missing_levels_ =
            queued_catalog_calculate_missing_levels_ || calculate_missing_levels;
        state.catalog_loading = true;
        return;
    }

    start_catalog_load(state, std::move(preferred_song_id), std::move(preferred_chart_id),
                       sync_media_on_apply, calculate_missing_levels);
}

title_play_data_controller::catalog_poll_result title_play_data_controller::poll_catalog_reload(
    song_select::state& state,
    bool play_mode_active,
    bool create_mode_active) {
    catalog_poll_result result;
    if (!catalog_loading_) {
        return result;
    }
    if (catalog_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    try {
        song_select::apply_catalog(state, catalog_future_.get(), catalog_song_id_, catalog_chart_id_);
    } catch (const std::exception& ex) {
        song_select::apply_catalog(state, {}, catalog_song_id_, catalog_chart_id_);
        state.load_errors = {ex.what()};
    }
    catalog_loading_ = false;
    result.sync_play_media = catalog_sync_media_on_apply_ || play_mode_active;
    result.sync_create_preview = !result.sync_play_media && create_mode_active;
    catalog_sync_media_on_apply_ = false;
    catalog_calculate_missing_levels_ = false;

    if (catalog_reload_pending_) {
        catalog_reload_pending_ = false;
        start_catalog_load(state, queued_catalog_song_id_, queued_catalog_chart_id_,
                           queued_catalog_sync_media_on_apply_,
                           queued_catalog_calculate_missing_levels_);
        queued_catalog_song_id_.clear();
        queued_catalog_chart_id_.clear();
        queued_catalog_sync_media_on_apply_ = false;
        queued_catalog_calculate_missing_levels_ = false;
    }

    return result;
}

void title_play_data_controller::request_ranking_reload(song_select::state& state) {
    if (ranking_loading_) {
        ++ranking_generation_;
        ranking_reload_pending_ = true;
        if (state.ranking_panel.selected_source == ranking_service::source::online) {
            state.ranking_panel.listing = {};
            state.ranking_panel.listing.ranking_source = state.ranking_panel.selected_source;
            state.ranking_panel.listing.available = false;
            state.ranking_panel.listing.message = "Loading online rankings...";
        }
        return;
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    const ranking_service::source source = state.ranking_panel.selected_source;

    ++ranking_generation_;
    ranking_pending_generation_ = ranking_generation_;
    state.ranking_panel.source_dropdown_open = false;
    state.ranking_panel.scroll_y = 0.0f;
    state.ranking_panel.scroll_y_target = 0.0f;
    state.ranking_panel.scrollbar_dragging = false;
    state.ranking_panel.scrollbar_drag_offset = 0.0f;

    if (source == ranking_service::source::online) {
        state.ranking_panel.listing = {};
        state.ranking_panel.listing.ranking_source = source;
        state.ranking_panel.listing.available = false;
        state.ranking_panel.listing.message = "Loading online rankings...";
    }

    start_ranking_load(state, chart_id, source);
}

void title_play_data_controller::poll_ranking_reload(song_select::state& state) {
    if (!ranking_loading_) {
        return;
    }
    if (ranking_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    ranking_service::listing listing;
    try {
        listing = ranking_future_.get();
    } catch (const std::exception& ex) {
        listing.available = false;
        listing.message = ex.what();
        listing.ranking_source = state.ranking_panel.selected_source;
    }
    ranking_loading_ = false;
    const bool stale = ranking_pending_generation_ != ranking_generation_;
    if (!stale) {
        state.ranking_panel.listing = std::move(listing);
        state.ranking_panel.reveal_anim = 0.0f;
    }

    if (ranking_reload_pending_) {
        ranking_reload_pending_ = false;
        request_ranking_reload(state);
    }
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

void title_play_data_controller::poll_scoring_ruleset_warm() {
    if (!scoring_ruleset_loading_) {
        return;
    }
    if (scoring_ruleset_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    try {
        scoring_ruleset_future_.get();
    } catch (...) {
    }
    scoring_ruleset_loading_ = false;
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
                                                    bool sync_media_on_apply,
                                                    bool calculate_missing_levels) {
    catalog_song_id_ = std::move(preferred_song_id);
    catalog_chart_id_ = std::move(preferred_chart_id);
    catalog_sync_media_on_apply_ = sync_media_on_apply;
    catalog_calculate_missing_levels_ = calculate_missing_levels;
    catalog_loading_ = true;
    state.catalog_loading = true;
    std::promise<song_select::catalog_data> promise;
    catalog_future_ = promise.get_future();
    std::thread([promise = std::move(promise), calculate_missing_levels]() mutable {
        try {
            promise.set_value(song_select::load_catalog(calculate_missing_levels));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_play_data_controller::start_ranking_load(song_select::state&,
                                                    std::string chart_id,
                                                    ranking_service::source source) {
    ranking_loading_ = true;
    std::promise<ranking_service::listing> promise;
    ranking_future_ = promise.get_future();
    std::thread([promise = std::move(promise), chart_id = std::move(chart_id), source]() mutable {
        try {
            promise.set_value(ranking_service::load_chart_ranking(chart_id, source, 50));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}
