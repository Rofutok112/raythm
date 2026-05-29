#include "song_select/song_select_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "network/auth_client.h"

namespace song_select {
namespace {

bool has_queueable_link_for_server(const chart_option& chart, const std::string& server_url) {
    if (!can_use_online_chart_routes(chart)) {
        return false;
    }
    const std::string normalized_server_url = auth::normalize_server_url(server_url);
    if (normalized_server_url.empty()) {
        return false;
    }
    if (online_content::is_queueable(chart.online_identity) &&
        auth::normalize_server_url(chart.online_identity->server_url) == normalized_server_url) {
        return true;
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (online_content::is_queueable(link) &&
            auth::normalize_server_url(link.server_url) == normalized_server_url) {
            return true;
        }
    }
    return false;
}

bool uses_submitted_ranking_best(const chart_option* chart) {
    if (chart == nullptr) {
        return false;
    }
    if (!can_use_online_chart_routes(*chart)) {
        return false;
    }
    if (chart->source_status == content_status::official ||
            chart->source_status == content_status::community ||
            chart->status == content_status::official ||
            chart->status == content_status::community) {
        return true;
    }

    const auth::session_summary summary = auth::load_session_summary();
    return summary.logged_in && has_queueable_link_for_server(*chart, summary.server_url);
}

}  // namespace

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
    const auto filtered = filtered_charts_for_selected_song(state);
    const chart_option* chart = selected_chart_for(state, filtered);
    if (state.ranking_panel.selected_source == ranking_service::source::online &&
        (chart == nullptr || !can_use_online_chart_routes(*chart))) {
        state.ranking_panel.selected_source = ranking_service::source::local;
    }

    if (ranking_loading_) {
        ++ranking_generation_;
        ranking_reload_pending_ = true;
        mark_online_ranking_loading(state);
        return;
    }

    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    const ranking_service::source source = state.ranking_panel.selected_source;
    const ranking_service::source best_source = uses_submitted_ranking_best(chart)
        ? ranking_service::source::online
        : ranking_service::source::local;
    const bool refresh_best =
        state.ranking_panel.best_chart_id != chart_id ||
        state.ranking_panel.best_source != best_source ||
        !state.ranking_panel.best_loaded;

    ++ranking_generation_;
    ranking_pending_generation_ = ranking_generation_;
    reset_ranking_panel_scroll(state);
    if (refresh_best) {
        state.ranking_panel.best_source = best_source;
        state.ranking_panel.best_chart_id = chart_id;
        state.ranking_panel.best_loaded = false;
        state.ranking_panel.best_entry.reset();
    }
    mark_online_ranking_loading(state);

    start_ranking_load(state, chart_id, source, best_source, refresh_best);
}

ranking_reload_result data_controller::poll_ranking_reload(state& state) {
    ranking_reload_result result;
    if (!ranking_loading_) {
        return result;
    }
    if (ranking_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    ranking_load_data loaded;
    try {
        loaded = ranking_future_.get();
    } catch (const std::exception& ex) {
        loaded.listing.available = false;
        loaded.listing.message = ex.what();
        loaded.listing.ranking_source = state.ranking_panel.selected_source;
        loaded.best_source = state.ranking_panel.best_source;
        loaded.best_chart_id = state.ranking_panel.best_chart_id;
    }
    ranking_loading_ = false;
    result.completed = true;
    result.stale = ranking_pending_generation_ != ranking_generation_;
    if (!result.stale) {
        state.ranking_panel.listing = std::move(loaded.listing);
        if (loaded.best_refreshed) {
            state.ranking_panel.best_source = loaded.best_source;
            state.ranking_panel.best_chart_id = loaded.best_chart_id;
            state.ranking_panel.best_entry = std::move(loaded.best_entry);
            state.ranking_panel.best_loaded = true;
        }
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
                                         ranking_service::source source,
                                         ranking_service::source best_source,
                                         bool refresh_best) {
    ranking_loading_ = true;
    std::promise<ranking_load_data> promise;
    ranking_future_ = promise.get_future();
    auto loader = ranking_loader_;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 chart_id = std::move(chart_id),
                 source,
                 best_source,
                 refresh_best]() mutable {
        try {
            ranking_load_data loaded;
            loaded.best_source = best_source;
            loaded.best_chart_id = chart_id;
            loaded.listing = loader(chart_id, source, 50);
            if (refresh_best) {
                loaded.best_entry = ranking_service::load_chart_personal_best(chart_id, best_source);
                loaded.best_refreshed = true;
            }
            promise.set_value(std::move(loaded));
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
