#pragma once

#include "editor/editor_scene_types.h"

struct editor_modal_view_result {
    bool save_dialog_submit_requested = false;
};

class editor_modal_view final {
public:
    static void draw_unsaved_changes_dialog();
    static editor_modal_view_result draw_save_dialog(save_dialog_state& state);
    static void draw_key_count_confirmation(int pending_key_count);
};
