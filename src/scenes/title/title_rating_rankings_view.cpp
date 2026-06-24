#include "title/title_rating_rankings_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_rating_rankings_view {
namespace {

constexpr Rectangle kModalRect{440.0f, 138.0f, 1040.0f, 804.0f};
constexpr float kRowHeight = 58.0f;
constexpr float kRowGap = 8.0f;

struct layout {
    Rectangle modal{};
    Rectangle close_button{};
    Rectangle refresh_button{};
    Rectangle previous_button{};
    Rectangle next_button{};
    Rectangle header{};
    Rectangle list{};
    Rectangle footer{};
};

Rectangle animated_bounds(float open_anim) {
    const float t = tween::ease_out_cubic(std::clamp(open_anim, 0.0f, 1.0f));
    const float scale = 0.94f + 0.06f * t;
    const Vector2 center{kModalRect.x + kModalRect.width * 0.5f,
                         kModalRect.y + kModalRect.height * 0.5f};
    return {
        center.x - kModalRect.width * scale * 0.5f,
        center.y - kModalRect.height * scale * 0.5f,
        kModalRect.width * scale,
        kModalRect.height * scale,
    };
}

layout make_layout(float open_anim) {
    const Rectangle modal = animated_bounds(open_anim);
    const Rectangle content = ui::inset(modal, ui::edge_insets::uniform(28.0f));
    return {
        .modal = modal,
        .close_button = {content.x + content.width - 92.0f, content.y, 92.0f, 42.0f},
        .refresh_button = {content.x + content.width - 202.0f, content.y, 96.0f, 42.0f},
        .previous_button = {content.x, content.y + content.height - 46.0f, 118.0f, 42.0f},
        .next_button = {content.x + content.width - 118.0f, content.y + content.height - 46.0f, 118.0f, 42.0f},
        .header = {content.x, content.y, content.width - 224.0f, 72.0f},
        .list = {content.x, content.y + 86.0f, content.width, content.height - 150.0f},
        .footer = {content.x + 136.0f, content.y + content.height - 44.0f, content.width - 272.0f, 40.0f},
    };
}

float max_scroll_for(const model& state, Rectangle list) {
    const Rectangle viewport = ui::inset(list, ui::edge_insets::uniform(12.0f));
    const float content_height =
        state.items.empty()
            ? 0.0f
            : static_cast<float>(state.items.size()) * kRowHeight +
                  static_cast<float>(state.items.size() - 1) * kRowGap;
    return std::max(0.0f, content_height - viewport.height);
}

std::string avatar_label(const auth::global_rating_ranking_entry& entry) {
    const std::string& source = entry.display_name.empty() ? entry.user_id : entry.display_name;
    std::string result;
    result.reserve(2);
    for (char ch : source) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "R" : result;
}

std::string page_label(const model& state) {
    if (state.total <= 0) {
        return "No rated players";
    }
    const int start = (state.page - 1) * state.page_size + 1;
    const int end = std::min(state.total, start + static_cast<int>(state.items.size()) - 1);
    return TextFormat("%d-%d / %d", start, std::max(start, end), state.total);
}

std::string rank_text(const auth::rating_summary& rating) {
    return rating.rank > 0 ? TextFormat("#%d", rating.rank) : "-";
}

void draw_ranking_row(Rectangle row,
                      const auth::global_rating_ranking_entry& entry,
                      const std::string& avatar_base_url,
                      ui::draw_layer layer) {
    const auto& t = *g_theme;
    const ui::row_state row_state = ui::draw_row(row, t.row, t.row_hover, t.border_light, 1.5f);
    const Rectangle avatar_rect{row.x + 82.0f, row.y + 9.0f, 40.0f, 40.0f};
    ui::draw_text_in_rect(rank_text(entry.rating).c_str(),
                          20,
                          {row.x + 14.0f, row.y + 10.0f, 56.0f, 38.0f},
                          entry.rating.rank <= 3 ? t.accent : t.text,
                          ui::text_align::left);
    avatar_texture_cache::draw_avatar(avatar_rect,
                                      entry.avatar_url,
                                      avatar_label(entry),
                                      t.row_soft_selected,
                                      t.text,
                                      14,
                                      avatar_base_url);
    ui::draw_text_in_rect(entry.display_name.empty() ? "Unknown Player" : entry.display_name.c_str(),
                          19,
                          {row.x + 136.0f, row.y + 7.0f, row.width - 440.0f, 26.0f},
                          t.text,
                          ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("Best %d / Eligible %d",
                                     entry.rating.best_play_count,
                                     entry.rating.eligible_play_count),
                          13,
                          {row.x + 136.0f, row.y + 34.0f, row.width - 440.0f, 18.0f},
                          t.text_muted,
                          ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%.2f", entry.rating.rating),
                          24,
                          {row.x + row.width - 216.0f, row.y + 8.0f, 126.0f, 30.0f},
                          t.text,
                          ui::text_align::right);
    ui::draw_text_in_rect("RATE",
                          10,
                          {row.x + row.width - 210.0f, row.y + 38.0f, 120.0f, 14.0f},
                          t.text_muted,
                          ui::text_align::right);
    ui::draw_text_in_rect("PROFILE",
                          13,
                          {row.x + row.width - 74.0f, row.y + 17.0f, 60.0f, 24.0f},
                          row_state.hovered ? t.accent : t.text_muted,
                          ui::text_align::right);
    (void)layer;
}

command make_command(command_type type, std::string user_id = {}) {
    return {.type = type, .user_id = std::move(user_id)};
}

}  // namespace

