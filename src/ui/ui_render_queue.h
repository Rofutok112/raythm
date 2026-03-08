//
// Created by rento on 2026/03/08.
//

#pragma once

#include <vector>
#include "raylib.h"

struct render_command {
    enum class type { rect, text, circle };
    type kind;
    Rectangle bounds;
    Color color;
};

class ui_render_queue {
public:
    void enqueue(const render_command &cmd);
    void clear();
    [[nodiscard]] std::pmr::vector<render_command> get_render_commands() const;
private:
    std::pmr::vector<render_command> commands_;
};
