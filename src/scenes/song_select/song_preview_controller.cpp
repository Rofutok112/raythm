#include "song_select/song_preview_controller.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "audio_manager.h"
#include "game_settings.h"
#include "managed_content_storage.h"
#include "path_utils.h"

namespace {

constexpr float kPreviewFadeSpeed = 2.4f;
constexpr float kPreviewMaxVolume = 0.55f;
constexpr double kPreviewLoopTailSeconds = 1.0;

double effective_preview_length_seconds(const song_data& song) {
    const double metadata_length = static_cast<double>(song.meta.duration_seconds);
    if (!song.meta.audio_url.empty()) {
        return metadata_length;
    }

    const double stream_length = audio_manager::instance().get_preview_length_seconds();
    if (metadata_length > 0.0) {
        if (stream_length <= 0.0) {
            return metadata_length;
        }
        return stream_length;
    }
    return stream_length;
}

song_select::jacket_cache::pending_texture load_jacket_bytes(const song_data& song) {
    song_select::jacket_cache::pending_texture result;
    if (song.meta.jacket_file.empty()) {
        return result;
    }

    const std::filesystem::path jacket_path = path_utils::join_utf8(song.directory, song.meta.jacket_file);
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(jacket_path);
    if (managed.managed) {
        if (managed.success) {
            result.bytes = managed.bytes;
            result.file_type = jacket_path.extension().string();
        }
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(jacket_path, ec) || !std::filesystem::is_regular_file(jacket_path, ec)) {
        return result;
    }

    std::ifstream input(jacket_path, std::ios::binary);
    if (!input.is_open()) {
        return result;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size <= 0) {
        return result;
    }

    result.bytes.resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(result.bytes.data()), size);
    if (!input.good() && !input.eof()) {
        result.bytes.clear();
        result.file_type.clear();
        return result;
    }

    result.file_type = jacket_path.extension().string();
    return result;
}

Texture2D load_jacket_texture(const song_select::jacket_cache::pending_texture& pending) {
    if (pending.bytes.empty() || pending.file_type.empty()) {
        return {};
    }

    Image image = LoadImageFromMemory(pending.file_type.c_str(),
                                      pending.bytes.data(),
                                      static_cast<int>(pending.bytes.size()));
    if (image.data == nullptr) {
        return {};
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id != 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
    return texture;
}

}  // namespace

