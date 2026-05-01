#include "title/title_audio_controller.h"

#include <utility>

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

void title_audio_controller::draw_spectrum(const Rectangle& rect, float alpha_scale) const {
    spectrum_visualizer_.draw(rect, alpha_scale);
}

title_audio_policy::resolved_state title_audio_controller::current_state() const {
    return current_state_;
}

song_select::preview_controller& title_audio_controller::preview() {
    return preview_controller_;
}

const song_select::preview_controller& title_audio_controller::preview() const {
    return preview_controller_;
}

title_audio_policy::resolved_state title_audio_controller::resolve_state(title_audio_policy::hub_mode mode) const {
    return title_audio_policy::resolve(mode, preview_controller_.is_audio_active());
}
