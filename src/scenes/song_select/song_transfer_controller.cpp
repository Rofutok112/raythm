#include "song_transfer_controller.h"

#include <chrono>
#include <exception>
#include <future>
#include <thread>
#include <utility>

namespace song_select::transfer {

void controller::on_exit() {
    clear_pending_song_import_request(true);
    clear_pending_chart_import_request();
}

void controller::start_song_import_prepare(const state& catalog_state, std::string source_path) {
    start_song_import_prepare(catalog_state, std::vector<std::string>{std::move(source_path)});
}

void controller::start_song_import_prepare(const state& catalog_state, std::vector<std::string> source_paths) {
    prepare_active_ = true;
    busy_label_ = source_paths.size() <= 1 ? "Reading song package..." : "Reading song packages...";
    std::promise<song_import_prepare_batch_result> promise;
    background_song_import_prepare_ = promise.get_future();
    std::thread([promise = std::move(promise), catalog_state, source_paths = std::move(source_paths)]() mutable {
        try {
            promise.set_value(prepare_song_imports_from_paths(catalog_state, source_paths));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void controller::start_song_export(song_export_request request) {
    transfer_active_ = true;
    busy_label_ = "Exporting song package...";
    std::promise<transfer_result> promise;
    background_transfer_ = promise.get_future();
    std::thread([promise = std::move(promise), request = std::move(request)]() mutable {
        try {
            promise.set_value(export_song_package(request));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void controller::start_song_import(song_import_request request) {
    start_song_imports(std::vector<song_import_request>{std::move(request)});
}

void controller::start_song_imports(std::vector<song_import_request> requests) {
    transfer_active_ = true;
    busy_label_ = requests.size() <= 1 ? "Importing song package..." : "Importing song packages...";
    pending_song_import_requests_ = requests;
    std::promise<transfer_result> promise;
    background_transfer_ = promise.get_future();
    std::thread([promise = std::move(promise), requests = std::move(requests)]() mutable {
        try {
            promise.set_value(import_song_packages(requests));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

std::optional<song_import_prepare_batch_result> controller::poll_song_import_prepare() {
    if (!prepare_active_) {
        return std::nullopt;
    }
    if (background_song_import_prepare_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return std::nullopt;
    }

    prepare_active_ = false;
    busy_label_.clear();
    try {
        return background_song_import_prepare_.get();
    } catch (const std::exception& ex) {
        song_import_prepare_batch_result result;
        result.transfer.success = false;
        result.transfer.message = ex.what();
        return result;
    } catch (...) {
        song_import_prepare_batch_result result;
        result.transfer.success = false;
        result.transfer.message = "Failed to read song package.";
        return result;
    }
}

std::optional<transfer_result> controller::poll_background_transfer() {
    if (!transfer_active_) {
        return std::nullopt;
    }
    if (background_transfer_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return std::nullopt;
    }

    transfer_active_ = false;
    busy_label_.clear();
    transfer_result result;
    try {
        result = background_transfer_.get();
    } catch (const std::exception& ex) {
        result.success = false;
        result.message = ex.what();
    } catch (...) {
        result.success = false;
        result.message = "Song transfer failed.";
    }
    clear_pending_song_import_request(true);
    return result;
}

void controller::set_pending_song_import_request(song_import_request request) {
    set_pending_song_import_requests(std::vector<song_import_request>{std::move(request)});
}

void controller::set_pending_song_import_requests(std::vector<song_import_request> requests) {
    pending_song_import_requests_ = std::move(requests);
}

void controller::clear_pending_song_import_request(bool cleanup_request) {
    if (cleanup_request) {
        cleanup_song_import_requests(pending_song_import_requests_);
    }
    pending_song_import_requests_.clear();
}

void controller::set_pending_chart_import_request(chart_import_request request) {
    set_pending_chart_import_requests(std::vector<chart_import_request>{std::move(request)});
}

void controller::set_pending_chart_import_requests(std::vector<chart_import_request> requests) {
    pending_chart_import_requests_ = std::move(requests);
}

void controller::clear_pending_chart_import_request() {
    pending_chart_import_requests_.clear();
}

}  // namespace song_select::transfer
