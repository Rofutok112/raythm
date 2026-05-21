#include "song_select/song_select_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace song_select {

data_controller::data_controller()
    : data_controller(load_catalog_from_service, load_ranking_from_service) {
}

data_controller::data_controller(catalog_loader catalog_loader_fn, ranking_loader ranking_loader_fn)
    : catalog_loader_(std::move(catalog_loader_fn)),
      ranking_loader_(std::move(ranking_loader_fn)) {
}

catalog_data data_controller::load_catalog_from_service(bool calculate_missing_levels) {
    return load_catalog(calculate_missing_levels);
}

ranking_service::listing data_controller::load_ranking_from_service(std::string chart_id,
                                                                    ranking_service::source source,
                                                                    int limit) {
    return ranking_service::load_chart_ranking(std::move(chart_id), source, limit);
}

void data_controller::reset(state& state) {
    catalog_loading_ = false;
    catalog_reload_pending_ = false;
    active_catalog_request_ = {};
    queued_catalog_request_ = {};
    state.catalog_loading = false;

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

void data_controller::request_catalog_reload(state& state, catalog_reload_request request) {
    if (catalog_loading_) {
        catalog_reload_pending_ = true;
        queued_catalog_request_ = std::move(request);
        state.catalog_loading = true;
        return;
    }

    start_catalog_load(state, std::move(request));
}

catalog_reload_result data_controller::poll_catalog_reload(state& state) {
    catalog_reload_result result;
    if (!catalog_loading_) {
        return result;
    }
    if (catalog_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    try {
        apply_catalog(state,
                      catalog_future_.get(),
                      active_catalog_request_.preferred_song_id,
                      active_catalog_request_.preferred_chart_id);
    } catch (const std::exception& ex) {
        apply_catalog(state,
                      {},
                      active_catalog_request_.preferred_song_id,
                      active_catalog_request_.preferred_chart_id);
        state.load_errors = {ex.what()};
        result.failed = true;
        result.message = ex.what();
    }
    catalog_loading_ = false;
    result.completed = true;

    if (catalog_reload_pending_) {
        catalog_reload_pending_ = false;
        start_catalog_load(state, queued_catalog_request_);
        queued_catalog_request_ = {};
        result.queued_reload_started = true;
    }

    return result;
}

void data_controller::request_ranking_reload(state& state) {
    if (ranking_loading_) {
        ++ranking_generation_;
        ranking_reload_pending_ = true;
        mark_online_ranking_loading(state);
        return;
    }

    const auto filtered = filtered_charts_for_selected_song(state);
    const chart_option* chart = selected_chart_for(state, filtered);
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    const ranking_service::source source = state.ranking_panel.selected_source;

    ++ranking_generation_;
    ranking_pending_generation_ = ranking_generation_;
    reset_ranking_panel_scroll(state);
    mark_online_ranking_loading(state);

    start_ranking_load(state, chart_id, source);
}

ranking_reload_result data_controller::poll_ranking_reload(state& state) {
    ranking_reload_result result;
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
    result.completed = true;
    result.stale = ranking_pending_generation_ != ranking_generation_;
    if (!result.stale) {
        state.ranking_panel.listing = std::move(listing);
        state.ranking_panel.reveal_anim = 0.0f;
    }

    if (ranking_reload_pending_) {
        ranking_reload_pending_ = false;
        request_ranking_reload(state);
        result.queued_reload_started = true;
    }

    return result;
}

void data_controller::start_catalog_load(state& state, catalog_reload_request request) {
    active_catalog_request_ = std::move(request);
    catalog_loading_ = true;
    state.catalog_loading = true;

    std::promise<catalog_data> promise;
    catalog_future_ = promise.get_future();
    auto loader = catalog_loader_;
    const bool calculate_missing_levels = active_catalog_request_.calculate_missing_levels;
    std::thread([promise = std::move(promise), loader = std::move(loader), calculate_missing_levels]() mutable {
        try {
            promise.set_value(loader(calculate_missing_levels));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void data_controller::start_ranking_load(state&,
                                         std::string chart_id,
                                         ranking_service::source source) {
    ranking_loading_ = true;
    std::promise<ranking_service::listing> promise;
    ranking_future_ = promise.get_future();
    auto loader = ranking_loader_;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 chart_id = std::move(chart_id),
                 source]() mutable {
        try {
            promise.set_value(loader(std::move(chart_id), source, 50));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void data_controller::mark_online_ranking_loading(state& state) const {
    if (state.ranking_panel.selected_source != ranking_service::source::online) {
        return;
    }

    state.ranking_panel.listing = {};
    state.ranking_panel.listing.ranking_source = state.ranking_panel.selected_source;
    state.ranking_panel.listing.available = false;
    state.ranking_panel.listing.message = "Loading online rankings...";
}

void data_controller::reset_ranking_panel_scroll(state& state) const {
    state.ranking_panel.source_dropdown_open = false;
    state.ranking_panel.scroll_y = 0.0f;
    state.ranking_panel.scroll_y_target = 0.0f;
    state.ranking_panel.scrollbar_dragging = false;
    state.ranking_panel.scrollbar_drag_offset = 0.0f;
}

}  // namespace song_select
