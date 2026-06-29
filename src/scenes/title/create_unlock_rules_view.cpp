#include "title/create_unlock_rules_view.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>

#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace title_create_unlock_rules_view {
namespace {

constexpr Rectangle kDialogRect = {430.0f, 168.0f, 1060.0f, 744.0f};
constexpr Rectangle kTitleRect = {462.0f, 198.0f, 720.0f, 38.0f};
constexpr Rectangle kSubtitleRect = {462.0f, 238.0f, 860.0f, 28.0f};
constexpr Rectangle kRuleListRect = {462.0f, 296.0f, 368.0f, 456.0f};
constexpr Rectangle kEditorRect = {858.0f, 296.0f, 600.0f, 456.0f};
constexpr Rectangle kMessageRect = {462.0f, 766.0f, 996.0f, 34.0f};
constexpr Rectangle kCloseRect = {1326.0f, 198.0f, 132.0f, 42.0f};
constexpr Rectangle kAddRect = {462.0f, 812.0f, 160.0f, 48.0f};
constexpr Rectangle kRemoveRect = {638.0f, 812.0f, 160.0f, 48.0f};
constexpr Rectangle kValidateRect = {1064.0f, 812.0f, 180.0f, 48.0f};
constexpr Rectangle kSaveRect = {1262.0f, 812.0f, 196.0f, 48.0f};
constexpr float kRowHeight = 58.0f;
constexpr float kRowGap = 10.0f;
constexpr float kRuleListPadding = 12.0f;
constexpr float kRuleListScrollbarWidth = 8.0f;
constexpr float kRuleListScrollbarGap = 6.0f;
constexpr float kRuleListWheelStep = 72.0f;
constexpr float kRuleTitleTop = 6.0f;
constexpr float kRuleTitleHeight = 20.0f;
constexpr float kRuleSummaryTop = 31.0f;
constexpr float kRuleSummaryHeight = 18.0f;

struct unlock_rules_layout {
    Rectangle dialog;
    Rectangle title;
    Rectangle subtitle;
    Rectangle rule_list;
    Rectangle editor;
    Rectangle message;
    Rectangle close;
    Rectangle add;
    Rectangle remove;
    Rectangle validate;
    Rectangle save;
};

struct rule_list_layout {
    Rectangle section;
    Rectangle content;
    Rectangle viewport;
    Rectangle scrollbar;
};

struct modal_action_button {
    Rectangle rect{};
    const char* label = "";
    bool enabled = true;
    command_type command = command_type::none;
};

constexpr unlock_rules_layout make_layout() {
    return {
        .dialog = kDialogRect,
        .title = kTitleRect,
        .subtitle = kSubtitleRect,
        .rule_list = kRuleListRect,
        .editor = kEditorRect,
        .message = kMessageRect,
        .close = kCloseRect,
        .add = kAddRect,
        .remove = kRemoveRect,
        .validate = kValidateRect,
        .save = kSaveRect,
    };
}

constexpr rule_list_layout rule_list_layout_for(Rectangle section) {
    const Rectangle content = ui::inset(section, kRuleListPadding);
    const ui::rect_pair split = ui::split_trailing(content, kRuleListScrollbarWidth, kRuleListScrollbarGap);
    return {.section = section, .content = content, .viewport = split.first, .scrollbar = split.second};
}

float rule_list_content_height(std::size_t count, float viewport_height) {
    return ui::vertical_list_content_height(count, kRowHeight, kRowGap, viewport_height);
}

Rectangle rule_row_rect(Rectangle viewport, int index, float scroll_y) {
    return ui::vertical_list_row_rect(viewport, index, kRowHeight, kRowGap, scroll_y);
}

Rectangle rule_row_title_rect(Rectangle row_content) {
    return ui::vertical_span_rect(row_content, row_content.y + kRuleTitleTop, kRuleTitleHeight);
}

Rectangle rule_row_summary_rect(Rectangle row_content) {
    return ui::vertical_span_rect(row_content, row_content.y + kRuleSummaryTop, kRuleSummaryHeight);
}

void capture_command(command& target, command_type type, int index = -1) {
    if (target.type == command_type::none) {
        target = {.type = type, .index = index};
    }
}

ui::button_state draw_modal_button(Rectangle rect, const char* label, int font_size) {
    const auto& t = *g_theme;
    return ui::button(rect, label, {
        .layer = ui::draw_layer::modal,
        .font_size = font_size,
        .border_width = 2.0f,
        .bg = t.row,
        .bg_hover = t.row_hover,
        .text_color = t.text,
        .custom_colors = true,
    });
}

std::array<modal_action_button, 4> footer_action_buttons(const unlock_rules_layout& layout, const model& data) {
    return {{
        {.rect = layout.add, .label = "ADD", .enabled = !data.busy && data.can_add, .command = command_type::add_rule},
        {.rect = layout.remove,
         .label = "REMOVE",
         .enabled = !data.busy && data.can_remove,
         .command = command_type::remove_rule},
        {.rect = layout.validate,
         .label = data.validate_label.c_str(),
         .enabled = !data.busy,
         .command = command_type::validate},
        {.rect = layout.save, .label = data.save_label.c_str(), .enabled = !data.busy, .command = command_type::save},
    }};
}

void draw_modal_action_buttons(std::span<const modal_action_button> buttons, command& action) {
    for (const modal_action_button& button : buttons) {
        if (draw_modal_button(button.rect, button.label, 16).clicked && button.enabled) {
            capture_command(action, button.command);
        }
    }
}

ui::row_state draw_modal_rule_row(Rectangle rect, bool selected) {
    const auto& t = *g_theme;
    return ui::row(rect, {
        .layer = ui::draw_layer::modal,
        .border_width = 1.4f,
        .bg = selected ? t.row_selected : t.row,
        .bg_hover = selected ? t.row_active : t.row_hover,
        .border_color = selected ? t.border_active : t.border,
        .custom_colors = true,
    });
}

}  // namespace

