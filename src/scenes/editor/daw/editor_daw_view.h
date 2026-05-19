#pragma once

#include "editor/view/editor_header_view.h"
#include "editor/view/editor_left_panel_view.h"
#include "editor/view/editor_right_panel_view.h"
#include "editor/view/editor_timeline_presenter.h"

namespace editor::daw {

editor_left_panel_view_result draw_left_panel(const editor_left_panel_view_model& model);

editor_right_panel_view_result draw_right_panel(const editor_right_panel_view_model& model,
                                                editor_timing_panel_state& timing_state);

editor_header_view_result draw_header(const editor_header_view_model& model, Rectangle snap_menu_rect);

void draw_timeline(const editor_timeline_presenter_model& model);

}  // namespace editor::daw
