#include "title/create_unlock_rules_view.h"

#include <algorithm>
#include <string>

#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
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
    const Rectangle scrollbar = {
        content.x + content.width - kRuleListScrollbarWidth,
        content.y,
        kRuleListScrollbarWidth,
        content.height,
    };
    const Rectangle viewport = {
        content.x,
        content.y,
        content.width - kRuleListScrollbarWidth - kRuleListScrollbarGap,
        content.height,
    };
    return {.section = section, .content = content, .viewport = viewport, .scrollbar = scrollbar};
}

float rule_list_content_height(std::size_t count, float viewport_height) {
    if (count == 0) {
        return viewport_height;
    }
    const float rows = static_cast<float>(count) * kRowHeight;
    const float gaps = static_cast<float>(count - 1) * kRowGap;
    return std::max(viewport_height, rows + gaps);
}

Rectangle rule_row_rect(Rectangle viewport, int index, float scroll_y) {
    return {
        viewport.x,
        viewport.y + static_cast<float>(index) * (kRowHeight + kRowGap) - scroll_y,
        viewport.width,
        kRowHeight,
    };
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
    return std::max(0.0f, rule_list_content_height(rule_count, list.content.height) - list.content.height);
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
    const float max_scroll = std::max(0.0f, list_content_height - list_layout.content.height);
    const bool list_scrollable = max_scroll > 0.0f;
    const Rectangle rule_viewport = list_scrollable ? list_layout.viewport : list_layout.content;
    result.rule_list_scroll_y = std::clamp(result.rule_list_scroll_y, 0.0f, max_scroll);

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
    if (ui::is_hovered(rule_viewport, ui::draw_layer::modal) && GetMouseWheelMove() != 0.0f) {
        result.rule_list_scroll_y =
            std::clamp(result.rule_list_scroll_y - GetMouseWheelMove() * kRuleListWheelStep, 0.0f, max_scroll);
    }
    {
        ui::scoped_clip_rect clip(rule_viewport);
        for (int i = 0; i < static_cast<int>(data.rules.size()); ++i) {
            const Rectangle row = rule_row_rect(rule_viewport, i, result.rule_list_scroll_y);
            if (row.y + row.height < rule_viewport.y ||
                row.y > rule_viewport.y + rule_viewport.height) {
                continue;
            }
            const rule_item& item = data.rules[static_cast<std::size_t>(i)];
            const ui::row_state state = draw_modal_rule_row(row, item.selected);
            const Rectangle row_content = ui::inset(row, ui::edge_insets::symmetric(0.0f, 12.0f));
            ui::draw_text_in_rect(item.title.c_str(), 15,
                                  {row_content.x, row_content.y + 6.0f, row_content.width, 20.0f},
                                  item.selected ? t.text : t.text_dim, ui::text_align::left);
            ui::draw_text_in_rect(item.summary.c_str(), 11,
                                  {row_content.x, row_content.y + 31.0f, row_content.width, 18.0f},
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

    if (draw_modal_button(layout.add, "ADD", 16).clicked && !data.busy && data.can_add) {
        capture_command(result.action, command_type::add_rule);
    }
    if (draw_modal_button(layout.remove, "REMOVE", 16).clicked && !data.busy && data.can_remove) {
        capture_command(result.action, command_type::remove_rule);
    }
    if (draw_modal_button(layout.validate, data.validate_label.c_str(), 16).clicked && !data.busy) {
        capture_command(result.action, command_type::validate);
    }
    if (draw_modal_button(layout.save, data.save_label.c_str(), 16).clicked && !data.busy) {
        capture_command(result.action, command_type::save);
    }
    ui::draw_text_in_rect(data.message.c_str(), 14, layout.message,
                          data.dirty ? t.accent : t.text_muted, ui::text_align::left);

    return result;
}

}  // namespace title_create_unlock_rules_view
