#include "song_select/song_preview_controller.h"

#include <algorithm>

#include "audio_manager.h"
#include "game_settings.h"

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
    select_song(song_media_key_for_song_id(song->song.meta.song_id), song);
}

void preview_controller::select_song(const selection_key& key, const song_entry* song) {
    if (song == nullptr) {
        stop();
        return;
    }

    request_jacket(key, song);
    request_audio(key, song);
}

void preview_controller::request_jacket(const song_entry* song) {
    if (song == nullptr) {
        jacket_loader_.reset();
        return;
    }

    request_jacket(song_media_key_for_song_id(song->song.meta.song_id), song);
}

void preview_controller::request_jacket(const selection_key& key, const song_entry* song) {
    if (song == nullptr) {
        jacket_loader_.reset();
        return;
    }

    jacket_loader_.request(key, song);
}

void preview_controller::request_audio(const song_entry* song) {
    if (song == nullptr) {
        audio_loader_.reset();
        pending_preview_song_.reset();
        active_preview_song_.reset();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        audio_manager::instance().unload_preview();
        return;
    }

    request_audio(song_media_key_for_song_id(song->song.meta.song_id), song);
}

void preview_controller::request_audio(const selection_key& key, const song_entry* song) {
    if (song == nullptr) {
        audio_loader_.reset();
        pending_preview_song_.reset();
        active_preview_song_.reset();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        audio_manager::instance().unload_preview();
        return;
    }

    queue_preview(key, song);
}

void preview_controller::update(float dt, const song_entry* selected_song) {
    selection_key current_key;
    if (selected_song != nullptr) {
        current_key = song_media_key_for_song_id(selected_song->song.meta.song_id);
    }
    update(dt, current_key, selected_song);
}

void preview_controller::update(float dt, const selection_key& current_key, const song_entry* selected_song) {
    jacket_loader_.poll();

    audio_manager& audio = audio_manager::instance();
    const preview_audio_loader::load_event load_event = audio_loader_.update(current_key, selected_song);
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
        audio.unload_preview();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        active_preview_song_.reset();
        pending_preview_song_.reset();
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
                    start_preview(current_key, *selected_song);
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

    resume(song_media_key_for_song_id(song->song.meta.song_id), song);
}

void preview_controller::resume(const selection_key& key, const song_entry* song) {
    if (song == nullptr) {
        stop();
        return;
    }

    const selection_key media_key = song_media_key_for(key);
    jacket_loader_.request(key, song);
    audio_manager& audio = audio_manager::instance();
    if (audio_loader_.target_key() == media_key &&
        audio_loader_.status() == preview_audio_loader::load_status::loading) {
        return;
    }
    if (audio_loader_.target_key() == media_key && audio.is_preview_loaded()) {
        pending_preview_song_.reset();
        active_preview_song_ = song->song;
        preview_fade_direction_ = 0;
        preview_volume_ = std::max(preview_volume_, kPreviewMaxVolume);
        audio.set_preview_volume(g_settings.bgm_volume);
        audio.set_preview_fade_gain(preview_volume_);
        audio.play_preview(false);
        return;
    }

    select_song(key, song);
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
    jacket_loader_.reset();
    pending_preview_song_.reset();
    active_preview_song_.reset();
    preview_volume_ = 0.0f;
    preview_fade_direction_ = 0;
}

bool preview_controller::is_audio_active() const {
    audio_manager& audio = audio_manager::instance();
    return audio.is_preview_loaded() || audio.is_preview_playing();
}

bool preview_controller::is_playing() const {
    return audio_manager::instance().is_preview_playing();
}

bool preview_controller::is_loading() const {
    return audio_loader_.loading_or_preparing();
}

preview_audio_loader::snapshot preview_controller::audio_snapshot() const {
    return audio_loader_.current();
}

jacket_loader::snapshot preview_controller::jacket_snapshot() const {
    return jacket_loader_.current();
}

void preview_controller::queue_preview(const song_entry* song) {
    if (song == nullptr) {
        return;
    }
    queue_preview(song_media_key_for_song_id(song->song.meta.song_id), song);
}

void preview_controller::queue_preview(const selection_key& key, const song_entry* song) {
    if (song == nullptr) {
        return;
    }
    if (audio_loader_.target_song_id() == song->song.meta.song_id) {
        return;
    }

    pending_preview_song_ = song->song;
    start_preview(key, *song);
}

void preview_controller::start_preview(const selection_key& key, const song_entry& song) {
    audio_manager& audio = audio_manager::instance();
    audio.stop_preview();

    if (!audio_loader_.request(key, song)) {
        audio.unload_preview();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        active_preview_song_.reset();
        pending_preview_song_.reset();
        return;
    }

    preview_volume_ = 0.0f;
    active_preview_song_.reset();
    preview_fade_direction_ = 0;
    pending_preview_song_.reset();
}

}  // namespace song_select
