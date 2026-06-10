#include "song_select/song_select_ranking_loader.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace song_select {

ranking_load_controller::ranking_load_controller() = default;

ranking_load_controller::ranking_load_controller(listing_loader loader)
    : loader_(std::move(loader)) {
}

void ranking_load_controller::reset() {
    active_request_.reset();
    queued_request_.reset();
    loaded_request_.reset();
    snapshot_key_.reset();
    loaded_data_.reset();
    loaded_best_source_ = ranking_service::source::local;
    loaded_best_chart_id_.clear();
    loaded_best_ = false;
    status_ = load_status::idle;
}

bool ranking_load_controller::loading() const {
    return status_ == load_status::loading;
}

ranking_load_controller::load_status ranking_load_controller::status() const {
    return status_;
}

ranking_load_controller::snapshot ranking_load_controller::current() const {
    snapshot result;
    result.status = status_;
    if (status_ == load_status::idle) {
        return result;
    }
    if (active_request_.has_value()) {
        result.key = active_request_->key;
    } else if (loaded_request_.has_value()) {
        result.key = loaded_request_->key;
    } else {
        result.key = snapshot_key_;
    }
    result.data = loaded_data_;
    return result;
}

ranking_request_result ranking_load_controller::request_reload(ranking_load_request next) {
    next.refresh_best = next.refresh_best || !loaded_best_covers(next);
    if (loading()) {
        if (active_request_covers(next)) {
            return {};
        }

        queued_request_ = std::move(next);
        return {
            .reload_action = ranking_request_result::action::queued,
            .accepted_request = queued_request_,
        };
    }

    if (loaded_request_covers(next)) {
        return {};
    }

    const ranking_load_request accepted = next;
    start_load(std::move(next));
    return {
        .reload_action = ranking_request_result::action::started,
        .accepted_request = accepted,
    };
}

ranking_reload_result ranking_load_controller::poll(const ranking_load_request& current_request) {
    ranking_reload_result result;
    if (!loading() || !future_.valid()) {
        return result;
    }
    if (future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    ranking_load_data loaded;
    bool failed = false;
    try {
        loaded = future_.get();
    } catch (const std::exception& ex) {
        failed = true;
        loaded.listing.available = false;
        loaded.listing.message = ex.what();
        loaded.listing.ranking_source =
            active_request_.has_value() ? active_request_->key.source : current_request.key.source;
        loaded.best_source = active_request_.has_value() ? active_request_->best_source : current_request.best_source;
        loaded.best_chart_id = active_request_.has_value() ? active_request_->key.chart_id : current_request.key.chart_id;
    } catch (...) {
        failed = true;
        loaded.listing.available = false;
        loaded.listing.message = "ランキングを読み込めませんでした。";
        loaded.listing.ranking_source =
            active_request_.has_value() ? active_request_->key.source : current_request.key.source;
        loaded.best_source = active_request_.has_value() ? active_request_->best_source : current_request.best_source;
        loaded.best_chart_id = active_request_.has_value() ? active_request_->key.chart_id : current_request.key.chart_id;
    }

    result.completed = true;
    result.stale = queued_request_.has_value() || !active_request_covers(current_request);
    if (!result.stale) {
        status_ = failed ? load_status::failed : load_status::ready;
        if (loaded.listing.available) {
            loaded_request_ = active_request_;
            loaded_data_ = loaded;
        } else {
            loaded_request_ = active_request_;
            loaded_data_ = loaded;
            status_ = load_status::failed;
        }
        if (loaded.best_refreshed) {
            loaded_best_source_ = loaded.best_source;
            loaded_best_chart_id_ = loaded.best_chart_id;
            loaded_best_ = true;
        }
        result.loaded = std::move(loaded);
    } else if (!queued_request_.has_value()) {
        status_ = load_status::idle;
        loaded_request_.reset();
        loaded_data_.reset();
        snapshot_key_.reset();
    }

    active_request_.reset();
    if (queued_request_.has_value()) {
        ranking_load_request queued = std::move(*queued_request_);
        queued_request_.reset();
        result.started_request = queued;
        start_load(std::move(queued));
        result.queued_reload_started = true;
    }

    return result;
}

bool ranking_load_controller::active_request_covers(const ranking_load_request& next) const {
    return active_request_.has_value() &&
        active_request_->key == next.key &&
        active_request_->best_source == next.best_source &&
        (!next.refresh_best || active_request_->refresh_best);
}

bool ranking_load_controller::loaded_request_covers(const ranking_load_request& next) const {
    return loaded_request_.has_value() &&
        loaded_request_->key == next.key &&
        loaded_request_->best_source == next.best_source &&
        (status_ == load_status::failed || !next.refresh_best);
}

bool ranking_load_controller::loaded_best_covers(const ranking_load_request& next) const {
    return loaded_best_ &&
        loaded_best_chart_id_ == next.key.chart_id &&
        loaded_best_source_ == next.best_source;
}

void ranking_load_controller::start_load(ranking_load_request next) {
    active_request_ = next;
    snapshot_key_ = next.key;
    status_ = load_status::loading;

    std::promise<ranking_load_data> promise;
    future_ = promise.get_future();
    loaded_data_.reset();
    auto loader = loader_;
    std::thread([promise = std::move(promise),
                 loader = std::move(loader),
                 next = std::move(next)]() mutable {
        try {
            ranking_load_data loaded;
            loaded.best_source = next.best_source;
            loaded.best_chart_id = next.key.chart_id;
            loaded.listing = loader(next.key.chart_id, next.key.source, 50);
            if (next.refresh_best) {
                loaded.best_entry = ranking_service::load_chart_personal_best(next.key.chart_id, next.best_source);
                loaded.best_refreshed = true;
            }
            promise.set_value(std::move(loaded));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

ranking_load_controller::snapshot ranking_snapshot_for_key(
    ranking_load_controller::snapshot snapshot,
    const selection_key& key) {
    if (!snapshot.key.has_value() || *snapshot.key != key) {
        return {};
    }
    return snapshot;
}

}  // namespace song_select
