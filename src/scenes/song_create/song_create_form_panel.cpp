#include "song_create/song_create_form_panel.h"

#include <algorithm>
#include <array>

#include "song_create/song_create_tag_editor.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"
#include "ui_text.h"

namespace song_create::form_panel {
namespace {

constexpr float kBrowseWidth = 138.0f;
constexpr float kBrowseGap = 12.0f;
constexpr float kButtonTopGap = 24.0f;
constexpr float kButtonWidth = 270.0f;
constexpr float kButtonHeight = 66.0f;
constexpr float kButtonGap = 18.0f;
constexpr float kErrorTopGap = 18.0f;
constexpr float kErrorHeight = 36.0f;

struct form_action_layout {
    Rectangle cancel_button;
    Rectangle submit_button;
    Rectangle error;
};

enum class form_action_kind {
    cancel,
    submit,
};

enum class browse_action_kind {
    audio,
    jacket,
};

struct browse_action_button {
    Rectangle rect;
    const char* label;
    browse_action_kind kind;
};

struct browse_field_row {
    Rectangle row;
    ui::text_input_state& input;
    const char* label;
    const char* placeholder;
    browse_action_kind kind;
};

ui::rect_pair browse_row_layout(Rectangle row) {
    return ui::split_trailing(row, kBrowseWidth, kBrowseGap);
}

form_action_layout action_layout_for(const config& view_config, float button_y) {
    constexpr float kActionButtonsWidth = kButtonWidth * 2.0f + kButtonGap;
    std::array<Rectangle, 2> action_buttons{};
    ui::hstack_fill({view_config.form_x + view_config.form_width - kActionButtonsWidth,
                     button_y,
                     kActionButtonsWidth,
                     kButtonHeight},
                    kButtonGap,
                    action_buttons);
    return {
        action_buttons[0],
        action_buttons[1],
        {
            view_config.form_x,
            button_y + kButtonHeight + kErrorTopGap,
            view_config.form_width,
            kErrorHeight,
        },
    };
}

std::array<ui::action_button_definition<form_action_kind>, 2> form_action_buttons_for(
    const form_action_layout& layout,
    const char* cancel_label,
    const char* submit_label) {
    return {{
        {layout.cancel_button, cancel_label, form_action_kind::cancel},
        {layout.submit_button, submit_label, form_action_kind::submit},
    }};
}

void apply_form_action_click(result& panel_result, form_action_kind kind) {
    switch (kind) {
        case form_action_kind::cancel:
            panel_result.cancel_requested = true;
            break;
        case form_action_kind::submit:
            panel_result.submit_requested = true;
            break;
    }
}

browse_action_button browse_action_button_for(Rectangle rect, browse_action_kind kind) {
    return {
        .rect = rect,
        .label = "BROWSE",
        .kind = kind,
    };
}

void apply_browse_action_click(result& panel_result, browse_action_kind kind) {
    switch (kind) {
        case browse_action_kind::audio:
            panel_result.browse_audio_requested = true;
            break;
        case browse_action_kind::jacket:
            panel_result.browse_jacket_requested = true;
            break;
    }
}

void draw_browse_action_button(result& panel_result,
                               const browse_action_button& button,
                               ui::draw_layer layer) {
    if (ui::button(button.rect, button.label, {
            .layer = layer,
            .font_size = 14,
        }).clicked) {
        apply_browse_action_click(panel_result, button.kind);
    }
}

void draw_browse_field_row(result& panel_result,
                           const browse_field_row& row,
                           const config& view_config) {
    const ui::rect_pair fields = browse_row_layout(row.row);
    ui::text_input(fields.first, row.input, row.label, row.placeholder, {
        .layer = view_config.layer,
        .font_size = 16,
        .max_length = 512,
        .filter = nullptr,
        .label_width = view_config.text_input_label_width,
    });
    draw_browse_action_button(panel_result,
                              browse_action_button_for(fields.second, row.kind),
                              view_config.layer);
}

bool int_filter(int codepoint, const std::string&) {
    return codepoint >= '0' && codepoint <= '9';
}

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

}  // namespace

result draw(state_refs state, const callbacks& actions, const config& view_config) {
    result panel_result;
    ui::vertical_layout_cursor rows =
        ui::vertical_cursor(view_config.form_x, view_config.form_start_y, view_config.form_width, view_config.row_gap);

    ui::text_input(rows.next(view_config.row_height),
                   state.title_input, "Title", "Song title", {
                       .layer = view_config.layer,
                       .font_size = 16,
                       .max_length = 128,
                       .filter = wide_text_filter,
                       .label_width = view_config.text_input_label_width,
                   });

    ui::text_input(rows.next(view_config.row_height),
                   state.artist_input, "Artist", "Artist name", {
                       .layer = view_config.layer,
                       .font_size = 16,
                       .max_length = 128,
                       .filter = wide_text_filter,
                       .label_width = view_config.text_input_label_width,
                   });

    panel_result.genre_selector = song_create::tag_editor::draw_genre_selector(
        rows.next(126.0f),
        state.selected_genres,
        state.genre_search_input,
        view_config.layer,
        view_config.text_input_label_width);

    panel_result.keyword_editor = song_create::tag_editor::draw_keyword_editor(
        rows.next(104.0f),
        state.selected_keywords,
        state.keyword_input,
        view_config.layer,
        view_config.text_input_label_width);

    panel_result.timing_summary_open_requested =
        actions.draw_timing_summary(rows.next(view_config.row_height));

    draw_browse_field_row(panel_result,
                          {rows.next(view_config.row_height),
                           state.audio_path_input,
                           "Audio",
                           "Select audio file...",
                           browse_action_kind::audio},
                          view_config);

    draw_browse_field_row(panel_result,
                          {rows.next(view_config.row_height),
                           state.jacket_path_input,
                           "Jacket",
                           "Select image file... (optional)",
                           browse_action_kind::jacket},
                          view_config);

    ui::text_input(rows.next(view_config.row_height),
                   state.preview_ms_input, "Preview (ms)", "0", {
                       .default_value = "0",
                       .layer = view_config.layer,
                       .font_size = 16,
                       .max_length = 10,
                       .filter = int_filter,
                       .label_width = view_config.text_input_label_width,
                   });

    const float button_y = rows.peek(0.0f).y + kButtonTopGap - view_config.row_gap;
    const form_action_layout actions_layout = action_layout_for(view_config, button_y);
    const char* submit_label = view_config.edit_mode ? "SAVE" : "CREATE";
    const char* cancel_label = view_config.edit_mode ? "BACK" : "CANCEL";
    const std::array<ui::action_button_definition<form_action_kind>, 2> action_buttons =
        form_action_buttons_for(actions_layout, cancel_label, submit_label);
    const auto clicked_action = ui::draw_action_buttons<form_action_kind>(action_buttons, {
        .layer = view_config.layer,
        .font_size = 16,
        .border_width = 2.0f,
    });
    if (clicked_action.has_value()) {
        apply_form_action_click(panel_result, *clicked_action);
    }

    if (!state.error.empty()) {
        ui::draw_text_in_rect(state.error.c_str(), 14, actions_layout.error, g_theme->error, ui::text_align::left);
    }

    if (state.jacket_picker.is_open()) {
        state.jacket_picker.draw();
    }

    return panel_result;
}

}  // namespace song_create::form_panel
