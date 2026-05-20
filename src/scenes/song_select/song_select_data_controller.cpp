#include "song_select/song_select_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace song_select {

void data_controller::reset(state& state) {
    catalog_loading_ = false;
    catalog_reload_pending_ = false;
    active_catalog_request_ = {};
    queued_catalog_request_ = {};

    ranking_loading_ = false;
    ranking_reload_pending_ = false;
    ranking_generation_ = 0;
    ranking_pending_generation_ = 0;
    state.ranking_panel.selected_source = ranking_service::source::local;
}

bool data_controller::catalog_loading() const {
    return catalog_loading_;
}

bool data_controller::ranking_loading() const {
    return ranking_loading_;
}

void data_controller::request_catalog_reload(state& state) {
    request_catalog_reload(state, {});
}

void data_controller::request_catalog_reload(state& state, catalog_reload_request request) {
    if (catalog_loading_) {
        catalog_reload_pending_ = true;
        queued_catalog_request_.preferred_song_id = std::move(request.preferred_song_id);
        queued_catalog_request_.preferred_chart_id = std::move(request.preferred_chart_id);
        queued_catalog_request_.sync_media_on_apply =
            queued_catalog_request_.sync_media_on_apply || request.sync_media_on_apply;
        queued_catalog_request_.calculate_missing_levels =
            queued_catalog_request_.calculate_missing_levels || request.calculate_missing_levels;
        state.catalog.loading = true;
        return;
    }

    start_catalog_load(state, std::move(request));
}

data_controller::catalog_poll_result data_controller::poll_catalog_reload(state& state) {
    catalog_poll_result result;
    if (!catalog_loading_) {
        return result;
    }
    if (catalog_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    try {
        apply_catalog(state, catalog_future_.get(),
                      active_catalog_request_.preferred_song_id,
                      active_catalog_request_.preferred_chart_id);
    } catch (const std::exception& ex) {
        apply_catalog(state, {},
                      active_catalog_request_.preferred_song_id,
                      active_catalog_request_.preferred_chart_id);
        state.catalog.load_errors = {ex.what()};
    }

    catalog_loading_ = false;
    result.applied = true;
    result.sync_media = active_catalog_request_.sync_media_on_apply;
    active_catalog_request_ = {};

    if (catalog_reload_pending_) {
        catalog_reload_pending_ = false;
        start_catalog_load(state, std::move(queued_catalog_request_));
        queued_catalog_request_ = {};
    }

    return result;
}

void data_controller::request_ranking_reload(state& state) {
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

    const auto filtered = filtered_charts_for_selected_song(state);
    const chart_option* chart = selected_chart_for(state, filtered);
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

    start_ranking_load(chart_id, source);
}

data_controller::ranking_poll_result data_controller::poll_ranking_reload(state& state) {
    ranking_poll_result result;
    if (!ranking_loading_) {
        return result;
    }
    if (ranking_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
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
    result.stale = ranking_pending_generation_ != ranking_generation_;
    if (!result.stale) {
        state.ranking_panel.listing = std::move(listing);
        state.ranking_panel.reveal_anim = 0.0f;
        result.applied = true;
    }

    if (ranking_reload_pending_) {
        ranking_reload_pending_ = false;
        request_ranking_reload(state);
    }

    return result;
}

void data_controller::start_catalog_load(state& state, catalog_reload_request request) {
    active_catalog_request_ = std::move(request);
    catalog_loading_ = true;
    state.catalog.loading = true;
    std::promise<catalog_data> promise;
    catalog_future_ = promise.get_future();
    std::thread([promise = std::move(promise),
                 calculate_missing_levels = active_catalog_request_.calculate_missing_levels]() mutable {
        try {
            promise.set_value(load_catalog(calculate_missing_levels));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void data_controller::start_ranking_load(std::string chart_id, ranking_service::source source) {
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

}  // namespace song_select
