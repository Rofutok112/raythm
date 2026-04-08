#include "mv_script_panel.h"

#include "mv/mv_script_editor_style.h"

mv_script_panel_result mv_script_panel::draw(
    const mv_script_panel_model& model,
    mv_script_panel_state& state) {

    mv_script_panel_result result;
    const Rectangle& c = model.content_rect;

    // Text editor
    auto editor_result = ui::draw_text_editor(c, state.editor, 500,
                         mv::mv_script_editor_style(), mv::highlight_mv_script_line,
                         mv::complete_mv_script_line);
    result.text_changed = editor_result.changed;

    return result;
}
