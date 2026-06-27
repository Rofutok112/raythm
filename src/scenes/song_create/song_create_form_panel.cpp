#include "song_create/song_create_form_panel.h"

#include <algorithm>
#include <array>

#include "song_create/song_create_tag_editor.h"
#include "theme.h"
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

Rectangle make_custom_row(const config& view_config, float y, float height) {
    return {view_config.form_x, y, view_config.form_width, height};
}

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

bool int_filter(int codepoint, const std::string&) {
    return codepoint >= '0' && codepoint <= '9';
}

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

}  // namespace

result draw(state_refs state, const callbacks& actions, const config& view_config) {
    result panel_result;
    float y = view_config.form_start_y;

    ui::text_input(make_custom_row(view_config, y, view_config.row_height),
                   state.title_input, "Title", "Song title", {
                       .layer = view_config.layer,
                       .font_size = 16,
                       .max_length = 128,
                       .filter = wide_text_filter,
                       .label_width = view_config.text_input_label_width,
                   });
    y += view_config.row_height + view_config.row_gap;

    ui::text_input(make_custom_row(view_config, y, view_config.row_height),
                   state.artist_input, "Artist", "Artist name", {
                       .layer = view_config.layer,
                       .font_size = 16,
                       .max_length = 128,
                       .filter = wide_text_filter,
                       .label_width = view_config.text_input_label_width,
                   });
    y += view_config.row_height + view_config.row_gap;

    panel_result.genre_selector = song_create::tag_editor::draw_genre_selector(
        make_custom_row(view_config, y, 126.0f),
        state.selected_genres,
        state.genre_search_input,
        view_config.layer,
        view_config.text_input_label_width);
    y += 126.0f + view_config.row_gap;

    panel_result.keyword_editor = song_create::tag_editor::draw_keyword_editor(
        make_custom_row(view_config, y, 104.0f),
        state.selected_keywords,
        state.keyword_input,
        view_config.layer,
        view_config.text_input_label_width);
    y += 104.0f + view_config.row_gap;

    panel_result.timing_summary_open_requested =
        actions.draw_timing_summary(make_custom_row(view_config, y, view_config.row_height));
    y += view_config.row_height + view_config.row_gap;

    {
        const Rectangle audio_row = make_custom_row(view_config, y, view_config.row_height);
        const ui::rect_pair audio_fields = browse_row_layout(audio_row);

        ui::text_input(audio_fields.first, state.audio_path_input, "Audio", "Select audio file...", {
            .layer = view_config.layer,
            .font_size = 16,
            .max_length = 512,
            .filter = nullptr,
            .label_width = view_config.text_input_label_width,
        });

        if (ui::button(audio_fields.second, "BROWSE", {
                .layer = view_config.layer,
                .font_size = 14,
            }).clicked) {
            panel_result.browse_audio_requested = true;
        }
    }
    y += view_config.row_height + view_config.row_gap;

    {
        const Rectangle jacket_row = make_custom_row(view_config, y, view_config.row_height);
        const ui::rect_pair jacket_fields = browse_row_layout(jacket_row);

        ui::text_input(jacket_fields.first, state.jacket_path_input, "Jacket", "Select image file... (optional)", {
            .layer = view_config.layer,
            .font_size = 16,
            .max_length = 512,
            .filter = nullptr,
            .label_width = view_config.text_input_label_width,
        });

        if (ui::button(jacket_fields.second, "BROWSE", {
                .layer = view_config.layer,
                .font_size = 14,
            }).clicked) {
            panel_result.browse_jacket_requested = true;
        }
    }
    y += view_config.row_height + view_config.row_gap;

    ui::text_input(make_custom_row(view_config, y, view_config.row_height),
                   state.preview_ms_input, "Preview (ms)", "0", {
                       .default_value = "0",
                       .layer = view_config.layer,
                       .font_size = 16,
                       .max_length = 10,
                       .filter = int_filter,
                       .label_width = view_config.text_input_label_width,
                   });
    y += view_config.row_height + view_config.row_gap;

    const float button_y = y + kButtonTopGap - view_config.row_gap;
    const form_action_layout actions_layout = action_layout_for(view_config, button_y);
    const char* submit_label = view_config.edit_mode ? "SAVE" : "CREATE";
    const char* cancel_label = view_config.edit_mode ? "BACK" : "CANCEL";

    if (ui::button(actions_layout.submit_button, submit_label, {
            .layer = view_config.layer,
            .font_size = 16,
        }).clicked) {
        panel_result.submit_requested = true;
    }

    if (ui::button(actions_layout.cancel_button, cancel_label, {
            .layer = view_config.layer,
            .font_size = 16,
        }).clicked) {
        panel_result.cancel_requested = true;
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
