#include "title/title_rating_rankings_view.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>

#include "localization/localization.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_modal.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace title_rating_rankings_view {
namespace {

constexpr Rectangle kModalRect{440.0f, 138.0f, 1040.0f, 804.0f};
constexpr float kRowHeight = 58.0f;
constexpr float kRowGap = 8.0f;
constexpr float kRankWidth = 72.0f;
constexpr float kAvatarSize = 40.0f;
constexpr float kAvatarGap = 16.0f;
constexpr float kRatingWidth = 154.0f;
constexpr float kColumnGap = 18.0f;
constexpr float kModalPadding = 28.0f;
constexpr float kButtonGap = 14.0f;
constexpr float kCloseButtonWidth = 92.0f;
constexpr float kRefreshButtonWidth = 96.0f;
constexpr float kHeaderActionsHeight = 42.0f;
constexpr float kPageButtonWidth = 118.0f;
constexpr float kFooterSideInset = 136.0f;
constexpr float kListViewportPadding = 12.0f;

struct layout {
    Rectangle modal{};
    Rectangle close_button{};
    Rectangle refresh_button{};
    Rectangle previous_button{};
    Rectangle next_button{};
    Rectangle header_title{};
    Rectangle page_label{};
    Rectangle list{};
    Rectangle list_viewport{};
    Rectangle footer{};
};

struct ranking_row_layout {
    Rectangle rank{};
    Rectangle avatar{};
    Rectangle name{};
    Rectangle detail{};
    Rectangle rating_value{};
    Rectangle rating_label{};
};

struct rating_ranking_row {
    const auth::global_rating_ranking_entry* entry = nullptr;
    Rectangle body{};
    std::string rank;
    std::string display_name;
    std::string avatar_label;
    std::string detail;
    std::string rating_value;
    bool top_rank = false;
};

enum class action_button_style {
    regular,
    gated,
};

struct ranking_action_button {
    Rectangle rect{};
    command_type command = command_type::none;
    const char* label = "";
    bool enabled = true;
    action_button_style style = action_button_style::regular;
};

constexpr Rectangle list_viewport_for(Rectangle list) {
    return ui::inset(list, ui::edge_insets::uniform(kListViewportPadding));
}

float ranking_content_height(int item_count) {
    return ui::vertical_list_content_height(item_count, kRowHeight, kRowGap);
}

constexpr Rectangle ranking_row_rect_for(Rectangle viewport, int index, float scroll_offset) {
    return ui::vertical_list_row_rect(viewport, index, kRowHeight, kRowGap, scroll_offset);
}

constexpr ranking_row_layout ranking_row_layout_for(Rectangle row) {
    const Rectangle content = ui::inset(row, ui::edge_insets{0.0f, 24.0f, 0.0f, 14.0f});
    const ui::rect_pair rating_split = ui::split_trailing(content, kRatingWidth, kColumnGap);
    const ui::rect_pair rank_split = ui::split_columns(content, kRankWidth - 16.0f, 12.0f);
    const ui::rect_pair avatar_split = ui::split_columns(rank_split.second, kAvatarSize, kAvatarGap);
    const float name_x = avatar_split.second.x;
    const float name_width = std::max(80.0f, rating_split.second.x - kColumnGap - name_x);
    return {
        {rank_split.first.x, row.y + 10.0f, rank_split.first.width, 38.0f},
        {avatar_split.first.x, row.y + 9.0f, kAvatarSize, kAvatarSize},
        {name_x, row.y + 7.0f, name_width, 26.0f},
        {name_x, row.y + 34.0f, name_width, 18.0f},
        {rating_split.second.x, row.y + 8.0f, rating_split.second.width, 30.0f},
        {rating_split.second.x, row.y + 38.0f, rating_split.second.width, 14.0f},
    };
}

layout make_layout(float open_anim) {
    const Rectangle modal = ui::animated_modal_rect(kModalRect, open_anim);
    const Rectangle content = ui::inset(modal, ui::edge_insets::uniform(kModalPadding));
    const Rectangle header_actions{content.x, content.y, content.width, kHeaderActionsHeight};
    const ui::rect_pair close_split = ui::split_trailing(header_actions, kCloseButtonWidth, kButtonGap);
    const ui::rect_pair refresh_split = ui::split_trailing(close_split.first, kRefreshButtonWidth, kButtonGap);
    const Rectangle footer_actions{content.x, content.y + content.height - 46.0f, content.width, kHeaderActionsHeight};
    const ui::rect_pair previous_split = ui::split_columns(footer_actions, kPageButtonWidth);
    const ui::rect_pair next_split = ui::split_trailing(footer_actions, kPageButtonWidth);
    const Rectangle header{content.x, content.y, content.width - 224.0f, 72.0f};
    const Rectangle list{content.x, content.y + 86.0f, content.width, content.height - 150.0f};
    const Rectangle footer = ui::inset(
        {content.x, content.y + content.height - 44.0f, content.width, 40.0f},
        ui::edge_insets{0.0f, kFooterSideInset, 0.0f, kFooterSideInset});
    return {
        .modal = modal,
        .close_button = close_split.second,
        .refresh_button = refresh_split.second,
        .previous_button = previous_split.first,
        .next_button = next_split.second,
        .header_title = {header.x, header.y, header.width, 42.0f},
        .page_label = {header.x, header.y + 44.0f, header.width, 22.0f},
        .list = list,
        .list_viewport = list_viewport_for(list),
        .footer = footer,
    };
}

float max_scroll_for(const model& state, Rectangle list) {
    const Rectangle viewport = list_viewport_for(list);
    return ui::max_scroll_offset(ranking_content_height(static_cast<int>(state.items.size())), viewport);
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
        return localization::tr_literal("No rated players");
    }
    const int start = (state.page - 1) * state.page_size + 1;
    const int end = std::min(state.total, start + static_cast<int>(state.items.size()) - 1);
    return TextFormat("%d-%d / %d", start, std::max(start, end), state.total);
}

