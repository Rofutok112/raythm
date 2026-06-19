#include "song_select/song_select_data_controller.h"

#include <chrono>
#include <exception>
#include <optional>
#include <thread>
#include <utility>

#include "song_select/selection_key.h"

namespace song_select {

namespace {

catalog_reload_request request_for_apply(const state& state, catalog_reload_request request) {
    if (!request.preserve_current_selection) {
        return request;
    }

    const song_entry* song = selected_song(state);
    if (song == nullptr) {
        return request;
    }

    request.preferred_song_id = song->song.meta.song_id;
    const auto filtered = filtered_charts_for_selected_song(state);
    if (const chart_option* chart = selected_chart_for(state, filtered)) {
        request.preferred_chart_id = chart->meta.chart_id;
    } else {
        request.preferred_chart_id.clear();
    }
    return request;
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
    catalog_progress_ = std::make_shared<shared_load_progress>();
    catalog_progress_->set("", 0.0f, false);
    state.catalog_loading = false;
}

bool data_controller::catalog_loading() const {
    return catalog_loading_;
}

load_progress data_controller::catalog_progress() const {
    return catalog_progress_ != nullptr ? catalog_progress_->snapshot() : load_progress{};
}

void data_controller::request_catalog_reload(state& state, catalog_reload_request request) {
    if (catalog_loading_) {
        const bool calculate_missing_levels =
            queued_catalog_request_.calculate_missing_levels || request.calculate_missing_levels;
        const bool preserve_current_selection =
            queued_catalog_request_.preserve_current_selection || request.preserve_current_selection;
        catalog_reload_pending_ = true;
        queued_catalog_request_ = std::move(request);
        queued_catalog_request_.calculate_missing_levels = calculate_missing_levels;
        queued_catalog_request_.preserve_current_selection = preserve_current_selection;
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

    std::optional<catalog_data> loaded_catalog;
    try {
        loaded_catalog = catalog_future_.get();
    } catch (const std::exception& ex) {
        result.failed = true;
        result.message = ex.what();
    }

    result.stale = catalog_reload_pending_;
    catalog_loading_ = false;
    result.completed = true;

    if (catalog_reload_pending_) {
        catalog_reload_pending_ = false;
        start_catalog_load(state, queued_catalog_request_);
        queued_catalog_request_ = {};
        result.queued_reload_started = true;
        return result;
    }

    const selection_key previous_selection = selection_key_for_state(state);
    const catalog_reload_request apply_request = request_for_apply(state, active_catalog_request_);

    const bool level_update_only =
        active_catalog_request_.calculate_missing_levels &&
        active_catalog_request_.preserve_current_selection &&
        state.catalog_loaded_once;

    if (loaded_catalog.has_value() &&
        level_update_only &&
        apply_catalog_chart_level_update(state, *loaded_catalog)) {
        result.chart_levels_updated = true;
        if (catalog_progress_ != nullptr) {
            catalog_progress_->set("Chart levels updated.", 1.0f, false);
        }
    } else if (loaded_catalog.has_value()) {
        apply_catalog(state,
                      std::move(*loaded_catalog),
                      apply_request.preferred_song_id,
                      apply_request.preferred_chart_id);
        if (catalog_progress_ != nullptr) {
            catalog_progress_->set("Local catalog ready.", 1.0f, false);
        }
    } else {
        apply_catalog(state,
                      {},
                      apply_request.preferred_song_id,
                      apply_request.preferred_chart_id);
        state.load_errors = {result.message.empty() ? "Failed to load local catalog." : result.message};
        if (catalog_progress_ != nullptr) {
            catalog_progress_->set(state.load_errors.front(), 0.0f, false);
        }
    }

    const selection_key next_selection = selection_key_for_state(state);
    result.selection_changed = previous_selection != next_selection;
    return result;
}

void data_controller::start_catalog_load(state& state, catalog_reload_request request) {
    active_catalog_request_ = std::move(request);
    catalog_loading_ = true;
    state.catalog_loading = true;

    std::promise<catalog_data> promise;
    catalog_future_ = promise.get_future();
    auto loader = catalog_loader_;
    catalog_progress_ = std::make_shared<shared_load_progress>();
    catalog_progress_->set("Preparing local catalog...", 0.0f, true);
    std::shared_ptr<shared_load_progress> progress = catalog_progress_;
    const bool calculate_missing_levels = active_catalog_request_.calculate_missing_levels;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 progress = std::move(progress),
                 calculate_missing_levels]() mutable {
        try {
            promise.set_value(loader(
                calculate_missing_levels,
                [progress](std::string message, float value, bool active) {
                    progress->set(std::move(message), value, active);
                }));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

}  // namespace song_select
