#pragma once

#include "editor/view/editor_header_view.h"
#include "editor/view/editor_left_panel_view.h"
#include "editor/view/editor_right_panel_view.h"
#include "editor/view/editor_timeline_presenter.h"

namespace editor::daw {

struct metadata_modal_result {
    editor_left_panel_view_result metadata_result;
    bool apply_requested = false;
    bool close_requested = false;
};

struct timing_modal_result {
    editor_timing_panel_result panel_result;
    bool offset_left_clicked = false;
    bool offset_right_clicked = false;
    bool close_requested = false;
};

editor_left_panel_view_result draw_left_panel(const editor_left_panel_view_model& model);

editor_header_view_result draw_header(const editor_header_view_model& model, Rectangle snap_menu_rect);

editor_right_panel_view_result draw_timeline(const editor_timeline_presenter_model& model,
                                             Rectangle snap_menu_rect,
                                             bool snap_dropdown_open);

metadata_modal_result draw_metadata_modal(const editor_left_panel_view_model& model);

timing_modal_result draw_timing_modal(const editor_right_panel_view_model& model,
                                      editor_timing_panel_state& timing_state,
                                      const char* offset_label);

}  // namespace editor::daw
