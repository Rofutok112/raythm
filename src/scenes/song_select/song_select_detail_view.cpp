#include "song_select/song_select_detail_view.h"

#include <cmath>
#include <string>

#include "scene_common.h"
#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace {

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
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

void draw_frame() {
    const auto& theme = *g_theme;
    ui::draw_panel(layout::kLeftPanelRect);
    ui::draw_panel(layout::kSongListRect);
    ui::draw_text_in_rect("SONG SELECT", 30, layout::kSceneTitleRect, theme.text, ui::text_align::left);
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
    const int local_offset_ms = song->local_note_offset_ms;
    const bool has_recent_result = state.recent_result_offset.has_value() &&
                                   state.recent_result_offset->song_id == song->song.meta.song_id;

    ui::draw_section(layout::kJacketRect);
    if (preview_controller.jacket_loaded()) {
        const Texture2D& jacket = preview_controller.jacket_texture();
        const Rectangle source = {0.0f, 0.0f, static_cast<float>(jacket.width), static_cast<float>(jacket.height)};
        DrawTexturePro(jacket, source, layout::kJacketRect, Vector2{0.0f, 0.0f}, 0.0f,
                       Color{255, 255, 255, content_alpha});
    } else {
        ui::draw_text_in_rect("JACKET", 30, layout::kJacketRect, with_alpha(theme.text_muted, content_alpha));
    }
    DrawRectangleLinesEx(layout::kJacketRect, 2.0f, theme.border_image);

    const float detail_x = layout::kDetailColumnX;
    const float detail_max_width = layout::kDetailColumnWidth;
    const double now = GetTime();
    draw_marquee_text(song->song.meta.title.c_str(), detail_x + content_offset_x, layout::kJacketRect.y + 4.0f, 40,
                      with_alpha(theme.text, content_alpha), detail_max_width, now);
    draw_marquee_text(song->song.meta.artist.c_str(), detail_x + content_offset_x, layout::kJacketRect.y + 56.0f, 28,
                      with_alpha(theme.text_secondary, content_alpha), detail_max_width, now);
    ui::draw_text_f(TextFormat("BPM %.0f", song->song.meta.base_bpm), detail_x + content_offset_x,
                    layout::kJacketRect.y + 100.0f, 24, with_alpha(theme.text_muted, content_alpha));
    if (selected_chart != nullptr) {
        const char* chart_source_label =
            selected_chart->source == content_source::official ? "Official" : "Unofficial";
        const Color chart_source_color =
            selected_chart->source == content_source::official ? theme.success : theme.text_muted;
        ui::draw_text_f(chart_source_label,
                        detail_x + content_offset_x, layout::kJacketRect.y + 126.0f, 18,
                        with_alpha(chart_source_color, content_alpha));
        ui::draw_text_f(TextFormat("%s %s Lv.%.1f", key_mode_label(selected_chart->meta.key_count).c_str(),
                                   selected_chart->meta.difficulty.c_str(), selected_chart->meta.level),
                        detail_x + content_offset_x, layout::kJacketRect.y + 158.0f, 28,
                        with_alpha(theme.text, content_alpha));
        ui::draw_text_f(selected_chart->meta.chart_author.c_str(), detail_x + content_offset_x,
                        layout::kJacketRect.y + 194.0f, 20, with_alpha(theme.text_muted, content_alpha));
    }

    const std::string local_label = format_offset_label(local_offset_ms);
    ui::draw_text_in_rect("Local Offset", 20, layout::kLocalOffsetLabelRect,
                          with_alpha(theme.text, content_alpha), ui::text_align::left);

    const Rectangle controls = layout::kLocalOffsetControlsRect;
    const Rectangle value_rect = {controls.x, controls.y, 120.0f, controls.height};
    ui::draw_text_in_rect(local_label.c_str(), 26, value_rect, with_alpha(theme.accent, content_alpha), ui::text_align::left);

    ui::draw_button_colored(layout::local_offset_double_left_rect(), "<<", 18,
                            with_alpha(theme.row, content_alpha), with_alpha(theme.row_hover, content_alpha),
                            with_alpha(theme.text, content_alpha));
    ui::draw_button_colored(layout::local_offset_left_rect(), "<", 18,
                            with_alpha(theme.row, content_alpha), with_alpha(theme.row_hover, content_alpha),
                            with_alpha(theme.text, content_alpha));
    ui::draw_button_colored(layout::local_offset_right_rect(), ">", 18,
                            with_alpha(theme.row, content_alpha), with_alpha(theme.row_hover, content_alpha),
                            with_alpha(theme.text, content_alpha));
    ui::draw_button_colored(layout::local_offset_double_right_rect(), ">>", 18,
                            with_alpha(theme.row, content_alpha), with_alpha(theme.row_hover, content_alpha),
                            with_alpha(theme.text, content_alpha));

    const std::string auto_apply_label = has_recent_result
        ? format_recent_offset_label(state.recent_result_offset->avg_offset_ms)
        : "-";
    ui::draw_button_colored(
        layout::auto_apply_button_rect(), auto_apply_label.c_str(), 18,
        has_recent_result ? with_alpha(theme.row, content_alpha) : with_alpha(theme.row, 170),
        has_recent_result ? with_alpha(theme.row_hover, content_alpha) : with_alpha(theme.row, 170),
        has_recent_result ? with_alpha(theme.text, content_alpha) : with_alpha(theme.text_muted, 210));
}

void draw_status_message(const state& state) {
    if (state.status_message.empty()) {
        return;
    }

    const auto& theme = *g_theme;
    ui::draw_text_in_rect(state.status_message.c_str(), 18,
                          ui::place(layout::kScreenRect, 520.0f, 24.0f,
                                    ui::anchor::bottom_right, ui::anchor::bottom_right,
                                    {-24.0f, -10.0f}),
                          state.status_message_is_error ? theme.error : theme.success,
                          ui::text_align::right);
}

}  // namespace song_select