std::string rank_text(const auth::rating_summary& rating) {
    return rating.rank > 0 ? TextFormat("#%d", rating.rank) : "-";
}

std::string best_eligible_text(const auth::rating_summary& rating) {
    if (localization::current_locale() == localization::locale::japanese) {
        return TextFormat("ベスト %d / 対象 %d", rating.best_play_count, rating.eligible_play_count);
    }
    return TextFormat("Best %d / Eligible %d", rating.best_play_count, rating.eligible_play_count);
}

rating_ranking_row rating_ranking_row_for(Rectangle row, const auth::global_rating_ranking_entry& entry) {
    return {
        .entry = &entry,
        .body = row,
        .rank = rank_text(entry.rating),
        .display_name = entry.display_name.empty() ? localization::tr_literal("Unknown Player") : entry.display_name,
        .avatar_label = avatar_label(entry),
        .detail = best_eligible_text(entry.rating),
        .rating_value = TextFormat("%.0f", entry.rating.rating),
        .top_rank = entry.rating.rank <= 3,
    };
}

void draw_ranking_row(const rating_ranking_row& row,
                      const std::string& avatar_base_url,
                      ui::draw_layer layer) {
    if (row.entry == nullptr) {
        return;
    }

    const auto& t = *g_theme;
    const auth::global_rating_ranking_entry& entry = *row.entry;
    ui::row(row.body, {
        .layer = layer,
        .border_width = 1.5f,
        .bg = t.row,
        .bg_hover = t.row_hover,
        .border_color = t.border_light,
        .custom_colors = true,
    });
    const ranking_row_layout row_layout = ranking_row_layout_for(row.body);
    ui::draw_text_in_rect(row.rank.c_str(),
                          20,
                          row_layout.rank,
                          row.top_rank ? t.accent : t.text,
                          ui::text_align::left);
    avatar_texture_cache::draw_avatar(row_layout.avatar,
                                      entry.avatar_url,
                                      row.avatar_label,
                                      t.row_soft_selected,
                                      t.text,
                                      14,
                                      avatar_base_url);
    ui::draw_text_in_rect(row.display_name.c_str(),
                          19,
                          row_layout.name,
                          t.text,
                          ui::text_align::left);
    ui::draw_text_in_rect(row.detail.c_str(),
                          13,
                          row_layout.detail,
                          t.text_muted,
                          ui::text_align::left);
    ui::draw_text_in_rect(row.rating_value.c_str(),
                          24,
                          row_layout.rating_value,
                          t.text,
                          ui::text_align::right);
    ui::draw_text_in_rect(localization::tr_literal("RATE"),
                          10,
                          row_layout.rating_label,
                          t.text_muted,
                          ui::text_align::right);
}

