#include "ui_layout.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>
#include <span>

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.001f;
}

}  // namespace

int main() {
    const Rectangle bounds{10.0f, 20.0f, 300.0f, 200.0f};

    const Rectangle wide = ui::aspect_fit_rect(bounds, 400.0f, 100.0f);
    assert(near(wide.x, 10.0f));
    assert(near(wide.y, 82.5f));
    assert(near(wide.width, 300.0f));
    assert(near(wide.height, 75.0f));

    const Rectangle tall = ui::aspect_fit_rect(bounds, 100.0f, 400.0f);
    assert(near(tall.x, 135.0f));
    assert(near(tall.y, 20.0f));
    assert(near(tall.width, 50.0f));
    assert(near(tall.height, 200.0f));

    const Rectangle square = ui::centered_square(bounds, 80.0f);
    assert(near(square.x, 120.0f));
    assert(near(square.y, 80.0f));
    assert(near(square.width, 80.0f));
    assert(near(square.height, 80.0f));

    ui::vertical_layout_cursor cursor = ui::vertical_cursor({40.0f, 50.0f, 180.0f, 0.0f}, 7.0f);
    const Rectangle first_row = cursor.next(30.0f);
    assert(near(first_row.x, 40.0f));
    assert(near(first_row.y, 50.0f));
    assert(near(first_row.width, 180.0f));
    assert(near(first_row.height, 30.0f));
    const Rectangle second_row = cursor.next(48.0f);
    assert(near(second_row.y, 87.0f));
    assert(near(second_row.height, 48.0f));
    cursor.skip(11.0f);
    const Rectangle peeked_row = cursor.peek(12.0f);
    assert(near(peeked_row.y, 153.0f));
    assert(near(cursor.next(12.0f).y, 153.0f));

    ui::wrapped_layout_cursor wrapped = ui::wrapped_cursor({10.0f, 20.0f, 120.0f, 90.0f}, 8.0f, 32.0f);
    const std::optional<Rectangle> chip_a = wrapped.next(50.0f, 20.0f);
    assert(chip_a.has_value());
    assert(near(chip_a->x, 10.0f));
    assert(near(chip_a->y, 20.0f));
    const std::optional<Rectangle> chip_b = wrapped.next(60.0f, 20.0f);
    assert(chip_b.has_value());
    assert(near(chip_b->x, 68.0f));
    assert(near(chip_b->y, 20.0f));
    const std::optional<Rectangle> chip_c = wrapped.next(40.0f, 20.0f);
    assert(chip_c.has_value());
    assert(near(chip_c->x, 10.0f));
    assert(near(chip_c->y, 52.0f));
    wrapped.next(40.0f, 20.0f);
    wrapped.next(40.0f, 20.0f);
    wrapped.next(40.0f, 20.0f);
    const std::optional<Rectangle> clipped_chip = wrapped.next(40.0f, 20.0f);
    assert(!clipped_chip.has_value());

    const Rectangle inscribed = ui::inscribed_square({20.0f, 30.0f, 50.0f, 100.0f}, 5.0f);
    assert(near(inscribed.x, 25.0f));
    assert(near(inscribed.y, 60.0f));
    assert(near(inscribed.width, 40.0f));
    assert(near(inscribed.height, 40.0f));

    const Rectangle scaled = ui::scaled_from_center(
        {100.0f, 200.0f, 80.0f, 40.0f},
        {0.0f, 0.0f, 300.0f, 120.0f},
        0.5f,
        {20.0f, -10.0f});
    assert(near(scaled.x, 85.0f));
    assert(near(scaled.y, 180.0f));
    assert(near(scaled.width, 150.0f));
    assert(near(scaled.height, 60.0f));

    const Rectangle from_space{0.0f, 0.0f, 200.0f, 100.0f};
    const Rectangle to_space{20.0f, 30.0f, 400.0f, 300.0f};
    const Rectangle mapped = ui::map_rect_between({50.0f, 20.0f, 80.0f, 40.0f}, from_space, to_space);
    assert(near(mapped.x, 120.0f));
    assert(near(mapped.y, 90.0f));
    assert(near(mapped.width, 160.0f));
    assert(near(mapped.height, 120.0f));
    const Rectangle translated = ui::translated(mapped, -20.0f, 15.0f);
    assert(near(translated.x, 100.0f));
    assert(near(translated.y, 105.0f));
    assert(near(translated.width, mapped.width));
    assert(near(translated.height, mapped.height));
    const Rectangle translated_by_vector = ui::translated(mapped, {5.0f, -10.0f});
    assert(near(translated_by_vector.x, 125.0f));
    assert(near(translated_by_vector.y, 80.0f));

    const Vector2 point = ui::map_point_between({100.0f, 50.0f}, from_space, to_space);
    assert(near(point.x, 220.0f));
    assert(near(point.y, 180.0f));

    assert(near(ui::stack_content_width(0, 40.0f, 8.0f), 0.0f));
    assert(near(ui::stack_content_width(3, 40.0f, 8.0f), 136.0f));
    Rectangle centered_buttons[3]{};
    assert(ui::centered_hstack({10.0f, 20.0f, 200.0f, 80.0f},
                               40.0f,
                               30.0f,
                               8.0f,
                               std::span<Rectangle>(centered_buttons)) == 3);
    assert(near(centered_buttons[0].x, 42.0f));
    assert(near(centered_buttons[0].y, 45.0f));
    assert(near(centered_buttons[1].x, 90.0f));
    assert(near(centered_buttons[2].x, 138.0f));
    Rectangle no_buttons[1]{};
    assert(ui::centered_hstack({10.0f, 20.0f, 200.0f, 80.0f},
                               40.0f,
                               30.0f,
                               8.0f,
                               std::span<Rectangle>(no_buttons, 0)) == 0);

    std::cout << "ui_layout smoke test passed\n";
    return 0;
}
