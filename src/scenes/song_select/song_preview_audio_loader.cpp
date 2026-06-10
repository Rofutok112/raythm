#include "song_select/song_preview_audio_loader.h"

#include <chrono>
#include <filesystem>
#include <thread>

#include "audio_manager.h"
#include "path_utils.h"

namespace song_select {
namespace {

constexpr std::chrono::seconds kPreviewLoadTimeout{15};

}  // namespace

const std::string& preview_audio_loader::target_song_id() const {
    return target_song_id_;
}

bool preview_audio_loader::loading_or_preparing() const {
    return status_ == load_status::loading;
}

preview_audio_loader::load_status preview_audio_loader::status() const {
    return status_;
}

bool preview_audio_loader::request(const song_entry& song) {
    reset();

    target_song_id_ = song.song.meta.song_id;
    std::string audio_source = song.song.meta.audio_url;
    if (audio_source.empty()) {
        if (song.song.meta.audio_file.empty()) {
            reset();
            status_ = load_status::failed;
            return false;
        }

        const std::filesystem::path audio_path = path_utils::join_utf8(song.song.directory, song.song.meta.audio_file);
        if (managed_content_storage::is_within_content_cache(audio_path)) {
            std::promise<managed_content_storage::managed_file_read_result> promise;
            managed_audio_future_ = promise.get_future();
            const std::filesystem::path audio_path_copy = audio_path;
            std::thread([promise = std::move(promise), audio_path_copy]() mutable {
                try {
                    promise.set_value(managed_content_storage::read_managed_file(audio_path_copy));
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }).detach();

            pending_managed_audio_path_ = path_utils::to_utf8(audio_path);
            load_song_ = song.song;
            load_started_at_ = std::chrono::steady_clock::now();
            status_ = load_status::loading;
            managed_audio_pending_ = true;
            return true;
        }

        audio_source = path_utils::to_utf8(audio_path);
    }

    if (!request_path_audio(audio_source)) {
        reset();
        status_ = load_status::failed;
        return false;
    }
    load_song_ = song.song;
    load_started_at_ = std::chrono::steady_clock::now();
    status_ = load_status::loading;
    audio_load_pending_ = true;
    return true;
}

preview_audio_loader::load_event preview_audio_loader::update(const song_entry* selected_song) {
    if ((managed_audio_pending_ || audio_load_pending_) &&
        load_started_at_ != std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() - load_started_at_ > kPreviewLoadTimeout) {
        reset();
        status_ = load_status::failed;
        return {.result = load_event::status::failed};
    }

    load_event prepare_event = poll_managed_audio_prepare(selected_song);
    if (prepare_event.result != load_event::status::none) {
        return prepare_event;
    }

    audio_manager& audio = audio_manager::instance();
    const audio_manager::async_preview_load_result result = audio.poll_preview_load();
    if (!result.completed || !audio_load_pending_) {
        return {};
    }

    audio_load_pending_ = false;
    if (!selected_song_matches_target(selected_song)) {
        reset();
        return {};
    }

    if (result.loaded && load_song_.has_value()) {
        status_ = load_status::ready;
        load_event event;
        event.result = load_event::status::loaded;
        event.song = load_song_;
        return event;
    }

    reset();
    status_ = load_status::failed;
    return {.result = load_event::status::failed};
}

void preview_audio_loader::reset() {
    audio_manager::instance().cancel_preview_load_request();
    target_song_id_.clear();
    pending_managed_audio_path_.clear();
    load_song_.reset();
    load_started_at_ = {};
    managed_audio_pending_ = false;
    audio_load_pending_ = false;
    status_ = load_status::idle;
}

bool preview_audio_loader::request_prepared_audio(std::vector<unsigned char> bytes) {
    return audio_manager::instance().request_preview_load_from_memory(std::move(bytes));
}

bool preview_audio_loader::request_path_audio(const std::string& audio_source) {
    return audio_manager::instance().request_preview_load(audio_source);
}

void preview_audio_loader::clear_prepared_audio_state() {
    pending_managed_audio_path_.clear();
    managed_audio_pending_ = false;
}

preview_audio_loader::load_event preview_audio_loader::poll_managed_audio_prepare(const song_entry* selected_song) {
    if (!managed_audio_pending_ || !managed_audio_future_.valid()) {
        return {};
    }
    if (managed_audio_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return {};
    }

    managed_audio_pending_ = false;
    managed_content_storage::managed_file_read_result managed_audio;
    try {
        managed_audio = managed_audio_future_.get();
    } catch (...) {
        managed_audio = {};
    }

    if (!load_song_.has_value() ||
        target_song_id_ != load_song_->meta.song_id ||
        !selected_song_matches_target(selected_song)) {
        reset();
        return {};
    }

    bool requested = false;
    if (managed_audio.managed) {
        requested = managed_audio.success && request_prepared_audio(std::move(managed_audio.bytes));
    } else {
        requested = request_path_audio(pending_managed_audio_path_);
    }
    clear_prepared_audio_state();

    if (!requested) {
        reset();
        status_ = load_status::failed;
        return {.result = load_event::status::failed};
    }

    load_started_at_ = std::chrono::steady_clock::now();
    status_ = load_status::loading;
    audio_load_pending_ = true;
    return {};
}

bool preview_audio_loader::selected_song_matches_target(const song_entry* selected_song) const {
    return selected_song != nullptr &&
        !target_song_id_.empty() &&
        selected_song->song.meta.song_id == target_song_id_;
}

}  // namespace song_select
