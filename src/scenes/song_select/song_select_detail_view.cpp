#include "song_select/song_select_detail_view.h"

#include <cmath>
#include <string>

#include "scene_common.h"
#include "shared/content_status_badge.h"
#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace {

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    return key_count == 6 ? theme.rank_c : theme.rank_b;
}

const char* rank_label(rank value) {
    switch (value) {
        case rank::ss: return "SS";
        case rank::s: return "S";
        case rank::aa: return "AA";
        case rank::a: return "A";
        case rank::b: return "B";
        case rank::c: return "C";
        case rank::f: return "F";
    }
    return "?";
}

Color rank_color(rank value) {
    const auto& theme = *g_theme;
    switch (value) {
        case rank::ss: return theme.rank_ss;
        case rank::s: return theme.rank_s;
        case rank::aa: return theme.rank_aa;
        case rank::a: return theme.rank_a;
        case rank::b: return theme.rank_b;
        case rank::c: return theme.rank_c;
        case rank::f: return theme.rank_f;
    }
    return theme.text_secondary;
}

std::string format_offset_label(int offset_ms) {
    return (offset_ms > 0 ? "+" : "") + std::to_string(offset_ms) + "ms";
}

std::string format_recent_offset_label(float offset_ms) {
    const int rounded_offset = static_cast<int>(std::lround(offset_ms));
    if (rounded_offset > 0) {
        return std::to_string(rounded_offset) + "ms";
    }
    if (rounded_offset < 0) {
        return std::to_string(rounded_offset) + "ms";
    }
    return "0ms";
}

}  // namespace

