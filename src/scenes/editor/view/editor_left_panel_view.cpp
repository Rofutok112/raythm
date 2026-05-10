#include "editor/view/editor_left_panel_view.h"

#include "editor/view/editor_layout.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;

const char* key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

bool accepts_metadata_character(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint <= 126;
}

const char* palette_label(note_type type) {
    switch (type) {
        case note_type::tap:
            return "TAP";
        case note_type::hold:
            return "LONG";
        case note_type::release:
            return "RELEASE";
        case note_type::stay:
            return "STAY";
    }
    return "TAP";
}

void draw_palette_button(Rectangle rect, note_type type, const editor_note_palette_selection& selection,
                         editor_left_panel_view_result& result) {
    const auto& t = *g_theme;
    const bool selected = selection.type == type;
    const ui::button_state button = ui::draw_button_colored(
        rect, palette_label(type), 14,
        selected ? t.row_selected : t.row,
        selected ? t.row_active : t.row_hover,
        selected ? t.text : t.text_secondary,
        1.5f);
    if (button.clicked) {
        result.selected_note_type = type;
    }
}
}

editor_left_panel_view_result editor_left_panel_view::draw(const editor_left_panel_view_model& model) {
    const auto& t = *g_theme;
    editor_left_panel_view_result result;
    metadata_panel_state& metadata_panel = *model.metadata_panel;

    const Rectangle content = ui::inset(layout::kLeftPanelRect, ui::edge_insets::uniform(16.0f));
    const char* status_label = model.is_dirty ? "Modified" : (model.has_file ? "Saved" : "Unsaved");

    const Rectangle header_rect = ui::place(content, content.width, 54.0f, ui::anchor::top_left, ui::anchor::top_left);
    ui::draw_header_block(header_rect, "Chart", model.has_file ? "Existing chart" : "New chart", 28, 18, 4.0f);
    const Rectangle song_title_rect = {header_rect.x, header_rect.y + 58.0f, header_rect.width, 24.0f};
    draw_marquee_text(model.song_title, song_title_rect.x,
                      song_title_rect.y + 2.0f, 18, t.text_secondary, song_title_rect.width, model.now);

    const Rectangle meta_box = {content.x, content.y + 100.0f, content.width, 214.0f};
    ui::draw_section(meta_box);
    ui::draw_text_in_rect("Metadata", 22,
                          {meta_box.x + 12.0f, meta_box.y + 10.0f, meta_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    result.difficulty_result = ui::draw_text_input(
        {meta_box.x + 12.0f, meta_box.y + 46.0f, meta_box.width - 24.0f, 34.0f},
        metadata_panel.difficulty_input, "Diff", "Difficulty", "New",
        ui::draw_layer::base, 16, 24, accepts_metadata_character, 58.0f);
    result.author_result = ui::draw_text_input(
        {meta_box.x + 12.0f, meta_box.y + 86.0f, meta_box.width - 24.0f, 34.0f},
        metadata_panel.chart_author_input, "Author", "Chart author", "Unknown",
        ui::draw_layer::base, 16, 32, accepts_metadata_character, 58.0f);

    const ui::selector_state key_count_selector = ui::draw_value_selector(
        {meta_box.x + 12.0f, meta_box.y + 126.0f, meta_box.width - 24.0f, 34.0f},
        "Mode", key_count_label(metadata_panel.key_count),
        16, 26.0f, 58.0f, 12.0f);
    result.key_count_left_clicked = key_count_selector.left.clicked;
    result.key_count_right_clicked = key_count_selector.right.clicked;

    ui::draw_label_value({meta_box.x + 12.0f, meta_box.y + 170.0f, meta_box.width - 24.0f, 20.0f},
                         "Status", status_label, 16, t.text_secondary,
                         model.is_dirty ? t.error : t.success, 58.0f);

    if (!metadata_panel.error.empty()) {
        ui::draw_text_in_rect(metadata_panel.error.c_str(), 16,
                              {meta_box.x + 12.0f, meta_box.y + 188.0f, meta_box.width - 24.0f, 20.0f},
                              t.error, ui::text_align::left);
    }

    const Rectangle palette_box = {content.x, meta_box.y + meta_box.height + 12.0f, content.width, 136.0f};
    ui::draw_section(palette_box);
    ui::draw_text_in_rect("Palette", 22,
                          {palette_box.x + 12.0f, palette_box.y + 10.0f, palette_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    const float gap = 8.0f;
    const float button_width = (palette_box.width - 24.0f - gap) * 0.5f;
    const float button_height = 28.0f;
    const float left = palette_box.x + 12.0f;
    const float right = left + button_width + gap;
    const float first_row_y = palette_box.y + 44.0f;
    const float second_row_y = first_row_y + button_height + gap;
    draw_palette_button({left, first_row_y, button_width, button_height},
                        note_type::tap, model.note_palette, result);
    draw_palette_button({right, first_row_y, button_width, button_height},
                        note_type::hold, model.note_palette, result);
    draw_palette_button({left, second_row_y, button_width, button_height},
                        note_type::release, model.note_palette, result);
    draw_palette_button({right, second_row_y, button_width, button_height},
                        note_type::stay, model.note_palette, result);

    const bool ray_selected = model.note_palette.is_ray;
    const ui::button_state ray_button = ui::draw_button_colored(
        {palette_box.x + 12.0f, second_row_y + button_height + gap, palette_box.width - 24.0f, 28.0f},
        "RAY", 14,
        ray_selected ? t.row_selected : t.row,
        ray_selected ? t.row_active : t.row_hover,
        ray_selected ? t.text : t.text_secondary,
        1.5f);
    result.ray_toggled = ray_button.clicked;

    if (model.load_error != nullptr) {
        ui::draw_text_in_rect(model.load_error->c_str(), 18,
                              {content.x, palette_box.y + palette_box.height + 18.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }

    return result;
}
