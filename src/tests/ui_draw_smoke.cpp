#include "ui_draw.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <span>

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.001f;
}

void assert_rect(Rectangle rect, float x, float y, float width, float height) {
    assert(near(rect.x, x));
    assert(near(rect.y, y));
    assert(near(rect.width, width));
    assert(near(rect.height, height));
}

}  // namespace

int main() {
    const Rectangle row{100.0f, 200.0f, 500.0f, 80.0f};

    assert(ui::row_action_slot_index(ui::row_action_slot::right) == 0);
    assert(ui::row_action_slot_index(ui::row_action_slot::middle) == 1);
    assert(ui::row_action_slot_index(ui::row_action_slot::left) == 2);

    assert_rect(ui::row_action_rect(row, ui::row_action_slot::right), 494.0f, 221.0f, 92.0f, 38.0f);
    assert_rect(ui::row_action_rect(row, ui::row_action_slot::middle), 390.0f, 221.0f, 92.0f, 38.0f);
    assert_rect(ui::row_action_rect(row, ui::row_action_slot::left), 286.0f, 221.0f, 92.0f, 38.0f);
    assert_rect(ui::row_action_rect(row, 2), 286.0f, 221.0f, 92.0f, 38.0f);
    assert_rect(ui::row_action_rect(row, ui::row_action_slot::left), ui::row_action_rect(row, 2).x,
                ui::row_action_rect(row, 2).y, ui::row_action_rect(row, 2).width,
                ui::row_action_rect(row, 2).height);

    const ui::row_action_layout_options compact{
        .button_width = 44.0f,
        .button_height = 30.0f,
        .button_gap = 6.0f,
        .right_padding = 10.0f,
    };
    assert_rect(ui::row_action_rect(row, ui::row_action_slot::middle, compact), 496.0f, 225.0f, 44.0f, 30.0f);

    assert_rect(ui::icon_rect({10.0f, 20.0f, 80.0f, 40.0f}, 5.0f), 35.0f, 25.0f, 30.0f, 30.0f);

    enum class smoke_action {
        first,
        second,
    };
    const std::array<ui::action_button_definition<smoke_action>, 2> actions = {{
        {{10.0f, 20.0f, 30.0f, 40.0f}, "First", smoke_action::first, true},
        {{50.0f, 60.0f, 70.0f, 80.0f}, "Second", smoke_action::second, false},
    }};
    const std::span<const ui::action_button_definition<smoke_action>> action_span(actions);
    assert(action_span.size() == 2);
    assert(action_span[0].action == smoke_action::first);
    assert(action_span[1].enabled == false);
    assert_rect(action_span[1].rect, 50.0f, 60.0f, 70.0f, 80.0f);

    std::cout << "ui_draw smoke test passed\n";
    return 0;
}
