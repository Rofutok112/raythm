#pragma once

struct editor_cursor_hud_view_model {
    bool visible = false;
    int snapped_tick = 0;
    double beat = 0.0;
    int measure = 0;
    int beat_index = 0;
};

class editor_cursor_hud_view final {
public:
    static void draw(const editor_cursor_hud_view_model& model);
};
