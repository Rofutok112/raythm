#include "title/title_audio_controller.h"

#include <utility>

#include "audio_manager.h"

void title_audio_controller::configure(std::string intro_path, std::string loop_path) {
    bgm_controller_.configure(std::move(intro_path), std::move(loop_path));
}

void title_audio_controller::on_enter() {
    spectrum_visualizer_.reset();
    bgm_controller_.on_enter();
    current_state_ = {};
}

void title_audio_controller::on_exit() {
    bgm_controller_.on_exit();
    spectrum_visualizer_.reset();
    preview_controller_.stop();
    current_state_ = {};
}

void title_audio_controller::update(title_audio_policy::hub_mode mode,
                                    const song_select::song_entry* selected_song,
                                    float dt) {
    title_audio_policy::resolved_state next_state = resolve_state(mode);
    if (next_state.update_preview) {
        preview_controller_.update(dt, selected_song);
        next_state = resolve_state(mode);
    }

    current_state_ = next_state;
    if (current_state_.music == title_audio_policy::music_source::preview_song) {
        bgm_controller_.suspend();
    } else {
        bgm_controller_.resume();
    }
    bgm_controller_.update();
    spectrum_visualizer_.update(current_state_.spectrum, dt);
}

void title_audio_controller::update_preview_only(const song_select::song_entry* selected_song, float dt) {
    if (selected_song == nullptr) {
        preview_controller_.stop();
    } else {
        preview_controller_.select_song(selected_song);
        preview_controller_.update(dt, selected_song);
    }
    current_state_ = {
        .music = title_audio_policy::music_source::preview_song,
        .spectrum = title_spectrum_visualizer::source::preview,
        .update_preview = selected_song != nullptr,
    };
    bgm_controller_.suspend();
    bgm_controller_.update();
    spectrum_visualizer_.update(current_state_.spectrum, dt);
}

void title_audio_controller::update_multiplayer_preview(const song_select::song_entry* selected_song, float dt) {
    if (selected_song != nullptr) {
        preview_controller_.update(dt, selected_song);
    } else if (preview_controller_.is_audio_active()) {
        preview_controller_.stop();
    }

    const bool preview_active = preview_controller_.is_audio_active();
    current_state_ = {
        .music = preview_active
            ? title_audio_policy::music_source::preview_song
            : title_audio_policy::music_source::theme_song,
        .spectrum = preview_active
            ? title_spectrum_visualizer::source::preview
            : title_spectrum_visualizer::source::bgm,
        .update_preview = selected_song != nullptr,
    };
    bgm_controller_.suspend_immediate();
    bgm_controller_.update();
    spectrum_visualizer_.update(current_state_.spectrum, dt);
}

void title_audio_controller::draw_spectrum(const Rectangle& rect, float alpha_scale) const {
    spectrum_visualizer_.draw(rect, alpha_scale);
}

void title_audio_controller::select_preview_song(const song_select::song_entry* song) {
    preview_controller_.select_song(song);
}

void title_audio_controller::request_preview_audio(const song_select::song_entry* song) {
    preview_controller_.request_audio(song);
}

void title_audio_controller::request_preview_jacket(const song_select::song_entry* song) {
    preview_controller_.request_jacket(song);
}

void title_audio_controller::resume_preview_song(const song_select::song_entry* song) {
    preview_controller_.resume(song);
}

void title_audio_controller::pause_preview() {
    preview_controller_.pause();
}

void title_audio_controller::stop_preview() {
    preview_controller_.stop();
}

void title_audio_controller::toggle_preview_song(const song_select::song_entry* song) {
    if (preview_controller_.is_playing()) {
        preview_controller_.pause();
    } else {
        preview_controller_.resume(song);
    }
}

void title_audio_controller::seek_preview(double seconds) {
    audio_manager::instance().seek_preview(seconds);
}

void title_audio_controller::play_preview_from_current() {
    audio_manager::instance().play_preview(false);
}

title_audio_policy::resolved_state title_audio_controller::current_state() const {
    return current_state_;
}

title_preview_snapshot title_audio_controller::preview_snapshot(const song_select::song_entry* fallback_song) const {
    const audio_manager& audio = audio_manager::instance();
    title_preview_snapshot snapshot;
    snapshot.audio_status = preview_controller_.audio_status();
    snapshot.jacket_status = preview_controller_.jacket_status();
    snapshot.loaded = audio.is_preview_loaded();
    snapshot.loading = snapshot.audio_status == song_select::preview_audio_loader::load_status::loading;
    snapshot.playing = audio.is_preview_playing();
    snapshot.position_seconds = snapshot.loaded ? audio.get_preview_position_seconds() : 0.0;
    snapshot.length_seconds = snapshot.loaded ? audio.get_preview_length_seconds() : 0.0;
    if (snapshot.length_seconds <= 0.0 && fallback_song != nullptr) {
        snapshot.length_seconds = static_cast<double>(fallback_song->song.meta.duration_seconds);
    }
    snapshot.jacket_loaded = snapshot.jacket_status == song_select::jacket_loader::load_status::ready &&
        preview_controller_.jacket_loaded();
    snapshot.jacket_texture = snapshot.jacket_loaded ? &preview_controller_.jacket_texture() : nullptr;
    return snapshot;
}

title_audio_policy::resolved_state title_audio_controller::resolve_state(title_audio_policy::hub_mode mode) const {
    return title_audio_policy::resolve(mode, preview_controller_.is_audio_active());
}