float max_rule_list_scroll(std::size_t rule_count) {
    const rule_list_layout list = rule_list_layout_for(kRuleListRect);
    return ui::max_scroll_offset(rule_list_content_height(rule_count, list.content.height), list.content);
}

draw_result draw(const model& data) {
    const auto& t = *g_theme;
    const unlock_rules_layout layout = make_layout();
    const rule_list_layout list_layout = rule_list_layout_for(layout.rule_list);
    draw_result result{
        .rule_list_scroll_y = data.rule_list_scroll_y,
        .rule_list_scrollbar_dragging = data.rule_list_scrollbar_dragging,
        .rule_list_scrollbar_drag_offset = data.rule_list_scrollbar_drag_offset,
    };

    ui::register_hit_region({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                            ui::draw_layer::modal);
    ui::draw_fullscreen_overlay({0, 0, 0, 168});
    ui::panel(layout.dialog);
    ui::draw_text_in_rect("UNLOCK RULES", 26, layout.title, t.text, ui::text_align::left);
    ui::draw_text_in_rect(data.subtitle.c_str(), 15, layout.subtitle, t.text_muted, ui::text_align::left);

    if (draw_modal_button(layout.close, data.close_label.c_str(), 16).clicked) {
        capture_command(result.action, command_type::close);
        return result;
    }

    ui::section(list_layout.section);
    const float list_content_height = rule_list_content_height(data.rules.size(), list_layout.content.height);
    ui::scroll_offset_state scroll_state =
        ui::scroll_offset_state_for(list_layout.content, list_content_height, result.rule_list_scroll_y);
    const bool list_scrollable = scroll_state.max_scroll > 0.0f;
    const Rectangle rule_viewport = list_scrollable ? list_layout.viewport : list_layout.content;
    result.rule_list_scroll_y = scroll_state.offset;

    const ui::scrollbar_interaction scrollbar = ui::vertical_scrollbar(
        list_layout.scrollbar,
        list_content_height,
        result.rule_list_scroll_y,
        result.rule_list_scrollbar_dragging,
        result.rule_list_scrollbar_drag_offset, {
            .layer = ui::draw_layer::modal,
            .min_thumb_height = 32.0f,
        });
    if (scrollbar.changed || scrollbar.dragging) {
        result.rule_list_scroll_y = scrollbar.scroll_offset;
    }
    const float wheel = ui::mouse_wheel_move();
    if (ui::is_hovered(rule_viewport, ui::draw_layer::modal)) {
        scroll_state = ui::wheel_scrolled_offset_state(
            rule_viewport,
            virtual_screen::get_virtual_mouse(),
            wheel,
            list_content_height,
            result.rule_list_scroll_y,
            kRuleListWheelStep);
        result.rule_list_scroll_y = scroll_state.offset;
    }
    {
        ui::scoped_clip_rect clip(rule_viewport);
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            data.rules.size(), rule_viewport, kRowHeight, kRowGap, result.rule_list_scroll_y);
        for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
            const Rectangle row = rule_row_rect(rule_viewport, i, result.rule_list_scroll_y);
            const rule_item& item = data.rules[static_cast<std::size_t>(i)];
            const ui::row_state state = draw_modal_rule_row(row, item.selected);
            const Rectangle row_content = ui::inset(row, ui::edge_insets::symmetric(0.0f, 12.0f));
            ui::draw_text_in_rect(item.title.c_str(), 15,
                                  rule_row_title_rect(row_content),
                                  item.selected ? t.text : t.text_dim, ui::text_align::left);
            ui::draw_text_in_rect(item.summary.c_str(), 11,
                                  rule_row_summary_rect(row_content),
                                  t.text_muted, ui::text_align::left);
            if (state.clicked && ui::contains_point(rule_viewport, virtual_screen::get_virtual_mouse())) {
                capture_command(result.action, command_type::select_rule, i);
            }
        }
    }
    ui::scrollbar(list_layout.scrollbar, list_content_height, result.rule_list_scroll_y, {
        .layer = ui::draw_layer::modal,
        .min_thumb_height = 32.0f,
    });

    ui::section(layout.editor);
    if (data.loading) {
        ui::draw_text_in_rect("Loading...", 18, layout.editor, t.text_muted);
    } else if (!data.editor.visible) {
        ui::draw_text_in_rect("No lock rules. Add one to lock this chart.", 16, layout.editor, t.text_muted);
    } else {
        Rectangle control{layout.editor.x + 18.0f, layout.editor.y + 20.0f, layout.editor.width - 36.0f, 54.0f};
        const ui::selector_state type_selector =
            ui::value_selector(control, "Type", data.editor.type_label.c_str(), {
                .layer = ui::draw_layer::modal,
                .font_size = 18,
                .button_size = 34.0f,
                .label_width = 154.0f,
            });
        if (type_selector.left.clicked) {
            capture_command(result.action, command_type::type_previous);
        }
        if (type_selector.right.clicked) {
            capture_command(result.action, command_type::type_next);
        }
        control.y += 68.0f;
        if (data.editor.show_source) {
            const ui::selector_state source_selector =
                ui::value_selector(control, "Source", data.editor.source_label.c_str(), {
                    .layer = ui::draw_layer::modal,
                    .font_size = 16,
                    .button_size = 34.0f,
                    .label_width = 154.0f,
                });
            if (source_selector.left.clicked) {
                capture_command(result.action, command_type::source_previous);
            }
            if (source_selector.right.clicked) {
                capture_command(result.action, command_type::source_next);
            }
            control.y += 68.0f;
        }
        if (data.editor.show_rank) {
            const ui::selector_state rank_selector =
                ui::value_selector(control, "Min rank", data.editor.rank_label.c_str(), {
                    .layer = ui::draw_layer::modal,
                    .font_size = 18,
                    .button_size = 34.0f,
                    .label_width = 154.0f,
                });
            if (rank_selector.left.clicked) {
                capture_command(result.action, command_type::rank_previous);
            }
            if (rank_selector.right.clicked) {
                capture_command(result.action, command_type::rank_next);
            }
            control.y += 68.0f;
        }
        ui::draw_label_value({control.x, control.y, control.width, 44.0f}, "Reason",
                             data.editor.reason.c_str(), 15, t.text, t.text_dim, 154.0f);
        control.y += 64.0f;
        ui::draw_text_in_rect(data.editor.summary.c_str(), 14,
                              {control.x, control.y, control.width, 54.0f},
                              t.text_muted, ui::text_align::left);
        control.y += 70.0f;
        if (!data.editor.validation_message.empty()) {
            ui::draw_text_in_rect(data.editor.validation_message.c_str(), 13,
                                  {control.x, control.y, control.width, 44.0f},
                                  data.editor.validation_is_error ? t.error : t.success,
                                  ui::text_align::left);
        }
    }

    draw_modal_action_buttons(footer_action_buttons(layout, data), result.action);
    ui::draw_text_in_rect(data.message.c_str(), 14, layout.message,
                          data.dirty ? t.accent : t.text_muted, ui::text_align::left);

    return result;
}

}  // namespace title_create_unlock_rules_view
