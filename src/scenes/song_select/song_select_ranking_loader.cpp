#include "song_select/song_select_ranking_loader.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "network/auth_client.h"
#include "song_select/song_select_navigation.h"

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
    if (chart->source == content_source::official ||
            chart->source == content_source::community) {
        return true;
    }

    const auth::session_summary summary = auth::load_session_summary();
    return summary.logged_in && has_queueable_link_for_server(*chart, summary.server_url);
}

}  // namespace

ranking_load_controller::ranking_load_controller() = default;

ranking_load_controller::ranking_load_controller(listing_loader loader)
    : loader_(std::move(loader)) {
}

void ranking_load_controller::reset(state& state) {
    active_request_.reset();
    queued_request_.reset();
    loaded_request_.reset();
    status_ = load_status::idle;
    state.ranking_panel.selected_source = ranking_service::source::local;
}

bool ranking_load_controller::loading() const {
    return status_ == load_status::loading;
}

ranking_load_controller::load_status ranking_load_controller::status() const {
    return status_;
}

void ranking_load_controller::request_reload(state& state) {
    request next = build_request(state);

    if (loading()) {
        if (active_request_covers(next)) {
            return;
        }

        queued_request_ = std::move(next);
        mark_online_loading(state);
        return;
    }

    if (loaded_request_covers(next)) {
        return;
    }

    reset_panel_scroll(state);
    if (next.refresh_best) {
        state.ranking_panel.best_source = next.best_source;
        state.ranking_panel.best_chart_id = next.chart_id;
        state.ranking_panel.best_loaded = false;
        state.ranking_panel.best_entry.reset();
    }
    mark_online_loading(state);
    start_load(std::move(next));
}

ranking_reload_result ranking_load_controller::poll(state& state) {
    ranking_reload_result result;
    if (!loading() || !future_.valid()) {
        return result;
    }
    if (future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    load_data loaded;
    bool failed = false;
    try {
        loaded = future_.get();
    } catch (const std::exception& ex) {
        failed = true;
        loaded.listing.available = false;
        loaded.listing.message = ex.what();
        loaded.listing.ranking_source = state.ranking_panel.selected_source;
        loaded.best_source = state.ranking_panel.best_source;
        loaded.best_chart_id = state.ranking_panel.best_chart_id;
    } catch (...) {
        failed = true;
        loaded.listing.available = false;
        loaded.listing.message = "Ranking load failed.";
        loaded.listing.ranking_source = state.ranking_panel.selected_source;
        loaded.best_source = state.ranking_panel.best_source;
        loaded.best_chart_id = state.ranking_panel.best_chart_id;
    }

    status_ = failed ? load_status::failed : load_status::ready;
    result.completed = true;
    result.stale = queued_request_.has_value();
    if (!result.stale) {
        apply_loaded(state, std::move(loaded));
    }

    active_request_.reset();
    if (queued_request_.has_value()) {
        request queued = std::move(*queued_request_);
        queued_request_.reset();
        start_load(std::move(queued));
        result.queued_reload_started = true;
    }

    return result;
}

ranking_load_controller::request ranking_load_controller::build_request(state& state) const {
    const auto filtered = filtered_charts_for_selected_song(state);
    const chart_option* chart = selected_chart_for(state, filtered);
    if (state.ranking_panel.selected_source == ranking_service::source::online &&
        (chart == nullptr || !can_use_online_chart_routes(*chart))) {
        state.ranking_panel.selected_source = ranking_service::source::local;
    }

    request next;
    next.chart_id = chart != nullptr ? chart->meta.chart_id : "";
    next.source = state.ranking_panel.selected_source;
    next.best_source = uses_submitted_ranking_best(chart)
        ? ranking_service::source::online
        : ranking_service::source::local;
    next.refresh_best =
        state.ranking_panel.best_chart_id != next.chart_id ||
        state.ranking_panel.best_source != next.best_source ||
        !state.ranking_panel.best_loaded;
    return next;
}

bool ranking_load_controller::active_request_covers(const request& next) const {
    return active_request_.has_value() &&
        active_request_->chart_id == next.chart_id &&
        active_request_->source == next.source &&
        active_request_->best_source == next.best_source &&
        (!next.refresh_best || active_request_->refresh_best);
}

bool ranking_load_controller::loaded_request_covers(const request& next) const {
    return loaded_request_.has_value() &&
        loaded_request_->chart_id == next.chart_id &&
        loaded_request_->source == next.source &&
        loaded_request_->best_source == next.best_source &&
        !next.refresh_best;
}

void ranking_load_controller::start_load(request next) {
    active_request_ = next;
    status_ = load_status::loading;

    std::promise<load_data> promise;
    future_ = promise.get_future();
    auto loader = loader_;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 next = std::move(next)]() mutable {
        try {
            load_data loaded;
            loaded.best_source = next.best_source;
            loaded.best_chart_id = next.chart_id;
            loaded.listing = loader(next.chart_id, next.source, 50);
            if (next.refresh_best) {
                loaded.best_entry = ranking_service::load_chart_personal_best(next.chart_id, next.best_source);
                loaded.best_refreshed = true;
            }
            promise.set_value(std::move(loaded));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void ranking_load_controller::apply_loaded(state& state, load_data loaded) {
    state.ranking_panel.listing = std::move(loaded.listing);
    if (loaded.best_refreshed) {
        state.ranking_panel.best_source = loaded.best_source;
        state.ranking_panel.best_chart_id = loaded.best_chart_id;
        state.ranking_panel.best_entry = std::move(loaded.best_entry);
        state.ranking_panel.best_loaded = true;
    }
    if (state.ranking_panel.listing.available) {
        loaded_request_ = active_request_;
    } else {
        loaded_request_.reset();
        status_ = load_status::failed;
    }
    state.ranking_panel.reveal_anim = 0.0f;
}

void ranking_load_controller::mark_online_loading(state& state) const {
    if (state.ranking_panel.selected_source != ranking_service::source::online) {
        return;
    }

    state.ranking_panel.listing = {};
    state.ranking_panel.listing.ranking_source = state.ranking_panel.selected_source;
    state.ranking_panel.listing.available = false;
    state.ranking_panel.listing.message = "Loading online rankings...";
}

void ranking_load_controller::reset_panel_scroll(state& state) const {
    state.ranking_panel.source_dropdown_open = false;
    state.ranking_panel.scroll_y = 0.0f;
    state.ranking_panel.scroll_y_target = 0.0f;
    state.ranking_panel.scrollbar_dragging = false;
    state.ranking_panel.scrollbar_drag_offset = 0.0f;
}

}  // namespace song_select
