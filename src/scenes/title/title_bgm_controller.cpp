#include "title/title_bgm_controller.h"

#include <algorithm>
#include <utility>

#include "audio_manager.h"
#include "game_settings.h"

namespace {

constexpr float kTitleBgmFadeSpeed = 0.07f;

float approach(float current, float target, float step) {
    if (current < target) {
        return std::min(current + step, target);
    }
    return std::max(current - step, target);
}

}  // namespace

void title_bgm_controller::configure(std::string intro_path, std::string loop_path) {
    intro_path_ = std::move(intro_path);
    loop_path_ = std::move(loop_path);
}

void title_bgm_controller::on_enter() {
    phase_ = phase::stopped;
    suspended_ = false;
    paused_for_suspend_ = false;
    current_volume_ = std::clamp(g_settings.bgm_volume, 0.0f, 1.0f);
    target_volume_ = current_volume_;
    if (intro_path_.empty() || !audio_manager::instance().load_bgm(intro_path_)) {
        return;
    }

    audio_manager::instance().set_bgm_volume(current_volume_);
    audio_manager::instance().play_bgm(true);
    phase_ = phase::intro;
}

void title_bgm_controller::on_exit() {
    phase_ = phase::stopped;
    suspended_ = false;
    paused_for_suspend_ = false;
    audio_manager::instance().stop_bgm();
}

void title_bgm_controller::update() {
    if (phase_ == phase::stopped || !audio_manager::instance().is_bgm_loaded()) {
        return;
    }

    const float desired_volume = suspended_ ? 0.0f : std::clamp(g_settings.bgm_volume, 0.0f, 1.0f);
    target_volume_ = desired_volume;
    current_volume_ = approach(current_volume_, target_volume_, kTitleBgmFadeSpeed);
    audio_manager::instance().set_bgm_volume(current_volume_);

    if (suspended_) {
        if (current_volume_ <= 0.001f && audio_manager::instance().is_bgm_playing()) {
            audio_manager::instance().pause_bgm();
            paused_for_suspend_ = true;
        }
        return;
    }

    if (paused_for_suspend_ && !audio_manager::instance().is_bgm_playing()) {
        audio_manager::instance().play_bgm(false);
        paused_for_suspend_ = false;
    }

    if (audio_manager::instance().is_bgm_playing()) {
        return;
    }

    if (phase_ == phase::intro) {
        if (!loop_path_.empty() && audio_manager::instance().load_bgm(loop_path_)) {
            audio_manager::instance().set_bgm_volume(current_volume_);
            audio_manager::instance().play_bgm(true);
            phase_ = phase::loop;
            return;
        }
        phase_ = phase::stopped;
        return;
    }

    audio_manager::instance().seek_bgm(0.0);
    audio_manager::instance().set_bgm_volume(current_volume_);
    audio_manager::instance().play_bgm(true);
}

void title_bgm_controller::suspend() {
    if (phase_ == phase::stopped) {
        return;
    }
    suspended_ = true;
}

void title_bgm_controller::resume() {
    if (phase_ == phase::stopped) {
        return;
    }
    suspended_ = false;
    target_volume_ = std::clamp(g_settings.bgm_volume, 0.0f, 1.0f);
    if (paused_for_suspend_ && !audio_manager::instance().is_bgm_playing()) {
        audio_manager::instance().set_bgm_volume(current_volume_);
        audio_manager::instance().play_bgm(false);
        paused_for_suspend_ = false;
    }
}

void title_bgm_controller::restart() {
    suspended_ = false;
    paused_for_suspend_ = false;
    current_volume_ = std::clamp(g_settings.bgm_volume, 0.0f, 1.0f);
    target_volume_ = current_volume_;

    audio_manager::instance().stop_bgm();
    if (intro_path_.empty() || !audio_manager::instance().load_bgm(intro_path_)) {
        phase_ = phase::stopped;
        return;
    }

    audio_manager::instance().set_bgm_volume(current_volume_);
    audio_manager::instance().play_bgm(true);
    phase_ = phase::intro;
}

title_bgm_controller::phase title_bgm_controller::current_phase() const {
    return phase_;
}
