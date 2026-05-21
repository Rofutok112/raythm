#include "song_create/song_create_form_panel.h"

#include <algorithm>

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

Rectangle make_custom_row(const config& view_config, float y, float height) {
    return {view_config.form_x, y, view_config.form_width, height};
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

    ui::draw_text_input(make_custom_row(view_config, y, view_config.row_height),
                        state.title_input, "Title", "Song title",
                        nullptr, view_config.layer, 16, 128, wide_text_filter,
                        view_config.text_input_label_width);
    y += view_config.row_height + view_config.row_gap;

    ui::draw_text_input(make_custom_row(view_config, y, view_config.row_height),
                        state.artist_input, "Artist", "Artist name",
                        nullptr, view_config.layer, 16, 128, wide_text_filter,
                        view_config.text_input_label_width);
    y += view_config.row_height + view_config.row_gap;

    song_create::tag_editor::draw_genre_selector(
        make_custom_row(view_config, y, 126.0f),
        state.selected_genres,
        state.genre_search_input,
        view_config.layer,
        view_config.text_input_label_width);
    y += 126.0f + view_config.row_gap;

    song_create::tag_editor::draw_keyword_editor(
        make_custom_row(view_config, y, 104.0f),
        state.selected_keywords,
        state.keyword_input,
        view_config.layer,
        view_config.text_input_label_width);
    y += 104.0f + view_config.row_gap;

    actions.draw_timing_summary(make_custom_row(view_config, y, view_config.row_height));
    y += view_config.row_height + view_config.row_gap;

    {
        const Rectangle audio_row = make_custom_row(view_config, y, view_config.row_height);
        const Rectangle input_rect = {
            audio_row.x,
            audio_row.y,
            audio_row.width - kBrowseWidth - kBrowseGap,
            audio_row.height,
        };
        const Rectangle browse_rect = {
            audio_row.x + audio_row.width - kBrowseWidth,
            audio_row.y,
            kBrowseWidth,
            audio_row.height,
        };

        ui::draw_text_input(input_rect, state.audio_path_input, "Audio", "Select audio file...",
                            nullptr, view_config.layer, 16, 512, nullptr,
                            view_config.text_input_label_width);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = actions.browse_audio();
            if (!path.empty()) {
                state.audio_path_input.value = path;
            }
        }
    }
    y += view_config.row_height + view_config.row_gap;

    {
        const Rectangle jacket_row = make_custom_row(view_config, y, view_config.row_height);
        const Rectangle input_rect = {
            jacket_row.x,
            jacket_row.y,
            jacket_row.width - kBrowseWidth - kBrowseGap,
            jacket_row.height,
        };
        const Rectangle browse_rect = {
            jacket_row.x + jacket_row.width - kBrowseWidth,
            jacket_row.y,
            kBrowseWidth,
            jacket_row.height,
        };

        ui::draw_text_input(input_rect, state.jacket_path_input, "Jacket", "Select image file... (optional)",
                            nullptr, view_config.layer, 16, 512, nullptr,
                            view_config.text_input_label_width);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            actions.browse_jacket();
        }
    }
    y += view_config.row_height + view_config.row_gap;

    ui::draw_text_input(make_custom_row(view_config, y, view_config.row_height),
                        state.preview_ms_input, "Preview (ms)", "0",
                        "0", view_config.layer, 16, 10, int_filter,
                        view_config.text_input_label_width);
    y += view_config.row_height + view_config.row_gap;

    const float button_y = y + kButtonTopGap - view_config.row_gap;
    const Rectangle create_rect = {
        view_config.form_x + view_config.form_width - kButtonWidth,
        button_y,
        kButtonWidth,
        kButtonHeight,
    };
    const Rectangle cancel_rect = {
        view_config.form_x + view_config.form_width - kButtonWidth * 2.0f - kButtonGap,
        button_y,
        kButtonWidth,
        kButtonHeight,
    };
    const char* submit_label = view_config.edit_mode ? "SAVE" : "CREATE";
    const char* cancel_label = view_config.edit_mode ? "BACK" : "CANCEL";

    if (ui::draw_button(create_rect, submit_label, 16).clicked) {
        const bool success = actions.submit();
        panel_result.created_song = success && !view_config.edit_mode;
    }

    if (ui::draw_button(cancel_rect, cancel_label, 16).clicked) {
        actions.cancel();
        return panel_result;
    }

    if (!state.error.empty()) {
        const Rectangle error_rect = {
            view_config.form_x,
            button_y + kButtonHeight + kErrorTopGap,
            view_config.form_width,
            kErrorHeight,
        };
        ui::draw_text_in_rect(state.error.c_str(), 14, error_rect, g_theme->error, ui::text_align::left);
    }

    if (state.jacket_picker.is_open()) {
        state.jacket_picker.draw();
    }

    return panel_result;
}

}  // namespace song_create::form_panel