namespace song_select {

preview_controller::~preview_controller() {
    stop();
}

void preview_controller::select_song(const song_entry* song) {
    if (song == nullptr) {
        stop();
        return;
    }

    load_jacket(song);
    queue_preview(song);
}

void preview_controller::update(float dt, const song_entry* selected_song) {
    poll_jacket_load();

    audio_manager& audio = audio_manager::instance();
    const preview_audio_loader::load_event load_event = audio_loader_.update(selected_song);
    if (load_event.result == preview_audio_loader::load_event::status::loaded &&
        load_event.song.has_value()) {
        audio.seek_preview(load_event.song->meta.preview_start_seconds);
        audio.set_preview_volume(g_settings.bgm_volume);
        audio.set_preview_fade_gain(0.0f);
        audio.play_preview(false);
        preview_volume_ = 0.0f;
        active_preview_song_ = load_event.song;
        preview_fade_direction_ = 1;
    } else if (load_event.result == preview_audio_loader::load_event::status::failed) {
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
    }

    if (preview_fade_direction_ < 0) {
        preview_volume_ = std::max(0.0f, preview_volume_ - dt * kPreviewFadeSpeed);
        audio.set_preview_volume(g_settings.bgm_volume);
        audio.set_preview_fade_gain(preview_volume_);
        if (preview_volume_ <= 0.0f) {
            audio.stop_preview();
            audio_loader_.reset();
            active_preview_song_.reset();
            preview_fade_direction_ = 0;
            if (pending_preview_song_.has_value()) {
                if (selected_song != nullptr && selected_song->song.meta.song_id == pending_preview_song_->meta.song_id) {
                    start_preview(*selected_song);
                } else {
                    pending_preview_song_.reset();
                }
            }
        }
    } else if (preview_fade_direction_ > 0) {
        preview_volume_ = std::min(kPreviewMaxVolume, preview_volume_ + dt * kPreviewFadeSpeed);
        audio.set_preview_volume(g_settings.bgm_volume);
        audio.set_preview_fade_gain(preview_volume_);
        if (preview_volume_ >= kPreviewMaxVolume) {
            preview_fade_direction_ = 0;
        }
    } else if (active_preview_song_.has_value() &&
               audio.is_preview_loaded() &&
               audio.is_preview_playing()) {
        const double preview_length = effective_preview_length_seconds(*active_preview_song_);
        if (preview_length <= 0.0) {
            return;
        }

        const double remaining = preview_length - audio.get_preview_position_seconds();
        if (remaining <= kPreviewLoopTailSeconds) {
            preview_fade_direction_ = -1;
            pending_preview_song_ = *active_preview_song_;
        }
    }
}

void preview_controller::resume(const song_entry* song) {
    if (song == nullptr) {
        stop();
        return;
    }

    load_jacket(song);
    audio_manager& audio = audio_manager::instance();
    if (audio_loader_.target_song_id() == song->song.meta.song_id && audio_loader_.loading_or_preparing()) {
        return;
    }
    if (audio_loader_.target_song_id() == song->song.meta.song_id && audio.is_preview_loaded()) {
        pending_preview_song_.reset();
        active_preview_song_ = song->song;
        preview_fade_direction_ = 0;
        preview_volume_ = std::max(preview_volume_, kPreviewMaxVolume);
        audio.set_preview_volume(g_settings.bgm_volume);
        audio.set_preview_fade_gain(preview_volume_);
        audio.play_preview(false);
        return;
    }

    select_song(song);
}

void preview_controller::pause() {
    pending_preview_song_.reset();
    audio_manager& audio = audio_manager::instance();
    if (!audio.is_preview_loaded()) {
        return;
    }

    preview_fade_direction_ = 0;
    audio.pause_preview();
}

void preview_controller::fade_out() {
    pending_preview_song_.reset();
    if (audio_loader_.target_song_id().empty() || !audio_manager::instance().is_preview_loaded()) {
        stop();
        return;
    }
    preview_fade_direction_ = -1;
}

void preview_controller::stop() {
    audio_manager::instance().unload_preview();
    audio_loader_.reset();
    jacket_song_id_.clear();
    pending_preview_song_.reset();
    active_preview_song_.reset();
    preview_volume_ = 0.0f;
    preview_fade_direction_ = 0;
    unload_jacket();
    jacket_load_pending_ = false;
    jacket_missing_ = false;
}

bool preview_controller::is_audio_active() const {
    audio_manager& audio = audio_manager::instance();
    return audio.is_preview_loaded() || audio.is_preview_playing();
}

bool preview_controller::is_playing() const {
    return audio_manager::instance().is_preview_playing();
}

void preview_controller::unload_jacket() {
    if (!jacket_loaded_) {
        return;
    }

    UnloadTexture(jacket_texture_);
    jacket_texture_ = {};
    jacket_loaded_ = false;
}

void preview_controller::load_jacket(const song_entry* song) {
    if (song == nullptr) {
        unload_jacket();
        jacket_song_id_.clear();
        return;
    }

    if (jacket_song_id_ == song->song.meta.song_id &&
        (jacket_loaded_ || jacket_load_pending_ || jacket_missing_)) {
        return;
    }

    unload_jacket();
    jacket_song_id_ = song->song.meta.song_id;
    jacket_load_pending_ = false;
    jacket_missing_ = false;
    if (song->song.meta.jacket_file.empty()) {
        jacket_missing_ = true;
        return;
    }

    std::promise<jacket_cache::pending_texture> promise;
    jacket_load_future_ = promise.get_future();
    const song_data song_copy = song->song;
    std::thread([promise = std::move(promise), song_copy]() mutable {
        try {
            promise.set_value(load_jacket_bytes(song_copy));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
    jacket_load_pending_ = true;
}

void preview_controller::poll_jacket_load() {
    if (!jacket_load_pending_ || !jacket_load_future_.valid()) {
        return;
    }
    if (jacket_load_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    jacket_load_pending_ = false;
    try {
        jacket_texture_ = load_jacket_texture(jacket_load_future_.get());
    } catch (...) {
        jacket_texture_ = {};
    }
    jacket_loaded_ = jacket_texture_.id != 0;
    jacket_missing_ = !jacket_loaded_;
    if (jacket_loaded_) {
        SetTextureFilter(jacket_texture_, TEXTURE_FILTER_BILINEAR);
    }
}

void preview_controller::queue_preview(const song_entry* song) {
    if (song == nullptr) {
        return;
    }

    if (audio_loader_.target_song_id() == song->song.meta.song_id) {
        return;
    }

    pending_preview_song_ = song->song;
    start_preview(*song);
}

void preview_controller::start_preview(const song_entry& song) {
    if (!audio_loader_.request(song)) {
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        return;
    }

    preview_volume_ = 0.0f;
    active_preview_song_.reset();
    preview_fade_direction_ = 0;
    pending_preview_song_.reset();
}

}  // namespace song_select