void draw_empty_state(Rectangle viewport, const model& state) {
    const char* message = state.loading
        ? localization::tr_literal("Loading rankings...")
        : (state.loaded_once
               ? localization::tr_literal("No rated players yet.")
               : localization::tr_literal("Open to load rankings."));
    ui::draw_text_in_rect(message, 18, viewport, g_theme->text_muted, ui::text_align::center);
}

command make_command(command_type type, std::string user_id = {}) {
    return {.type = type, .user_id = std::move(user_id)};
}

std::optional<command> hit_test_ranking_row_command(const model& state,
                                                    Rectangle viewport,
                                                    float scroll_offset,
                                                    ui::draw_layer layer) {
    const ui::index_range visible_rows = ui::vertical_list_visible_range(
        state.items.size(), viewport, kRowHeight, kRowGap, scroll_offset);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        const auth::global_rating_ranking_entry& entry = state.items[static_cast<size_t>(i)];
        const Rectangle row = ranking_row_rect_for(viewport, i, scroll_offset);
        if (ui::is_clicked(row, layer)) {
            return make_command(command_type::open_profile, entry.user_id);
        }
    }
    return std::nullopt;
}

std::array<ranking_action_button, 4> action_buttons_for(const layout& l, const model& state) {
    return {{
        {
            .rect = l.refresh_button,
            .command = command_type::refresh,
            .label = state.loading ? localization::tr_literal("Loading") : localization::tr_literal("Refresh"),
            .enabled = !state.loading,
            .style = action_button_style::regular,
        },
        {
            .rect = l.close_button,
            .command = command_type::close,
            .label = localization::tr_literal("Close"),
            .enabled = true,
            .style = action_button_style::regular,
        },
        {
            .rect = l.previous_button,
            .command = command_type::previous_page,
            .label = localization::tr_literal("Prev"),
            .enabled = state.page > 1 && !state.loading,
            .style = action_button_style::gated,
        },
        {
            .rect = l.next_button,
            .command = command_type::next_page,
            .label = localization::tr_literal("Next"),
            .enabled = state.has_next_page && !state.loading,
            .style = action_button_style::gated,
        },
    }};
}

std::optional<command> hit_test_action_button_command(const std::array<ranking_action_button, 4>& buttons,
                                                      ui::draw_layer layer) {
    for (const ranking_action_button& button : buttons) {
        if (button.enabled && ui::is_clicked(button.rect, layer)) {
            return make_command(button.command);
        }
    }
    return std::nullopt;
}

void draw_action_button(const ranking_action_button& button, ui::draw_layer layer) {
    if (button.style == action_button_style::gated) {
        ui::action_button(button.rect, button.label, {
            .layer = layer,
            .font_size = 15,
            .border_width = 1.5f,
            .enabled = button.enabled,
        });
        return;
    }

    ui::button(button.rect, button.label, {
        .layer = layer,
        .font_size = 15,
    });
}

