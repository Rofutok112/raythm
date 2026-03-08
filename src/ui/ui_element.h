//
// Created by rento on 2026/03/08.
//

#pragma once
#include "raylib.h"

class ui_element {
public:
    virtual ~ui_element() = default;
    Rectangle bounds;
    virtual void build_render_data() const = 0;
};
