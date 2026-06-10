#include "title/online_catalog_ranking_loader.h"

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

ranking_service::listing failed_listing(const char* message) {
    ranking_service::listing listing;
    listing.ranking_source = ranking_service::source::online;
    listing.available = false;
    listing.message = message;
    return listing;
}

}  // namespace

ranking_load_controller::ranking_load_controller()
    : ranking_load_controller(load_ranking_from_service) {
}

ranking_load_controller::ranking_load_controller(listing_loader loader)
    : loader_(std::move(loader)) {
}

void ranking_load_controller::reset() {
    active_chart_id_.reset();
    loaded_chart_id_.reset();
    loaded_listing_.reset();
    status_ = load_status::idle;
}

bool ranking_load_controller::loading() const {
    return status_ == load_status::loading;
}

bool ranking_load_controller::request(std::string chart_id) {
    if (chart_id.empty()) {
        reset();
        return false;
    }
    if (active_chart_id_.has_value() && *active_chart_id_ == chart_id) {
        return false;
    }
    if (!loading() && loaded_chart_id_.has_value() && *loaded_chart_id_ == chart_id) {
        return false;
    }

    start_load(std::move(chart_id));
    return true;
}

ranking_load_controller::poll_result ranking_load_controller::poll(const std::string& current_chart_id) {
    poll_result result;
    if (!loading() || !future_.valid()) {
        return result;
    }
    if (future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    ranking_service::listing listing;
    try {
        listing = future_.get();
    } catch (const std::exception& ex) {
        listing = failed_listing(ex.what());
    } catch (...) {
        listing = failed_listing("Could not load global ranking.");
    }

    result.completed = true;
    result.stale = !active_chart_id_.has_value() || *active_chart_id_ != current_chart_id;
    if (!result.stale) {
        loaded_chart_id_ = active_chart_id_;
        loaded_listing_ = listing;
        status_ = listing.available ? load_status::ready : load_status::failed;
        result.loaded = std::move(listing);
    } else {
        loaded_chart_id_.reset();
        loaded_listing_.reset();
        status_ = load_status::idle;
    }
    active_chart_id_.reset();
    return result;
}

ranking_load_controller::snapshot ranking_load_controller::current() const {
    snapshot result;
    result.status = status_;
    if (active_chart_id_.has_value()) {
        result.chart_id = active_chart_id_;
    } else if (loaded_chart_id_.has_value()) {
        result.chart_id = loaded_chart_id_;
    }
    result.listing = loaded_listing_;
    return result;
}

void ranking_load_controller::start_load(std::string chart_id) {
    active_chart_id_ = chart_id;
    loaded_chart_id_.reset();
    loaded_listing_.reset();
    status_ = load_status::loading;

    std::promise<ranking_service::listing> promise;
    future_ = promise.get_future();
    auto loader = loader_;
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