namespace song_select {

void draw_frame(const state& state) {
    const auto& theme = *g_theme;
    ui::draw_panel(layout::kLeftPanelRect);
    ui::draw_panel(layout::kSongListRect);
    ui::draw_text_in_rect("SONG SELECT", 30, layout::kSceneTitleRect, theme.text, ui::text_align::left);
    const char* login_label = state.auth.logged_in ? "ACCOUNT" : "LOGIN";
    ui::draw_button_colored(layout::kLoginButtonRect, login_label, 20,
                            theme.row, theme.row_hover, theme.text);
    ui::draw_button_colored(layout::kSettingsButtonRect, "SETTINGS", 20,
                            theme.row, theme.row_hover, theme.text);
}

void draw_empty_state(const state& state) {
    const auto& theme = *g_theme;
    ui::draw_text_in_rect("No songs found", 36,
                          ui::place(layout::kLeftPanelRect, 320.0f, 40.0f,
                                    ui::anchor::center, ui::anchor::center,
                                    {0.0f, -20.0f}),
                          theme.text);
    if (!state.load_errors.empty()) {
        ui::draw_text_in_rect(state.load_errors.front().c_str(), 22,
                              ui::place(layout::kLeftPanelRect, 620.0f, 28.0f,
                                        ui::anchor::center, ui::anchor::center,
                                        {0.0f, 28.0f}),
                              theme.error);
    }
}

void draw_song_details(const state& state, const preview_controller& preview_controller) {
    const song_entry* song = selected_song(state);
    if (song == nullptr) {
        return;
    }

    const chart_option* selected_chart = selected_chart_for(state, filtered_charts_for_selected_song(state));
    const auto& theme = *g_theme;
    const float content_anim = 1.0f - state.song_change_anim_t;
    const float content_offset_x = 18.0f * state.song_change_anim_t;
    const unsigned char content_alpha = static_cast<unsigned char>(145.0f + 110.0f * content_anim);
    const float chart_anim = 1.0f - state.chart_change_anim_t;
    const float chart_offset_x = 14.0f * state.chart_change_anim_t;
    const unsigned char chart_alpha = static_cast<unsigned char>(120.0f + 135.0f * chart_anim);
    const int local_offset_ms = selected_chart != nullptr ? selected_chart->local_note_offset_ms : 0;
    const bool has_recent_result = state.recent_result_offset.has_value() &&
                                   state.recent_result_offset->song_id == song->song.meta.song_id &&
                                   selected_chart != nullptr &&
                                   state.recent_result_offset->chart_id == selected_chart->meta.chart_id;

    ui::draw_section(layout::kJacketRect);
    if (preview_controller.jacket_loaded()) {
        const Texture2D& jacket = preview_controller.jacket_texture();
        const Rectangle source = {0.0f, 0.0f, static_cast<float>(jacket.width), static_cast<float>(jacket.height)};
        DrawTexturePro(jacket, source, layout::kJacketRect, Vector2{0.0f, 0.0f}, 0.0f,
                       Color{255, 255, 255, content_alpha});
    } else {
        ui::draw_text_in_rect("JACKET", 30, layout::kJacketRect, with_alpha(theme.text_muted, content_alpha));
    }
    ui::draw_rect_lines(layout::kJacketRect, 2.0f, theme.border_image);

    const float detail_x = layout::kDetailColumnX;
    const float detail_max_width = layout::kDetailColumnWidth;
    const double now = GetTime();
    draw_marquee_text(song->song.meta.title.c_str(), detail_x + content_offset_x, layout::kJacketRect.y + 4.0f, 40,
                      with_alpha(theme.text, content_alpha), detail_max_width - 118.0f, now);
    content_status_badge::draw(
        {detail_x + detail_max_width - 108.0f + content_offset_x,
         layout::kJacketRect.y + 13.0f, 100.0f, 24.0f},
        song->status, content_alpha, 12);
    draw_marquee_text(song->song.meta.artist.c_str(), detail_x + content_offset_x, layout::kJacketRect.y + 56.0f, 28,
                      with_alpha(theme.text_secondary, content_alpha), detail_max_width, now);
    ui::draw_text_f(TextFormat("BPM %.0f", song->song.meta.base_bpm), detail_x + content_offset_x,
                    layout::kJacketRect.y + 100.0f, 24, with_alpha(theme.text_muted, content_alpha));
    if (selected_chart != nullptr) {
        const float key_x = detail_x + content_offset_x + chart_offset_x;
        const float key_y = layout::kJacketRect.y + 126.0f;
        const float difficulty_x = key_x + 54.0f;
        const float difficulty_width = detail_max_width - 54.0f;
        const std::string difficulty_label =
            selected_chart->meta.difficulty + "  Lv." + TextFormat("%.1f", selected_chart->meta.level);
        ui::draw_text_f(key_mode_label(selected_chart->meta.key_count).c_str(), key_x, key_y, 28,
                        with_alpha(key_mode_color(selected_chart->meta.key_count), chart_alpha));
        draw_marquee_text(difficulty_label.c_str(), difficulty_x, key_y, 28,
                          with_alpha(theme.text, chart_alpha), difficulty_width, now);
        content_status_badge::draw(
            {difficulty_x, layout::kJacketRect.y + 158.0f, 100.0f, 22.0f},
            selected_chart->status, chart_alpha, 11);
        draw_marquee_text(selected_chart->meta.chart_author.c_str(), key_x, layout::kJacketRect.y + 194.0f, 20,
                          with_alpha(theme.text_muted, chart_alpha), detail_max_width - 94.0f, now);
        if (selected_chart->best_local_rank.has_value()) {
            const Rectangle rank_rect = {
                detail_x + detail_max_width - 74.0f + content_offset_x + chart_offset_x,
                layout::kJacketRect.y + 190.0f,
                64.0f,
                28.0f
            };
            ui::draw_rect_f(rank_rect, with_alpha(theme.section, chart_alpha));
            ui::draw_rect_lines(rank_rect, 1.5f, with_alpha(theme.border_light, chart_alpha));
            ui::draw_text_in_rect(rank_label(*selected_chart->best_local_rank), 18, rank_rect,
                                  with_alpha(rank_color(*selected_chart->best_local_rank), chart_alpha), ui::text_align::center);
        }
    }

    const std::string local_label = format_offset_label(local_offset_ms);
    ui::draw_text_in_rect("Local Offset", 20, layout::kLocalOffsetLabelRect,
                          with_alpha(theme.text, chart_alpha), ui::text_align::left);

    const Rectangle controls = layout::kLocalOffsetControlsRect;
    const Rectangle value_rect = {
        controls.x + chart_offset_x,
        controls.y,
        120.0f,
        controls.height
    };
    ui::draw_text_in_rect(local_label.c_str(), 26, value_rect,
                          with_alpha(theme.accent, chart_alpha), ui::text_align::left);

    ui::draw_button_colored(layout::local_offset_double_left_rect(), "<<", 18,
                            theme.row, theme.row_hover, theme.text);
    ui::draw_button_colored(layout::local_offset_left_rect(), "<", 18,
                            theme.row, theme.row_hover, theme.text);
    ui::draw_button_colored(layout::local_offset_right_rect(), ">", 18,
                            theme.row, theme.row_hover, theme.text);
    ui::draw_button_colored(layout::local_offset_double_right_rect(), ">>", 18,
                            theme.row, theme.row_hover, theme.text);

    const std::string auto_apply_label = has_recent_result
        ? format_recent_offset_label(state.recent_result_offset->avg_offset_ms)
        : "-";
    ui::draw_button_colored(
        layout::auto_apply_button_rect(), auto_apply_label.c_str(), 18,
        has_recent_result ? with_alpha(theme.row, chart_alpha) : with_alpha(theme.row, 170),
        has_recent_result ? with_alpha(theme.row_hover, chart_alpha) : with_alpha(theme.row, 170),
        has_recent_result ? with_alpha(theme.text, chart_alpha) : with_alpha(theme.text_muted, 210));
}

void draw_busy_overlay(const std::string& message) {
    const auto& theme = *g_theme;
    ui::draw_fullscreen_overlay(Color{0, 0, 0, 120});
    const Rectangle panel = ui::place(layout::kScreenRect, 420.0f, 96.0f,
                                      ui::anchor::center, ui::anchor::center);
    ui::draw_panel(panel);
    ui::draw_text_in_rect(message.c_str(), 22,
                          {panel.x + 24.0f, panel.y + 18.0f, panel.width - 48.0f, panel.height - 36.0f},
                          theme.text, ui::text_align::center);
}

}  // namespace song_select
