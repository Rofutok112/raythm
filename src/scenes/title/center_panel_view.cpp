#include "title/center_panel_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

constexpr float kChartButtonHeight = 54.0f;
constexpr float kChartButtonGap = 8.0f;

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

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    return key_count == 6 ? theme.rank_c : theme.rank_b;
}

std::string format_bpm_range(float min_bpm, float max_bpm) {
    if (min_bpm <= 0.0f && max_bpm <= 0.0f) {
        return "-";
    }
    if (std::fabs(max_bpm - min_bpm) < 0.05f) {
        return TextFormat("%.0f", min_bpm);
    }
    return TextFormat("%.0f-%.0f", min_bpm, max_bpm);
}

}  // namespace

namespace title_center_view {

float chart_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kChartButtonHeight + kChartButtonGap) - kChartButtonGap;
}

float max_chart_scroll(Rectangle area, int count) {
    return std::max(0.0f, chart_list_content_height(count) - area.height);
}

Rectangle chart_button_rect(Rectangle area, int index, float scroll_y) {
    return {
        area.x,
        area.y + static_cast<float>(index) * (kChartButtonHeight + kChartButtonGap) - scroll_y,
        area.width,
        kChartButtonHeight
    };
}

int hit_test_chart(Rectangle area, float scroll_y, Vector2 point, int count) {
    if (!CheckCollisionPointRec(point, area)) {
        return -1;
    }
    for (int index = 0; index < count; ++index) {
        if (CheckCollisionPointRec(point, chart_button_rect(area, index, scroll_y))) {
            return index;
        }
    }
    return -1;
}

void draw(const song_select::state& state,
          const song_select::preview_controller& preview_controller,
          const song_select::song_entry* song,
          const song_select::chart_option* chart,
          std::span<const song_select::chart_option* const> filtered,
          const draw_config& config) {
    const auto& t = *g_theme;

    if (song == nullptr) {
        ui::draw_text_in_rect("No songs found yet.", 34,
                              {config.main_column_rect.x, config.main_column_rect.y + 120.0f,
                               config.main_column_rect.width, 40.0f},
                              with_alpha(t.text, config.alpha));
        return;
    }

    const Rectangle jacket_frame = {
        config.jacket_rect.x - 8.0f,
        config.jacket_rect.y - 8.0f,
        config.jacket_rect.width + 16.0f,
        config.jacket_rect.height + 16.0f
    };
    const Rectangle jacket_inner = {
        config.jacket_rect.x + 2.0f,
        config.jacket_rect.y + 2.0f,
        config.jacket_rect.width - 4.0f,
        config.jacket_rect.height - 4.0f
    };

    DrawRectangleLinesEx(jacket_frame, 1.5f,
                         with_alpha(t.border_image, static_cast<unsigned char>(190.0f * config.play_t)));
    if (preview_controller.jacket_loaded()) {
        const Texture2D& jacket = preview_controller.jacket_texture();
        DrawTexturePro(jacket,
                       {0.0f, 0.0f, static_cast<float>(jacket.width), static_cast<float>(jacket.height)},
                       jacket_inner, {0.0f, 0.0f}, 0.0f,
                       with_alpha(WHITE, config.alpha));
    } else {
        ui::draw_text_in_rect("JACKET", 26, jacket_inner, with_alpha(t.text_muted, config.alpha));
    }

    const Rectangle title_rect = {
        config.main_column_rect.x + 12.0f,
        config.main_column_rect.y + 14.0f,
        config.main_column_rect.width - 24.0f,
        26.0f
    };
    const Rectangle artist_rect = {
        config.main_column_rect.x + 12.0f,
        config.main_column_rect.y + 46.0f,
        config.main_column_rect.width - 24.0f,
        20.0f
    };
    draw_marquee_text(song->song.meta.title.c_str(),
                      title_rect,
                      28, with_alpha(t.text, config.alpha), config.now);
    draw_marquee_text(song->song.meta.artist.c_str(),
                      artist_rect,
                      18, with_alpha(t.text_secondary, config.alpha), config.now);
    if (chart != nullptr) {
        ui::draw_text_in_rect(TextFormat("%s  Lv.%.1f", chart->meta.difficulty.c_str(), chart->meta.level),
                              18,
                              {config.chart_detail_rect.x, config.chart_detail_rect.y + 4.0f,
                               config.chart_detail_rect.width, 18.0f},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%s   %d Notes", key_mode_label(chart->meta.key_count).c_str(),
                                         chart->note_count),
                              14,
                              {config.chart_detail_rect.x, config.chart_detail_rect.y + 32.0f,
                               config.chart_detail_rect.width, 16.0f},
                              with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("BPM %s", format_bpm_range(chart->min_bpm, chart->max_bpm).c_str()),
                              14,
                              {config.chart_detail_rect.x, config.chart_detail_rect.y + 54.0f,
                               config.chart_detail_rect.width, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        draw_marquee_text(chart->meta.chart_author.c_str(),
                          {config.chart_detail_rect.x, config.chart_detail_rect.y + 76.0f,
                           config.chart_detail_rect.width, 14.0f},
                          14, with_alpha(t.text_muted, config.alpha), config.now);
    }

    ui::scoped_clip_rect clip(config.chart_buttons_rect);
    for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
        const song_select::chart_option& item = *filtered[static_cast<size_t>(i)];
        const Rectangle row = chart_button_rect(config.chart_buttons_rect, i, state.chart_scroll_y);
        if (row.y + row.height < config.chart_buttons_rect.y - 4.0f ||
            row.y > config.chart_buttons_rect.y + config.chart_buttons_rect.height + 4.0f) {
            continue;
        }

        const bool selected = i == state.difficulty_index;
        const bool hovered = ui::is_hovered(row);
        const unsigned char row_alpha = selected ? config.selected_row_alpha
            : hovered ? config.hover_row_alpha
                      : config.normal_row_alpha;
        DrawRectangleRec(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
        DrawRectangleLinesEx(
            row, 1.0f,
            with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t)));
        ui::draw_text_in_rect(item.meta.difficulty.c_str(), 16,
                              {row.x + 12.0f, row.y + 9.0f, row.width - 56.0f, 16.0f},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("Lv.%.1f", item.meta.level), 13,
                              {row.x + 12.0f, row.y + 28.0f, row.width - 56.0f, 14.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(key_mode_label(item.meta.key_count).c_str(), 13,
                              {row.x + row.width - 34.0f, row.y + 10.0f, 22.0f, 14.0f},
                              with_alpha(key_mode_color(item.meta.key_count), config.alpha), ui::text_align::right);
        if (item.best_local_rank.has_value()) {
            ui::draw_text_in_rect(rank_label(*item.best_local_rank), 12,
                                  {row.x + row.width - 34.0f, row.y + 28.0f, 22.0f, 12.0f},
                                  with_alpha(rank_color(*item.best_local_rank), config.alpha), ui::text_align::right);
        }
    }
}

}  // namespace title_center_view
