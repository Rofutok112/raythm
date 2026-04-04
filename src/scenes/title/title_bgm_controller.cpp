#include "title/title_bgm_controller.h"

#include <utility>

#include "audio_manager.h"

void title_bgm_controller::configure(std::string intro_path, std::string loop_path) {
    intro_path_ = std::move(intro_path);
    loop_path_ = std::move(loop_path);
}

void title_bgm_controller::on_enter() {
    phase_ = phase::stopped;
    if (intro_path_.empty() || !audio_manager::instance().load_bgm(intro_path_)) {
        return;
    }

    audio_manager::instance().play_bgm(true);
    phase_ = phase::intro;
}

void title_bgm_controller::on_exit() {
    phase_ = phase::stopped;
    audio_manager::instance().stop_bgm();
}

void title_bgm_controller::update() {
    if (phase_ == phase::stopped || !audio_manager::instance().is_bgm_loaded()) {
        return;
    }
    if (audio_manager::instance().is_bgm_playing()) {
        return;
    }

    if (phase_ == phase::intro) {
        if (!loop_path_.empty() && audio_manager::instance().load_bgm(loop_path_)) {
            audio_manager::instance().play_bgm(true);
            phase_ = phase::loop;
            return;
        }
        phase_ = phase::stopped;
        return;
    }

    audio_manager::instance().seek_bgm(0.0);
    audio_manager::instance().play_bgm(true);
}

title_bgm_controller::phase title_bgm_controller::current_phase() const {
    return phase_;
}
