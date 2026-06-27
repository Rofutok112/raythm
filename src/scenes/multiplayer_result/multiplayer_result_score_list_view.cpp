#include "multiplayer_result/multiplayer_result_score_list_view.h"

#include <algorithm>
#include <cctype>

#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
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
    if (row_count <= 0) {
        return kListViewportRect.height;
    }
    return static_cast<float>(row_count) * kRowHeight +
        static_cast<float>(std::max(0, row_count - 1)) * kRowGap;
}

Color rank_color(int rank_index) {
    if (rank_index == 0) {
        return g_theme->all_perfect;
    }
    if (rank_index == 1) {
        return g_theme->slow;
    }
    if (rank_index == 2) {
        return g_theme->fast;
    }
    return g_theme->text_muted;
}

std::string avatar_label_for(const std::string& name) {
    std::string result;
    result.reserve(2);
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "?" : result;
}

void draw_profile_image_slot(Rectangle rect,
                             const std::string& avatar_url,
                             const std::string& display_name,
                             const std::string& base_url) {
    avatar_texture_cache::draw_avatar(rect,
                                      avatar_url,
                                      avatar_label_for(display_name),
                                      with_alpha(g_theme->section, 235),
                                      g_theme->text_secondary,
                                      13,
                                      base_url);
}

void draw_chip(Rectangle rect, const char* text, Color color, int font_size = 18) {
    ui::surface(rect, with_alpha(color, 34), with_alpha(color, 180), 1.5f);
    ui::draw_text_in_rect(text, font_size, rect, color, ui::text_align::center);
}

std::string format_score(int score) {
    std::string value = std::to_string(std::max(0, score));
    for (int i = static_cast<int>(value.size()) - 3; i > 0; i -= 3) {
        value.insert(static_cast<size_t>(i), ",");
    }
    return value;
}

void draw_header(Rectangle rect) {
    const row_layout header_layout = row_layout_for(rect);
    ui::draw_text_in_rect("SCORE", 16, header_layout.score, g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("ACC", 16, header_layout.accuracy, g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("COMBO", 16, header_layout.combo, g_theme->text_muted, ui::text_align::right);
}

void draw_row(Rectangle row_rect,
              const play_multiplayer_score_row& score,
              int rank_index,
              bool self,
              bool selected,
              const std::string& avatar_base_url) {
    const row_layout layout = row_layout_for(row_rect);
    const Color bg = selected
        ? with_alpha(g_theme->row_soft_selected, 245)
        : (self ? with_alpha(g_theme->row_selected, 218) : g_theme->row);
    ui::row(row_rect, {
        .border_width = selected ? 3.0f : 1.5f,
        .bg = bg,
        .bg_hover = selected ? g_theme->row_soft_selected_hover : g_theme->row_hover,
        .border_color = selected ? g_theme->border_active : g_theme->border,
        .custom_colors = true,
    });
    ui::draw_text_in_rect(TextFormat("#%d", rank_index + 1), 27,
                          layout.rank,
                          rank_color(rank_index), ui::text_align::left);
    draw_profile_image_slot(layout.avatar,
                            score.avatar_url,
                            score.display_name,
                            avatar_base_url);
    {
        ui::scoped_clip_rect name_clip(layout.name);
        ui::draw_text_in_rect(score.display_name.c_str(), 20, layout.name,
                              score.failed ? g_theme->text_muted : g_theme->text,
                              ui::text_align::left);
    }
    if (self) {
        draw_chip(layout.self_chip, "YOU", g_theme->accent, 13);
    }
    ui::draw_text_in_rect(format_score(score.score).c_str(), 22,
                          layout.score,
                          g_theme->text, ui::text_align::right);
    ui::draw_text_in_rect(TextFormat("%.1f%%", score.accuracy), 20,
                          layout.accuracy,
                          g_theme->fast, ui::text_align::right);
    ui::draw_text_in_rect(std::to_string(score.combo).c_str(), 20,
                          layout.combo,
                          g_theme->text_secondary, ui::text_align::right);
    ui::accent_bar(layout.accent,
                   selected ? rank_color(rank_index) : (score.failed ? g_theme->error : g_theme->success));
}

}  // namespace

std::string score_key(const play_multiplayer_score_row& score) {
    return !score.user_id.empty() ? score.user_id : score.display_name;
}

interaction_result handle_input(const std::vector<play_multiplayer_score_row>& scores,
                                float current_scroll_y,
                                bool current_scrollbar_dragging,
                                float current_scrollbar_drag_offset) {
    interaction_result result;
    const float list_content_height = content_height(static_cast<int>(scores.size()));
    const float max_scroll = std::max(0.0f, list_content_height - kListViewportRect.height);
    float next_scroll_y = std::clamp(current_scroll_y, 0.0f, max_scroll);
    bool next_scrollbar_dragging = current_scrollbar_dragging;
    float next_scrollbar_drag_offset = current_scrollbar_drag_offset;

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (ui::contains_point(kListViewportRect, mouse)) {
        next_scroll_y = std::clamp(next_scroll_y - GetMouseWheelMove() * 46.0f, 0.0f, max_scroll);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            const float local_y = mouse.y - kListViewportRect.y + next_scroll_y;
            const int index = static_cast<int>(local_y / (kRowHeight + kRowGap));
            const float row_local_y = local_y - static_cast<float>(index) * (kRowHeight + kRowGap);
            if (index >= 0 && index < static_cast<int>(scores.size()) && row_local_y <= kRowHeight) {
                result.selected_score_key = score_key(scores[static_cast<size_t>(index)]);
            }
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
        float y = kListViewportRect.y - scroll_y;
        for (int i = 0; i < static_cast<int>(scores.size()); ++i) {
            const play_multiplayer_score_row& score = scores[static_cast<size_t>(i)];
            const bool self = !self_user_id.empty() && score.user_id == self_user_id;
            const bool selected = score_key(score) == selected_score_key;
            const Rectangle row_rect{kListViewportRect.x, y, kListViewportRect.width, kRowHeight};
            if (row_rect.y + row_rect.height >= kListViewportRect.y &&
                row_rect.y <= kListViewportRect.y + kListViewportRect.height) {
                draw_row(row_rect, score, i, self, selected, avatar_base_url);
            }
            y += kRowHeight + kRowGap;
        }
    }
    ui::scrollbar(kListScrollbarRect, list_content_height, scroll_y, {
        .track_color = g_theme->scrollbar_track,
        .thumb_color = g_theme->scrollbar_thumb,
        .custom_colors = true,
    });
}

}  // namespace multiplayer_result::score_list
