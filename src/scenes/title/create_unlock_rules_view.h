#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace title_create_unlock_rules_view {

struct rule_item {
    std::string title;
    std::string summary;
    bool selected = false;
};

struct editor_model {
    bool visible = false;
    bool show_source = false;
    bool show_rank = false;
    std::string type_label;
    std::string source_label;
    std::string rank_label;
    std::string reason;
    std::string summary;
    std::string validation_message;
    bool validation_is_error = false;
};

enum class command_type {
    none,
    close,
    select_rule,
    type_previous,
    type_next,
    source_previous,
    source_next,
    rank_previous,
    rank_next,
    add_rule,
    remove_rule,
    validate,
    save,
};

struct command {
    command_type type = command_type::none;
    int index = -1;
};

struct model {
    std::string subtitle;
    std::string message;
    std::string close_label;
    std::string validate_label;
    std::string save_label;
    std::span<const rule_item> rules;
    editor_model editor;
    bool loading = false;
    bool dirty = false;
    bool busy = false;
    bool can_add = false;
    bool can_remove = false;
    float rule_list_scroll_y = 0.0f;
    bool rule_list_scrollbar_dragging = false;
    float rule_list_scrollbar_drag_offset = 0.0f;
};

struct draw_result {
    command action;
    float rule_list_scroll_y = 0.0f;
    bool rule_list_scrollbar_dragging = false;
    float rule_list_scrollbar_drag_offset = 0.0f;
};

float max_rule_list_scroll(std::size_t rule_count);
draw_result draw(const model& data);

}  // namespace title_create_unlock_rules_view
