#include "shared/scene_fade.h"

scene_fade::scene_fade(direction mode, float duration_seconds, float max_alpha) {
    restart(mode, duration_seconds, max_alpha);
}

void scene_fade::restart(direction mode, float duration_seconds, float max_alpha) {
    mode_ = mode;
    duration_seconds_ = std::max(0.0f, duration_seconds);
    elapsed_seconds_ = 0.0f;
    max_alpha_ = std::clamp(max_alpha, 0.0f, 1.0f);
}

void scene_fade::update(float dt) {
    elapsed_seconds_ = std::min(duration_seconds_, elapsed_seconds_ + std::max(0.0f, dt));
}

bool scene_fade::active() const {
    return alpha_scale() > 0.0f;
}

bool scene_fade::complete() const {
    return elapsed_seconds_ >= duration_seconds_;
}

float scene_fade::progress() const {
    if (duration_seconds_ <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(elapsed_seconds_ / duration_seconds_, 0.0f, 1.0f);
}

float scene_fade::alpha_scale() const {
    const float fade_progress = progress();
    if (mode_ == direction::in) {
        return std::max(0.0f, (1.0f - fade_progress) * max_alpha_);
    }
    return std::min(1.0f, fade_progress * max_alpha_);
}

Color scene_fade::overlay_color() const {
    return {
        0,
        0,
        0,
        static_cast<unsigned char>(std::clamp(alpha_scale(), 0.0f, 1.0f) * 255.0f)
    };
}

void scene_fade::draw() const {
    if (!active()) {
        return;
    }
    ui::draw_fullscreen_overlay(overlay_color());
}
