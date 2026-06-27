#pragma once

#include "raylib.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui/icons/raythm_icons.h"

namespace title_preview_transport {

struct toggle_button_options {
    bool hover_border_accent = false;
};

inline ui::button_state toggle_button(Rectangle rect, bool playing, unsigned char alpha,
                                      toggle_button_options options = {}) {
    const auto& t = *g_theme;
    const Color active_fill = lerp_color(t.section, t.accent, 0.34f);
    return ui::icon_button(rect, playing ? raythm_icons::draw_pause : raythm_icons::draw_play, {
        .border_width = 1.3f,
        .bg = with_alpha(playing ? active_fill : t.section, static_cast<unsigned char>(alpha * 0.72f)),
        .bg_hover = with_alpha(playing ? active_fill : t.section, alpha),
        .icon_color = with_alpha(playing ? t.text : t.text_secondary, alpha),
        .icon_hover_color = with_alpha(t.text, alpha),
        .border_color = with_alpha(playing ? t.accent : t.border_light, alpha),
        .border_hover_color = options.hover_border_accent ? with_alpha(t.accent, alpha) : Color{},
        .icon_inset = 13.0f,
        .icon_stroke_width = 3.0f,
        .border_alpha_tracks_fill = false,
    });
}

inline ui::button_state skip_button(Rectangle rect, bool next, unsigned char alpha) {
    const auto& t = *g_theme;
    return ui::icon_button(rect, next ? raythm_icons::draw_skip_forward : raythm_icons::draw_skip_back, {
        .border_width = 1.2f,
        .bg = with_alpha(t.section, static_cast<unsigned char>(alpha * 0.64f)),
        .bg_hover = with_alpha(t.section, alpha),
        .icon_color = with_alpha(t.text_secondary, alpha),
        .icon_hover_color = with_alpha(t.text, alpha),
        .border_color = with_alpha(t.border_light, alpha),
        .icon_inset = 13.0f,
        .icon_stroke_width = 3.0f,
        .border_alpha_tracks_fill = false,
    });
}

}  // namespace title_preview_transport
