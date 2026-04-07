#pragma once

#include <string>
#include <vector>

#include "mv/lang/mv_sandbox.h"
#include "raylib.h"
#include "ui_text_editor.h"

struct editor_mv_script_panel_state {
    ui::text_editor_state editor;
    std::vector<mv::script_error> errors;
    bool compile_success = false;
    bool show_compile_result = false;
};

struct editor_mv_script_panel_model {
    Rectangle content_rect = {};
    Vector2 mouse = {};
};

struct editor_mv_script_panel_result {
    bool compile_clicked = false;
    bool save_clicked = false;
};

class editor_mv_script_panel {
public:
    static editor_mv_script_panel_result draw(
        const editor_mv_script_panel_model& model,
        editor_mv_script_panel_state& state);
};
