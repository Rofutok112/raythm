#include "multiplayer_result/multiplayer_result_score_list_view.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "multiplayer_result/multiplayer_result_widgets.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace multiplayer_result::score_list {
namespace {

constexpr Rectangle kListViewportRect{1258.0f, 246.0f, 590.0f, 754.0f};
constexpr Rectangle kListHeaderRect{1258.0f, 192.0f, 590.0f, 36.0f};
constexpr Rectangle kListScrollbarRect{kListViewportRect.x + kListViewportRect.width + 8.0f,
                                       kListViewportRect.y,
                                       10.0f,
                                       kListViewportRect.height};
constexpr float kRowHeight = 88.0f;
constexpr float kRowGap = 10.0f;

struct row_layout {
    Rectangle rank;
    Rectangle avatar;
    Rectangle name;
    Rectangle self_chip;
    Rectangle score;
    Rectangle accuracy;
    Rectangle combo;
    Rectangle accent;
};

struct score_row_descriptor {
    Rectangle rect{};
    const play_multiplayer_score_row* score = nullptr;
    int rank_index = 0;
    bool self = false;
    bool selected = false;
};

struct score_stat_column {
    const char* label;
    Rectangle rect;
};

struct score_stat_value {
    Rectangle rect;
    std::string text;
    int font_size;
    Color color;
};

std::string score_key_for(const play_multiplayer_score_row& score) {
    return !score.user_id.empty() ? score.user_id : score.display_name;
}

constexpr row_layout row_layout_for(Rectangle row) {
    const Rectangle content = ui::inset(row, ui::edge_insets{0.0f, 28.0f, 0.0f, 18.0f});
    const ui::rect_pair rank_split = ui::split_columns(content, 50.0f, 6.0f);
    const ui::rect_pair avatar_split = ui::split_columns(rank_split.second, 48.0f, 16.0f);
    const ui::rect_pair name_split = ui::split_columns(avatar_split.second, 96.0f, 42.0f);
    const ui::rect_pair score_split = ui::split_columns(name_split.second, 104.0f, 12.0f);
    const ui::rect_pair accuracy_split = ui::split_columns(score_split.second, 78.0f, 22.0f);
    const ui::rect_pair combo_split = ui::split_columns(accuracy_split.second, 70.0f);
    return {
        rank_split.first,
        {avatar_split.first.x, row.y + 20.0f, avatar_split.first.width, 48.0f},
        {name_split.first.x, row.y + 9.0f, name_split.first.width, 34.0f},
        {name_split.first.x, row.y + 50.0f, 56.0f, 22.0f},
        score_split.first,
        accuracy_split.first,
        combo_split.first,
        {row.x, row.y, 4.0f, row.height},
    };
}

float content_height(int row_count) {
    return ui::vertical_list_content_height(row_count, kRowHeight, kRowGap, kListViewportRect.height);
}

std::array<score_stat_column, 3> score_stat_columns_for(const row_layout& layout) {
    return {{
        {"SCORE", layout.score},
        {"ACC", layout.accuracy},
        {"COMBO", layout.combo},
    }};
}

score_row_descriptor score_row_descriptor_for(const std::vector<play_multiplayer_score_row>& scores,
                                              int index,
                                              const std::string& self_user_id,
                                              const std::string& selected_score_key,
                                              float scroll_y) {
    const play_multiplayer_score_row& score = scores[static_cast<size_t>(index)];
    return {
        .rect = ui::vertical_list_row_rect(kListViewportRect, index, kRowHeight, kRowGap, scroll_y),
        .score = &score,
        .rank_index = index,
        .self = !self_user_id.empty() && score.user_id == self_user_id,
        .selected = score_key_for(score) == selected_score_key,
    };
}

std::optional<std::string> score_row_key_at(const std::vector<play_multiplayer_score_row>& scores,
                                            Vector2 point,
                                            float scroll_y) {
    if (!ui::contains_point(kListViewportRect, point)) {
        return std::nullopt;
    }
    const int index = ui::vertical_list_index_at_y(point.y, kListViewportRect, kRowHeight, kRowGap, scroll_y);
    if (index < 0 || index >= static_cast<int>(scores.size())) {
        return std::nullopt;
    }
    return score_key_for(scores[static_cast<size_t>(index)]);
}

std::array<score_stat_value, 3> score_stat_values_for(const row_layout& layout,
                                                      const play_multiplayer_score_row& score) {
    return {{
        {layout.score, widgets::format_score(score.score), 22, g_theme->text},
        {layout.accuracy, TextFormat("%.1f%%", score.accuracy), 20, g_theme->fast},
        {layout.combo, std::to_string(score.combo), 20, g_theme->text_secondary},
    }};
}

void draw_chip(Rectangle rect, const char* text, Color color, int font_size = 18) {
    ui::surface(rect, with_alpha(color, 34), with_alpha(color, 180), 1.5f);
    ui::draw_text_in_rect(text, font_size, rect, color, ui::text_align::center);
}

void draw_header(Rectangle rect) {
    const row_layout header_layout = row_layout_for(rect);
    for (const score_stat_column& column : score_stat_columns_for(header_layout)) {
        ui::draw_text_in_rect(column.label, 16, column.rect, g_theme->text_muted, ui::text_align::right);
    }
}

void draw_row(const score_row_descriptor& row, const std::string& avatar_base_url) {
    const play_multiplayer_score_row& score = *row.score;
    const row_layout layout = row_layout_for(row.rect);
    const Color bg = row.selected
        ? with_alpha(g_theme->row_soft_selected, 245)
        : (row.self ? with_alpha(g_theme->row_selected, 218) : g_theme->row);
    ui::row(row.rect, {
        .border_width = row.selected ? 3.0f : 1.5f,
        .bg = bg,
        .bg_hover = row.selected ? g_theme->row_soft_selected_hover : g_theme->row_hover,
        .border_color = row.selected ? g_theme->border_active : g_theme->border,
        .custom_colors = true,
    });
    ui::draw_text_in_rect(TextFormat("#%d", row.rank_index + 1), 27,
                          layout.rank,
                          widgets::podium_rank_color(row.rank_index), ui::text_align::left);
    widgets::draw_profile_image_slot(layout.avatar,
                                     score.avatar_url,
                                     score.display_name,
                                     avatar_base_url);
    {
        ui::scoped_clip_rect name_clip(layout.name);
        ui::draw_text_in_rect(score.display_name.c_str(), 20, layout.name,
                              score.failed ? g_theme->text_muted : g_theme->text,
                              ui::text_align::left);
    }
    if (row.self) {
        draw_chip(layout.self_chip, "YOU", g_theme->accent, 13);
    }
    for (const score_stat_value& value : score_stat_values_for(layout, score)) {
        ui::draw_text_in_rect(value.text.c_str(), value.font_size, value.rect, value.color, ui::text_align::right);
    }
    const Color accent_color = row.selected
        ? widgets::podium_rank_color(row.rank_index)
        : (score.failed ? g_theme->error : g_theme->success);
    ui::accent_bar(layout.accent,
                   accent_color);
}

}  // namespace