Rectangle modal_bounds(float open_anim) {
    return animated_bounds(open_anim);
}

void clamp_scroll(model& state) {
    state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll_for(state, make_layout(state.open_anim).list));
}

command handle_input(model& state, ui::draw_layer layer) {
    const layout l = make_layout(state.open_anim);
    if (ui::is_clicked(l.close_button, layer)) {
        return make_command(command_type::close);
    }
    if (!state.loading && ui::is_clicked(l.refresh_button, layer)) {
        return make_command(command_type::refresh);
    }
    if (!state.loading && state.page > 1 && ui::is_clicked(l.previous_button, layer)) {
        return make_command(command_type::previous_page);
    }
    if (!state.loading && state.has_next_page && ui::is_clicked(l.next_button, layer)) {
        return make_command(command_type::next_page);
    }

    const Rectangle viewport = ui::inset(l.list, ui::edge_insets::uniform(12.0f));
    if (CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), viewport)) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.01f) {
            state.scroll_offset -= wheel * 72.0f;
            state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll_for(state, l.list));
        }
    }

    float y = viewport.y - state.scroll_offset;
    for (const auth::global_rating_ranking_entry& entry : state.items) {
        const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
        if (row.y + row.height >= viewport.y && row.y <= viewport.y + viewport.height &&
            ui::is_clicked(row, layer)) {
            return make_command(command_type::open_profile, entry.user_id);
        }
        y += kRowHeight + kRowGap;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), l.modal)) {
        return make_command(command_type::close);
    }
    return {};
}

void draw(const model& state, ui::draw_layer layer, bool draw_backdrop) {
    const auto& t = *g_theme;
    const layout l = make_layout(state.open_anim);
    if (draw_backdrop) {
        ui::draw_fullscreen_overlay(with_alpha(BLACK, 122));
    }
    ui::draw_panel(l.modal);

    ui::draw_text_in_rect("Global Rating", 30, l.header, t.text, ui::text_align::left);
    ui::draw_text_in_rect(page_label(state).c_str(),
                          15,
                          {l.header.x, l.header.y + 44.0f, l.header.width, 22.0f},
                          t.text_muted,
                          ui::text_align::left);

    ui::draw_button(l.refresh_button, state.loading ? "Loading" : "Refresh", 15);
    ui::draw_button(l.close_button, "Close", 15);
    ui::draw_button_colored(l.previous_button,
                            "Prev",
                            15,
                            state.page > 1 && !state.loading ? t.row : t.section,
                            t.row_hover,
                            state.page > 1 && !state.loading ? t.text : t.text_muted,
                            1.5f);
    ui::draw_button_colored(l.next_button,
                            "Next",
                            15,
                            state.has_next_page && !state.loading ? t.row : t.section,
                            t.row_hover,
                            state.has_next_page && !state.loading ? t.text : t.text_muted,
                            1.5f);
    ui::draw_text_in_rect(state.message.c_str(), 14, l.footer, t.text_muted, ui::text_align::center);

    ui::draw_section(l.list);
    const Rectangle viewport = ui::inset(l.list, ui::edge_insets::uniform(12.0f));
    if (state.loading && state.items.empty()) {
        ui::draw_text_in_rect("Loading rankings...", 18, viewport, t.text_muted, ui::text_align::center);
        return;
    }
    if (state.items.empty()) {
        ui::draw_text_in_rect(state.loaded_once ? "No rated players yet." : "Open to load rankings.",
                              18,
                              viewport,
                              t.text_muted,
                              ui::text_align::center);
        return;
    }

    ui::scoped_clip_rect clip(viewport);
    float y = viewport.y - state.scroll_offset;
    for (const auth::global_rating_ranking_entry& entry : state.items) {
        const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
        if (row.y + row.height >= viewport.y && row.y <= viewport.y + viewport.height) {
            draw_ranking_row(row, entry, state.avatar_base_url, layer);
        }
        y += kRowHeight + kRowGap;
    }
}

}  // namespace title_rating_rankings_view
