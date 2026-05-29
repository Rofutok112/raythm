#include "song_select/song_preview_controller.h"

#include <algorithm>
#include <filesystem>

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
    audio_manager& audio = audio_manager::instance();
    const audio_manager::async_preview_load_result load_result = audio.poll_preview_load();
    if (load_result.completed && preview_load_pending_) {
        if (load_result.loaded && selected_song != nullptr &&
            selected_song->song.meta.song_id == preview_song_id_) {
            audio.seek_preview(preview_load_song_->meta.preview_start_seconds);
            audio.set_preview_volume(g_settings.bgm_volume);
            audio.set_preview_fade_gain(0.0f);
            audio.play_preview(false);
            preview_volume_ = 0.0f;
            active_preview_song_ = preview_load_song_;
            preview_fade_direction_ = 1;
        } else if (!load_result.loaded) {
            preview_song_id_.clear();
            preview_volume_ = 0.0f;
            preview_fade_direction_ = 0;
        }
        preview_load_pending_ = false;
        preview_load_song_.reset();
    }

    if (preview_fade_direction_ < 0) {
        preview_volume_ = std::max(0.0f, preview_volume_ - dt * kPreviewFadeSpeed);
        audio.set_preview_volume(g_settings.bgm_volume);
        audio.set_preview_fade_gain(preview_volume_);
        if (preview_volume_ <= 0.0f) {
            audio.stop_preview();
            preview_song_id_.clear();
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
    if (preview_song_id_ == song->song.meta.song_id && audio.is_preview_loading()) {
        return;
    }
    if (preview_song_id_ == song->song.meta.song_id && audio.is_preview_loaded()) {
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
    if (preview_song_id_.empty() || !audio_manager::instance().is_preview_loaded()) {
        stop();
        return;
    }
    preview_fade_direction_ = -1;
}

void preview_controller::stop() {
    audio_manager::instance().unload_preview();
    preview_song_id_.clear();
    jacket_song_id_.clear();
    pending_preview_song_.reset();
    active_preview_song_.reset();
    preview_load_pending_ = false;
    preview_load_song_.reset();
    preview_volume_ = 0.0f;
    preview_fade_direction_ = 0;
    unload_jacket();
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

    if (jacket_song_id_ == song->song.meta.song_id && jacket_loaded_) {
        return;
    }

    unload_jacket();
    jacket_song_id_ = song->song.meta.song_id;
    if (song->song.meta.jacket_file.empty()) {
        return;
    }

    const std::filesystem::path jacket_path = path_utils::join_utf8(song->song.directory, song->song.meta.jacket_file);
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(jacket_path);
    if (managed.managed) {
        if (!managed.success || managed.bytes.empty()) {
            return;
        }
        Image image = LoadImageFromMemory(jacket_path.extension().string().c_str(),
                                          managed.bytes.data(),
                                          static_cast<int>(managed.bytes.size()));
        if (image.data == nullptr) {
            return;
        }
        jacket_texture_ = LoadTextureFromImage(image);
        UnloadImage(image);
    } else {
        if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
            return;
        }
        const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
        jacket_texture_ = LoadTexture(jacket_path_utf8.c_str());
    }
    jacket_loaded_ = jacket_texture_.id != 0;
    if (jacket_loaded_) {
        SetTextureFilter(jacket_texture_, TEXTURE_FILTER_BILINEAR);
    }
}

void preview_controller::queue_preview(const song_entry* song) {
    if (song == nullptr) {
        return;
    }

    if (preview_song_id_ == song->song.meta.song_id) {
        return;
    }

    pending_preview_song_ = song->song;
    if (preview_song_id_.empty() || !audio_manager::instance().is_preview_loaded()) {
        start_preview(*song);
        return;
    }

    preview_fade_direction_ = -1;
}

void preview_controller::start_preview(const song_entry& song) {
    audio_manager& audio = audio_manager::instance();
    std::string audio_source = song.song.meta.audio_url;
    if (audio_source.empty()) {
        if (song.song.meta.audio_file.empty()) {
            preview_song_id_.clear();
            preview_volume_ = 0.0f;
            preview_fade_direction_ = 0;
            return;
        }

        const std::filesystem::path audio_path = path_utils::join_utf8(song.song.directory, song.song.meta.audio_file);
        const managed_content_storage::managed_file_read_result managed_audio =
            managed_content_storage::read_managed_file(audio_path);
        if (managed_audio.managed) {
            if (!managed_audio.success || !audio.request_preview_load_from_memory(managed_audio.bytes)) {
                preview_song_id_.clear();
                preview_volume_ = 0.0f;
                preview_fade_direction_ = 0;
                return;
            }

            preview_volume_ = 0.0f;
            preview_song_id_ = song.song.meta.song_id;
            active_preview_song_.reset();
            preview_fade_direction_ = 0;
            preview_load_pending_ = true;
            preview_load_song_ = song.song;
            pending_preview_song_.reset();
            return;
        }
        audio_source = path_utils::to_utf8(audio_path);
    }

    if (!audio.request_preview_load(audio_source)) {
        preview_song_id_.clear();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        return;
    }

    preview_volume_ = 0.0f;
    preview_song_id_ = song.song.meta.song_id;
    active_preview_song_.reset();
    preview_fade_direction_ = 0;
    preview_load_pending_ = true;
    preview_load_song_ = song.song;
    pending_preview_song_.reset();
}

}  // namespace song_select
