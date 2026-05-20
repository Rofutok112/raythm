#include "title/online_catalog_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace online_catalog {

namespace {

ranking_service::listing load_ranking_from_service(std::string chart_id,
                                                   ranking_service::source source,
                                                   int limit) {
    return ranking_service::load_chart_ranking(std::move(chart_id), source, limit);
}

}  // namespace

data_controller::data_controller()
    : data_controller(load_ranking_from_service) {
}

data_controller::data_controller(ranking_loader ranking_loader_fn)
    : ranking_loader_(std::move(ranking_loader_fn)) {
}

void data_controller::reset() {
    ranking_loading_ = false;
    ranking_requested_chart_id_.clear();
    ranking_loaded_chart_id_.clear();
}

std::future<title_online_view::catalog_load_result>& data_controller::catalog_future() {
    return catalog_future_;
}

std::future<title_online_view::remote_song_page_fetch_result>& data_controller::song_page_future() {
    return song_page_future_;
}

std::future<title_online_view::remote_chart_page_fetch_result>& data_controller::chart_page_future() {
    return chart_page_future_;
}

std::future<std::vector<title_online_view::song_entry_state>>& data_controller::owned_future() {
    return owned_future_;
}

std::future<title_online_view::download_song_result>& data_controller::download_future() {
    return download_future_;
}

void data_controller::request_selected_chart_ranking(title_online_view::state& state) {
    if (!state.detail_open) {
        return;
    }

    const std::string chart_id = selected_ranking_chart_id(state);
    if (chart_id.empty()) {
        ranking_requested_chart_id_.clear();
        ranking_loaded_chart_id_.clear();
        set_select_chart_message(state);
        return;
    }

    if (ranking_loading_ && ranking_requested_chart_id_ == chart_id) {
        return;
    }
    if (!ranking_loading_ && ranking_loaded_chart_id_ == chart_id) {
        return;
    }

    start_ranking_load(state, chart_id);
}

void data_controller::poll_selected_chart_ranking(title_online_view::state& state) {
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
        listing.ranking_source = ranking_service::source::online;
        listing.available = false;
        listing.message = ex.what();
    } catch (...) {
        listing.ranking_source = ranking_service::source::online;
        listing.available = false;
        listing.message = "Could not load global ranking.";
    }

    const std::string completed_chart_id = ranking_requested_chart_id_;
    ranking_loading_ = false;
    state.ranking_loading = false;
    if (completed_chart_id == selected_ranking_chart_id(state)) {
        ranking_loaded_chart_id_ = completed_chart_id;
        state.ranking_listing = std::move(listing);
    }
    request_selected_chart_ranking(state);
}

std::string data_controller::selected_ranking_chart_id(const title_online_view::state& state) const {
    const title_online_view::chart_entry_state* chart = title_online_view::selected_chart(state);
    return chart != nullptr ? chart->chart.meta.chart_id : "";
}

void data_controller::set_select_chart_message(title_online_view::state& state) {
    state.ranking_loading = false;
    state.ranking_listing = {};
    state.ranking_listing.ranking_source = ranking_service::source::online;
    state.ranking_listing.available = false;
    state.ranking_listing.message = "Select a chart to view global ranking.";
}

void data_controller::start_ranking_load(title_online_view::state& state, std::string chart_id) {
    ranking_requested_chart_id_ = chart_id;
    ranking_loaded_chart_id_.clear();
    ranking_loading_ = true;
    state.ranking_loading = true;
    state.ranking_listing = {};
    state.ranking_listing.ranking_source = ranking_service::source::online;
    state.ranking_listing.available = false;
    state.ranking_listing.message = "Loading global ranking...";

    std::promise<ranking_service::listing> promise;
    ranking_future_ = promise.get_future();
    auto loader = ranking_loader_;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 chart_id = std::move(chart_id)]() mutable {
        try {
            promise.set_value(loader(std::move(chart_id), ranking_service::source::online, 10));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

}  // namespace online_catalog
