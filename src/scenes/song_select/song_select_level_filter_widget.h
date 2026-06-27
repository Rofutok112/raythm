#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "scene_common.h"
#include "song_select/song_select_level_filter.h"
#include "theme.h"
#include "ui_draw.h"

namespace song_select::level_filter {

struct chip_options {
    int font_size = 11;
    bool body_text = false;
};

struct range_slider_options {
    int chip_font_size = 11;
    bool body_text = false;
};

inline void draw_gradient(Rectangle rect, unsigned char alpha) {
    constexpr int kSegments = 48;
    for (int i = 0; i < kSegments; ++i) {
        const float from_level = kUsefulMaxLevel * (static_cast<float>(i) / kSegments);
        const float to_level = kUsefulMaxLevel * (static_cast<float>(i + 1) / kSegments);
        const float from_t = t_for_level(from_level);
        const float to_t = t_for_level(to_level);
        const Rectangle segment = {
            rect.x + rect.width * from_t,
            rect.y,
            std::max(1.0f, rect.width * (to_t - from_t)),
            rect.height,
        };
        ui::horizontal_gradient(segment,
                                with_alpha(difficulty_level_color(from_level), alpha),
                                with_alpha(difficulty_level_color(to_level), alpha));
    }
    const float useful_end_x = rect.x + rect.width * t_for_level(kUsefulMaxLevel);
    ui::surface_fill({useful_end_x, rect.y, rect.x + rect.width - useful_end_x, rect.height},
                     with_alpha({34, 38, 46, 255}, alpha));
}

inline void draw_chip(Rectangle rect, float level, bool max_chip,
                      unsigned char alpha, chip_options options = {}) {
    const Color tone = max_chip && level >= kMaxLevel - 0.05f
        ? g_theme->text_muted
        : difficulty_level_color(level);
    const std::string text = label(level);
    ui::surface(rect,
                with_alpha(lerp_color(g_theme->bg_alt, tone, 0.18f), alpha),
                with_alpha(tone, alpha),
                1.1f);
    if (options.body_text) {
        ui::draw_body_text_in_rect(text.c_str(), options.font_size, rect, with_alpha(tone, alpha));
    } else {
        ui::draw_text_in_rect(text.c_str(), options.font_size, rect, with_alpha(tone, alpha));
    }
}

inline void draw_range_slider(Rectangle range, float min_level, float max_level,
                              unsigned char alpha, range_slider_options options = {}) {
    if (min_level > max_level) {
        std::swap(min_level, max_level);
    }
    const Rectangle track = track_rect(range);
    ui::surface_fill(track, with_alpha(g_theme->slider_track, alpha));
    draw_gradient(track, static_cast<unsigned char>(alpha / 2));
    draw_chip(chip_rect(range, min_level), min_level, false, alpha, {
        .font_size = options.chip_font_size,
        .body_text = options.body_text,
    });
    draw_chip(chip_rect(range, max_level), max_level, true, alpha, {
        .font_size = options.chip_font_size,
        .body_text = options.body_text,
    });
}

}  // namespace song_select::level_filter
