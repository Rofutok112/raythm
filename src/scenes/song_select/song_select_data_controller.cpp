#include "song_select/song_select_data_controller.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace song_select {
namespace {

struct catalog_selection {
    std::string song_id;
    std::string chart_id;

    [[nodiscard]] bool operator==(const catalog_selection& other) const {
        return song_id == other.song_id && chart_id == other.chart_id;
    }

    [[nodiscard]] bool operator!=(const catalog_selection& other) const {
        return !(*this == other);
    }
};

catalog_selection current_catalog_selection(const state& state) {
    catalog_selection selection;
    if (const song_entry* song = selected_song(state)) {
        selection.song_id = song->song.meta.song_id;
    }
    const auto filtered = filtered_charts_for_selected_song(state);
    if (const chart_option* chart = selected_chart_for(state, filtered)) {
        selection.chart_id = chart->meta.chart_id;
    }
    return selection;
}

}  // namespace

data_controller::data_controller()
    : data_controller(load_catalog_from_service) {
}

data_controller::data_controller(catalog_loader catalog_loader_fn)
    : catalog_loader_(std::move(catalog_loader_fn)) {
}

catalog_data data_controller::load_catalog_from_service(bool calculate_missing_levels,
                                                        catalog_progress_callback progress) {
    return load_catalog(calculate_missing_levels, std::move(progress));
}

void data_controller::reset(state& state) {
    catalog_loading_ = false;
    catalog_reload_pending_ = false;
    active_catalog_request_ = {};
    queued_catalog_request_ = {};
    catalog_progress_.set("", 0.0f, false);
    state.catalog_loading = false;
}

bool data_controller::catalog_loading() const {
    return catalog_loading_;
}

load_progress data_controller::catalog_progress() const {
    return catalog_progress_.snapshot();
}

void data_controller::request_catalog_reload(state& state, catalog_reload_request request) {
    if (catalog_loading_) {
        const bool calculate_missing_levels =
            queued_catalog_request_.calculate_missing_levels || request.calculate_missing_levels;
        catalog_reload_pending_ = true;
        queued_catalog_request_ = std::move(request);
        queued_catalog_request_.calculate_missing_levels = calculate_missing_levels;
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

    const catalog_selection previous_selection = current_catalog_selection(state);

    try {
        apply_catalog(state,
                      catalog_future_.get(),
                      active_catalog_request_.preferred_song_id,
                      active_catalog_request_.preferred_chart_id);
        catalog_progress_.set("Local catalog ready.", 1.0f, false);
    } catch (const std::exception& ex) {
        apply_catalog(state,
                      {},
                      active_catalog_request_.preferred_song_id,
                      active_catalog_request_.preferred_chart_id);
        state.load_errors = {ex.what()};
        catalog_progress_.set(ex.what(), 0.0f, false);
        result.failed = true;
        result.message = ex.what();
    }
    const catalog_selection next_selection = current_catalog_selection(state);
    result.selection_changed = previous_selection != next_selection;

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

void data_controller::start_catalog_load(state& state, catalog_reload_request request) {
    active_catalog_request_ = std::move(request);
    catalog_loading_ = true;
    state.catalog_loading = true;

    std::promise<catalog_data> promise;
    catalog_future_ = promise.get_future();
    auto loader = catalog_loader_;
    catalog_progress_.set("Preparing local catalog...", 0.0f, true);
    shared_load_progress* progress = &catalog_progress_;
    const bool calculate_missing_levels = active_catalog_request_.calculate_missing_levels;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 progress,
                 calculate_missing_levels]() mutable {
        try {
            promise.set_value(loader(
                calculate_missing_levels,
                [progress](std::string message, float value, bool active) {
                    if (progress != nullptr) {
                        progress->set(std::move(message), value, active);
                    }
                }));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

}  // namespace song_select
