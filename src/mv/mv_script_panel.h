#pragma once

#include <string>
#include <vector>

#include "mv/lang/mv_sandbox.h"
#include "raylib.h"
#include "ui_text_editor.h"

struct mv_script_panel_state {
    ui::text_editor_state editor;
    std::vector<mv::script_error> errors;
};

struct mv_script_panel_model {
    Rectangle content_rect = {};
    Vector2 mouse = {};
};

struct mv_script_panel_result {
    bool text_changed = false;
};

class mv_script_panel {
public:
    static mv_script_panel_result draw(
        const mv_script_panel_model& model,
        mv_script_panel_state& state);
};
