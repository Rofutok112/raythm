#include "editor_mv_script_panel.h"

#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace {

constexpr float kButtonRowHeight = 30.0f;
constexpr float kErrorAreaHeight = 80.0f;
constexpr float kSpacing = 8.0f;

} // anonymous namespace

editor_mv_script_panel_result editor_mv_script_panel::draw(
    const editor_mv_script_panel_model& model,
    editor_mv_script_panel_state& state) {

    editor_mv_script_panel_result result;
    const Rectangle& c = model.content_rect;

    // Layout: editor | spacing | buttons | spacing | errors
    float editor_h = c.height - kButtonRowHeight - kErrorAreaHeight - kSpacing * 2;
    Rectangle editor_rect = {c.x, c.y, c.width, editor_h};
    Rectangle button_row = {c.x, c.y + editor_h + kSpacing, c.width, kButtonRowHeight};
    Rectangle error_rect = {c.x, button_row.y + kButtonRowHeight + kSpacing, c.width, kErrorAreaHeight};

    // Text editor
    ui::draw_text_editor(editor_rect, state.editor, 14, 500);

    // Buttons
    float btn_w = (button_row.width - 4.0f) * 0.5f;
    Rectangle compile_btn = {button_row.x, button_row.y, btn_w, kButtonRowHeight};
    Rectangle save_btn = {button_row.x + btn_w + 4.0f, button_row.y, btn_w, kButtonRowHeight};

    auto compile_state = ui::draw_button(compile_btn, "Compile", 14);
    auto save_state = ui::draw_button(save_btn, "Save", 14);

    if (compile_state.clicked) result.compile_clicked = true;
    if (save_state.clicked) result.save_clicked = true;

    // Error display area
    DrawRectangleRec(error_rect, with_alpha(g_theme->section, 200));
    DrawRectangleLinesEx(error_rect, 1.0f, g_theme->border_light);

    if (state.show_compile_result) {
        Rectangle error_content = ui::inset(error_rect, ui::edge_insets::uniform(6.0f));
        if (state.compile_success) {
            DrawText("OK", static_cast<int>(error_content.x), static_cast<int>(error_content.y),
                     14, Color{100, 200, 100, 255});
        } else {
            float y = error_content.y;
            int max_errors = std::min(static_cast<int>(state.errors.size()), 5);
            for (int i = 0; i < max_errors; i++) {
                const auto& err = state.errors[i];
                const char* text = TextFormat("L%d: %s", err.line, err.message.c_str());
                DrawText(text, static_cast<int>(error_content.x), static_cast<int>(y),
                         12, Color{255, 120, 100, 255});
                y += 14.0f;
            }
            if (static_cast<int>(state.errors.size()) > max_errors) {
                const char* more = TextFormat("... +%d more", static_cast<int>(state.errors.size()) - max_errors);
                DrawText(more, static_cast<int>(error_content.x), static_cast<int>(y),
                         12, g_theme->text_dim);
            }
        }
    } else {
        Rectangle error_content = ui::inset(error_rect, ui::edge_insets::uniform(6.0f));
        DrawText("Press Compile to check", static_cast<int>(error_content.x),
                 static_cast<int>(error_content.y), 12, g_theme->text_hint);
    }

    return result;
}
