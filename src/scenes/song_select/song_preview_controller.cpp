#include "song_select/song_preview_controller.h"

#include <algorithm>
#include <filesystem>

#include "audio_manager.h"
#include "game_settings.h"
#include "path_utils.h"

namespace {

constexpr float kPreviewFadeSpeed = 2.4f;
constexpr float kPreviewMaxVolume = 0.55f;

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
    if (preview_fade_direction_ < 0) {
        preview_volume_ = std::max(0.0f, preview_volume_ - dt * kPreviewFadeSpeed);
        audio_manager::instance().set_preview_volume(preview_volume_ * g_settings.bgm_volume);
        if (preview_volume_ <= 0.0f) {
            audio_manager::instance().stop_preview();
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
        audio_manager::instance().set_preview_volume(preview_volume_ * g_settings.bgm_volume);
        if (preview_volume_ >= kPreviewMaxVolume) {
            preview_fade_direction_ = 0;
        }
    } else if (active_preview_song_.has_value() && audio_manager::instance().is_preview_loaded()) {
        const double remaining = audio_manager::instance().get_preview_length_seconds() -
                                 audio_manager::instance().get_preview_position_seconds();
        if (remaining <= 1.0) {
            preview_fade_direction_ = -1;
            pending_preview_song_ = *active_preview_song_;
        }
    }
}

void preview_controller::stop() {
    audio_manager::instance().unload_preview();
    preview_song_id_.clear();
    jacket_song_id_.clear();
    pending_preview_song_.reset();
    active_preview_song_.reset();
    preview_volume_ = 0.0f;
    preview_fade_direction_ = 0;
    unload_jacket();
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
    if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
        return;
    }

    const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
    jacket_texture_ = LoadTexture(jacket_path_utf8.c_str());
    jacket_loaded_ = jacket_texture_.id != 0;
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
    const std::filesystem::path audio_path = path_utils::join_utf8(song.song.directory, song.song.meta.audio_file);
    audio.load_preview(path_utils::to_utf8(audio_path));
    if (!audio.is_preview_loaded()) {
        preview_song_id_.clear();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        return;
    }

    audio.seek_preview(song.song.meta.preview_start_seconds);
    audio.set_preview_volume(0.0f);
    audio.play_preview(false);
    preview_volume_ = 0.0f;
    preview_song_id_ = song.song.meta.song_id;
    active_preview_song_ = song.song;
    preview_fade_direction_ = 1;
    pending_preview_song_.reset();
}

}  // namespace song_select
