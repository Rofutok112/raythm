//
// Created by rento on 2026/03/08.
//

#ifndef RAYTHM_BUTTON_H
#define RAYTHM_BUTTON_H

#include "ui_element.h"

class ui_render_queue;

class button final : public ui_element {
public:
    button(Rectangle rect, Color color);

    void set_color(Color color);
    [[nodiscard]] Color color() const;
    void build_render_data(ui_render_queue &queue) const override;

private:
    Color color_;
};


#endif //RAYTHM_BUTTON_H