std::string score_key(const play_multiplayer_score_row& score) {
    return score_key_for(score);
}

interaction_result handle_input(const std::vector<play_multiplayer_score_row>& scores,
                                float current_scroll_y,
                                bool current_scrollbar_dragging,
                                float current_scrollbar_drag_offset) {
    interaction_result result;
    const float list_content_height = content_height(static_cast<int>(scores.size()));
    ui::scroll_offset_state scroll_state =
        ui::scroll_offset_state_for(kListViewportRect, list_content_height, current_scroll_y);
    float next_scroll_y = scroll_state.offset;
    bool next_scrollbar_dragging = current_scrollbar_dragging;
    float next_scrollbar_drag_offset = current_scrollbar_drag_offset;

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (ui::contains_point(kListViewportRect, mouse)) {
        scroll_state = ui::wheel_scrolled_offset_state(
            kListViewportRect, mouse, ui::mouse_wheel_move(), list_content_height, next_scroll_y, 46.0f);
        next_scroll_y = scroll_state.offset;
        if (ui::is_mouse_button_released()) {
            result.selected_score_key = score_row_key_at(scores, mouse, next_scroll_y);
        }
    }

    const ui::scrollbar_interaction scroll =
        ui::vertical_scrollbar(kListScrollbarRect, list_content_height, next_scroll_y,
                               next_scrollbar_dragging, next_scrollbar_drag_offset, {
                                   .drag_blocked_by_layer = false,
                               });
    if (scroll.changed) {
        next_scroll_y = scroll.scroll_offset;
    }

    if (next_scroll_y != current_scroll_y) {
        result.scroll_changed = true;
        result.scroll_y = next_scroll_y;
    }
    if (next_scrollbar_dragging != current_scrollbar_dragging ||
        next_scrollbar_drag_offset != current_scrollbar_drag_offset) {
        result.scrollbar_drag_state_changed = true;
        result.scrollbar_dragging = next_scrollbar_dragging;
        result.scrollbar_drag_offset = next_scrollbar_drag_offset;
    }
    return result;
}

void draw(const std::vector<play_multiplayer_score_row>& scores,
          const std::string& self_user_id,
          const std::string& selected_score_key,
          const std::string& avatar_base_url,
          float scroll_y) {
    draw_header(kListHeaderRect);

    const float list_content_height = content_height(static_cast<int>(scores.size()));
    {
        ui::scoped_clip_rect clip(kListViewportRect);
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            scores.size(), kListViewportRect, kRowHeight, kRowGap, scroll_y);
        for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
            const score_row_descriptor row =
                score_row_descriptor_for(scores, i, self_user_id, selected_score_key, scroll_y);
            draw_row(row, avatar_base_url);
        }
    }
    ui::scrollbar(kListScrollbarRect, list_content_height, scroll_y, {
        .track_color = g_theme->scrollbar_track,
        .thumb_color = g_theme->scrollbar_thumb,
        .custom_colors = true,
    });
}

}  // namespace multiplayer_result::score_list
