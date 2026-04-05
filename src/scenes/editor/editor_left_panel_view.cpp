#include "editor_left_panel_view.h"

#include "editor_layout.h"
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

    const Rectangle tools_box = {content.x, meta_box.y + meta_box.height + 12.0f, content.width, 114.0f};
    ui::draw_section(tools_box);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 16.0f, tools_box.width - 24.0f, 24.0f},
                         "Mode", model.current_key_mode_label, 16,
                         t.text_secondary, t.text, 92.0f);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 44.0f, tools_box.width - 24.0f, 24.0f},
                         "Offset", TextFormat("%d ms", model.current_offset_ms), 16,
                         t.text_secondary, t.text, 92.0f);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 72.0f, tools_box.width - 24.0f, 24.0f},
                         "Notes", TextFormat("%d", model.note_count), 16,
                         t.text_secondary, t.text, 92.0f);

    if (model.load_error != nullptr) {
        ui::draw_text_in_rect(model.load_error->c_str(), 18,
                              {content.x, tools_box.y + tools_box.height + 18.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }

    return result;
}