void draw_ranking_rows(const model& state, Rectangle viewport, ui::draw_layer layer) {
    ui::scoped_clip_rect clip(viewport);
    const ui::index_range visible_rows = ui::vertical_list_visible_range(
        state.items.size(), viewport, kRowHeight, kRowGap, state.scroll_offset);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        const auth::global_rating_ranking_entry& entry = state.items[static_cast<size_t>(i)];
        const Rectangle row = ranking_row_rect_for(viewport, i, state.scroll_offset);
        draw_ranking_row(rating_ranking_row_for(row, entry), state.avatar_base_url, layer);
    }
}

void draw_rating_modal_chrome(const layout& l, const model& state, ui::draw_layer layer) {
    const auto& t = *g_theme;
    ui::draw_text_in_rect(localization::tr_literal("Global Rating"),
                          30,
                          l.header_title,
                          t.text,
                          ui::text_align::left);
    ui::draw_text_in_rect(page_label(state).c_str(),
                          15,
                          l.page_label,
                          t.text_muted,
                          ui::text_align::left);

    const std::array<ranking_action_button, 4> action_buttons = action_buttons_for(l, state);
    for (const ranking_action_button& button : action_buttons) {
        draw_action_button(button, layer);
    }
    if (!state.message.empty()) {
        ui::draw_text_in_rect(state.message.c_str(), 14, l.footer, t.text_muted, ui::text_align::center);
    }
}

void draw_rating_ranking_list(const layout& l, const model& state, ui::draw_layer layer) {
    ui::section(l.list);
    const Rectangle viewport = l.list_viewport;
    if (state.items.empty()) {
        draw_empty_state(viewport, state);
        return;
    }

    draw_ranking_rows(state, viewport, layer);
}

float scrolled_ranking_offset(const model& state, Rectangle viewport, float scroll_offset) {
    const float content_height = ranking_content_height(static_cast<int>(state.items.size()));
    ui::scroll_offset_state scroll_state =
        ui::scroll_offset_state_for(viewport, content_height, scroll_offset);
    const float wheel = ui::mouse_wheel_move();
    scroll_state = ui::wheel_scrolled_offset_state(
        viewport,
        virtual_screen::get_virtual_mouse(),
        std::abs(wheel) > 0.01f ? wheel : 0.0f,
        content_height,
        scroll_state.offset,
        72.0f);
    return scroll_state.offset;
}

}  // namespace

Rectangle modal_bounds(float open_anim) {
    return ui::animated_modal_rect(kModalRect, open_anim);
}

float clamped_scroll_offset(const model& state) {
    return ui::clamp_scroll_offset(state.scroll_offset, max_scroll_for(state, make_layout(state.open_anim).list));
}

input_result handle_input(const model& state, ui::draw_layer layer) {
    input_result result;
    float scroll_offset = clamped_scroll_offset(state);
    const layout l = make_layout(state.open_anim);
    const std::array<ranking_action_button, 4> action_buttons = action_buttons_for(l, state);
    if (const std::optional<command> button_command = hit_test_action_button_command(action_buttons, layer)) {
        result.action = *button_command;
        return result;
    }

    const Rectangle viewport = l.list_viewport;
    scroll_offset = scrolled_ranking_offset(state, viewport, scroll_offset);
    if (scroll_offset != state.scroll_offset) {
        result.scroll_offset_changed = true;
        result.scroll_offset = scroll_offset;
    }

    if (const std::optional<command> row_command =
            hit_test_ranking_row_command(state, viewport, scroll_offset, layer)) {
        result.action = *row_command;
        return result;
    }

    if (ui::modal_outside_released(l.modal, virtual_screen::get_virtual_mouse())) {
        result.action = make_command(command_type::close);
        return result;
    }
    return result;
}

void draw(const model& state, ui::draw_layer layer, bool draw_backdrop) {
    const layout l = make_layout(state.open_anim);
    if (draw_backdrop) {
        ui::draw_fullscreen_overlay(with_alpha(BLACK, 122));
    }
    ui::panel(l.modal);
    draw_rating_modal_chrome(l, state, layer);
    draw_rating_ranking_list(l, state, layer);
}

}  // namespace title_rating_rankings_view
