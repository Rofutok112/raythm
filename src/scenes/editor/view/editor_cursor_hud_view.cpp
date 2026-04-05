#include "editor/view/editor_cursor_hud_view.h"

#include "editor/view/editor_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace {
namespace layout = editor::layout;
}

void editor_cursor_hud_view::draw(const editor_cursor_hud_view_model& model) {
    if (!model.visible) {
        return;
    }

    const auto& t = *g_theme;
    const Rectangle hud_rect = layout::cursor_hud_rect();
    DrawRectangleRec(hud_rect, with_alpha(t.panel, 240));
    DrawRectangleLinesEx(hud_rect, 1.5f, t.border);
    ui::draw_text_f(TextFormat("bar %d:%d   beat %.2f   snap %d", model.measure, model.beat_index, model.beat, model.snapped_tick),
                    hud_rect.x + 12.0f, hud_rect.y + 8.0f, 18, t.text);
}
