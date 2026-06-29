#include "ui_scroll.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <span>

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.001f;
}

}  // namespace

int main() {
    const Rectangle viewport{10.0f, 20.0f, 200.0f, 100.0f};

    assert(near(ui::vertical_list_content_height(0, 30.0f, 5.0f), 0.0f));
    assert(near(ui::vertical_list_content_height(0, 30.0f, 5.0f, 100.0f), 100.0f));
    assert(near(ui::vertical_list_content_height(3, 30.0f, 5.0f), 100.0f));
    assert(near(ui::vertical_list_content_height_with_trailing_padding(3, 30.0f, 5.0f, 7.0f), 107.0f));
    assert(near(ui::vertical_list_content_height_with_trailing_padding(0, 30.0f, 5.0f, 7.0f), 0.0f));
    assert(ui::grid_row_count(0, 3) == 0);
    assert(ui::grid_row_count(7, 3) == 3);
    assert(ui::grid_row_count(7, 0) == 0);
    assert(near(ui::vertical_grid_content_height(7, 3, 40.0f, 6.0f), 132.0f));

    const Rectangle row = ui::vertical_list_row_rect(viewport, 2, 30.0f, 5.0f, 12.0f);
    assert(near(row.x, viewport.x));
    assert(near(row.y, 78.0f));
    assert(near(row.width, viewport.width));
    assert(near(row.height, 30.0f));
    const Rectangle span = ui::vertical_span_rect(viewport, 64.0f, 18.0f);
    assert(near(span.x, viewport.x));
    assert(near(span.y, 64.0f));
    assert(near(span.width, viewport.width));
    assert(near(span.height, 18.0f));
    const ui::vertical_stack_item stack_items[] = {
        {0.0f, 22.0f},
        {28.0f, 34.0f},
        {67.0f, 34.0f},
        {106.0f, 22.0f},
        {134.0f, 34.0f},
    };
    assert(near(ui::vertical_stack_content_height(std::span<const ui::vertical_stack_item>(stack_items)), 168.0f));
    assert(near(ui::vertical_stack_content_height(std::span<const ui::vertical_stack_item>(stack_items), 5.0f),
                173.0f));
    assert(near(ui::vertical_stack_content_height(5,
                                                  [&](int index) {
                                                      return stack_items[static_cast<std::size_t>(index)];
                                                  },
                                                  5.0f),
                173.0f));
    const std::span<const ui::vertical_stack_item> empty_stack;
    assert(near(ui::vertical_stack_content_height(empty_stack, 5.0f, 12.0f), 12.0f));
    const Rectangle stack_row = ui::vertical_stack_item_rect(viewport, stack_items[2], 12.0f);
    assert(near(stack_row.x, viewport.x));
    assert(near(stack_row.y, 75.0f));
    assert(near(stack_row.width, viewport.width));
    assert(near(stack_row.height, 34.0f));
    const ui::index_range stack_visible =
        ui::vertical_stack_visible_range(std::span<const ui::vertical_stack_item>(stack_items), viewport, 0.0f);
    assert(stack_visible.begin == 0);
    assert(stack_visible.end == 3);
    const ui::index_range stack_scrolled_visible =
        ui::vertical_stack_visible_range(std::span<const ui::vertical_stack_item>(stack_items), viewport, 70.0f);
    assert(stack_scrolled_visible.begin == 2);
    assert(stack_scrolled_visible.end == 5);
    const ui::index_range stack_slack_visible =
        ui::vertical_stack_visible_range(std::span<const ui::vertical_stack_item>(stack_items), viewport, 129.0f, 6.0f);
    assert(stack_slack_visible.begin == 3);
    assert(stack_slack_visible.end == 5);
    const Rectangle inset_viewport = ui::vertical_viewport_after_top_inset(viewport, 35.0f);
    assert(near(inset_viewport.x, viewport.x));
    assert(near(inset_viewport.y, 55.0f));
    assert(near(inset_viewport.width, viewport.width));
    assert(near(inset_viewport.height, 65.0f));
    const Rectangle over_inset_viewport = ui::vertical_viewport_after_top_inset(viewport, 140.0f);
    assert(near(over_inset_viewport.height, 0.0f));
    const Rectangle grid_item = ui::vertical_grid_item_rect(viewport, 5, 3, 50.0f, 40.0f, 8.0f, 6.0f, 10.0f);
    assert(near(grid_item.x, 126.0f));
    assert(near(grid_item.y, 56.0f));
    assert(near(grid_item.width, 50.0f));
    assert(near(grid_item.height, 40.0f));

    assert(ui::vertical_list_index_at_y(20.0f, viewport, 30.0f, 5.0f, 0.0f) == 0);
    assert(ui::vertical_list_index_at_y(49.0f, viewport, 30.0f, 5.0f, 0.0f) == 0);
    assert(ui::vertical_list_index_at_y(52.0f, viewport, 30.0f, 5.0f, 0.0f) == -1);
    assert(ui::vertical_list_index_at_y(55.0f, viewport, 30.0f, 5.0f, 0.0f) == 1);
    assert(ui::vertical_list_index_at_y(43.0f, viewport, 30.0f, 5.0f, 12.0f) == 1);

    assert(near(ui::max_scroll_offset(240.0f, viewport), 140.0f));
    assert(near(ui::max_scroll_offset(90.0f, viewport), 0.0f));
    assert(near(ui::clamp_scroll_offset(-12.0f, 140.0f), 0.0f));
    assert(near(ui::clamp_scroll_offset(160.0f, 140.0f), 140.0f));
    const ui::scroll_offset_state scrolled_state =
        ui::scroll_offset_state_for(viewport, 240.0f, 160.0f);
    assert(near(scrolled_state.content_height, 240.0f));
    assert(near(scrolled_state.max_scroll, 140.0f));
    assert(near(scrolled_state.offset, 140.0f));
    const ui::scroll_offset_state wheel_state =
        ui::wheel_scrolled_offset_state(viewport, {30.0f, 30.0f}, -1.0f, 240.0f, 20.0f, 12.0f);
    assert(near(wheel_state.max_scroll, 140.0f));
    assert(near(wheel_state.offset, 32.0f));
    const ui::scroll_offset_state outside_wheel_state =
        ui::wheel_scrolled_offset_state(viewport, {0.0f, 0.0f}, -1.0f, 240.0f, 20.0f, 12.0f);
    assert(near(outside_wheel_state.offset, 20.0f));

    assert(ui::rect_visible_in_viewport({0.0f, 119.0f, 10.0f, 10.0f}, viewport));
    assert(!ui::rect_visible_in_viewport({0.0f, 131.0f, 10.0f, 10.0f}, viewport));
    assert(ui::rect_visible_in_viewport({0.0f, 131.0f, 10.0f, 10.0f}, viewport, 12.0f));
    assert(ui::vertical_list_row_visible(viewport, 0, 30.0f, 5.0f, 0.0f));
    assert(ui::vertical_list_row_visible(viewport, 2, 30.0f, 5.0f, 0.0f));
    assert(!ui::vertical_list_row_visible(viewport, 3, 30.0f, 5.0f, 0.0f));
    assert(ui::vertical_list_row_visible(viewport, 3, 30.0f, 5.0f, 0.0f, 10.0f));
    const ui::index_range visible = ui::vertical_list_visible_range(8, viewport, 30.0f, 5.0f, 0.0f);
    assert(visible.begin == 0);
    assert(visible.end == 3);
    assert(!visible.empty());
    const ui::index_range slack_visible = ui::vertical_list_visible_range(8, viewport, 30.0f, 5.0f, 0.0f, 10.0f);
    assert(slack_visible.begin == 0);
    assert(slack_visible.end == 4);
    const ui::index_range scrolled_visible = ui::vertical_list_visible_range(8, viewport, 30.0f, 5.0f, 35.0f);
    assert(scrolled_visible.begin == 1);
    assert(scrolled_visible.end == 4);
    const ui::index_range boundary_visible = ui::vertical_list_visible_range(8, viewport, 30.0f, 5.0f, 30.0f);
    assert(boundary_visible.begin == 0);
    assert(boundary_visible.end == 4);
    const ui::index_range fitting = ui::vertical_list_fitting_range(8, viewport, 30.0f, 5.0f, 0.0f);
    assert(fitting.begin == 0);
    assert(fitting.end == 3);
    const ui::index_range fitting_scrolled = ui::vertical_list_fitting_range(8, viewport, 30.0f, 5.0f, 35.0f);
    assert(fitting_scrolled.begin == 1);
    assert(fitting_scrolled.end == 4);
    const ui::index_range fitting_boundary = ui::vertical_list_fitting_range(8, viewport, 30.0f, 5.0f, 30.0f);
    assert(fitting_boundary.begin == 1);
    assert(fitting_boundary.end == 3);
    const Rectangle row_viewport{10.0f, 60.0f, 200.0f, 100.0f};
    const ui::index_range clipped_visible =
        ui::vertical_list_visible_range(8, row_viewport, viewport, 30.0f, 5.0f, 0.0f);
    assert(clipped_visible.begin == 0);
    assert(clipped_visible.end == 2);
    const ui::index_range clipped_slack_visible =
        ui::vertical_list_visible_range(8, row_viewport, viewport, 30.0f, 5.0f, 0.0f, 6.0f);
    assert(clipped_slack_visible.begin == 0);
    assert(clipped_slack_visible.end == 2);
    const ui::index_range clipped_scrolled_visible =
        ui::vertical_list_visible_range(8, row_viewport, viewport, 30.0f, 5.0f, 40.0f);
    assert(clipped_scrolled_visible.begin == 0);
    assert(clipped_scrolled_visible.end == 3);
    const ui::index_range grid_visible =
        ui::vertical_grid_visible_index_range(10, viewport, 3, 30.0f, 5.0f, 0.0f);
    assert(grid_visible.begin == 0);
    assert(grid_visible.end == 9);
    const ui::index_range grid_slack_visible =
        ui::vertical_grid_visible_index_range(10, viewport, 3, 30.0f, 5.0f, 10.0f, 10.0f);
    assert(grid_slack_visible.begin == 0);
    assert(grid_slack_visible.end == 10);
    const ui::index_range grid_scrolled_visible =
        ui::vertical_grid_visible_index_range(10, viewport, 3, 30.0f, 5.0f, 35.0f);
    assert(grid_scrolled_visible.begin == 3);
    assert(grid_scrolled_visible.end == 10);
    const ui::index_range grid_empty_visible =
        ui::vertical_grid_visible_index_range(0, viewport, 3, 30.0f, 5.0f, 0.0f);
    assert(grid_empty_visible.empty());
    const ui::index_range grid_invalid_visible =
        ui::vertical_grid_visible_index_range(10, viewport, 0, 30.0f, 5.0f, 0.0f);
    assert(grid_invalid_visible.empty());
    const ui::index_range clipped_fitting =
        ui::vertical_list_fitting_range(8, row_viewport, viewport, 30.0f, 5.0f, 0.0f);
    assert(clipped_fitting.begin == 0);
    assert(clipped_fitting.end == 1);
    const ui::index_range clipped_scrolled_fitting =
        ui::vertical_list_fitting_range(8, row_viewport, viewport, 30.0f, 5.0f, 40.0f);
    assert(clipped_scrolled_fitting.begin == 0);
    assert(clipped_scrolled_fitting.end == 3);
    const ui::index_range empty_visible = ui::vertical_list_visible_range(0, viewport, 30.0f, 5.0f, 0.0f);
    assert(empty_visible.empty());
    const ui::index_range empty_fitting = ui::vertical_list_fitting_range(0, viewport, 30.0f, 5.0f, 0.0f);
    assert(empty_fitting.empty());
    const ui::index_range invalid_visible = ui::vertical_list_visible_range(8, viewport, 0.0f, 5.0f, 0.0f);
    assert(invalid_visible.empty());
    const ui::index_range invalid_fitting = ui::vertical_list_fitting_range(8, viewport, 0.0f, 5.0f, 0.0f);
    assert(invalid_fitting.empty());
    assert(ui::rect_fits_vertically_in_viewport({10.0f, 25.0f, 20.0f, 30.0f}, viewport));
    assert(!ui::rect_fits_vertically_in_viewport({10.0f, 95.0f, 20.0f, 30.0f}, viewport));
    assert(ui::vertical_span_rect_fits_in_viewport(viewport, 25.0f, 30.0f));
    assert(!ui::vertical_span_rect_fits_in_viewport(viewport, 95.0f, 30.0f));
    assert(ui::vertical_list_row_fits(viewport, 0, 30.0f, 5.0f, 0.0f));
    assert(!ui::vertical_list_row_fits(viewport, 3, 30.0f, 5.0f, 0.0f));
    assert(ui::vertical_list_row_fits(viewport, 2, 30.0f, 5.0f, 35.0f));
    assert(near(ui::vertical_list_scroll_offset_with_index_visible(
                    0.0f, 5, 8, viewport.height, 30.0f, 5.0f),
                105.0f));
    assert(near(ui::vertical_list_scroll_offset_with_index_visible(
                    140.0f, 1, 8, viewport.height, 30.0f, 5.0f),
                35.0f));
    assert(near(ui::vertical_list_scroll_offset_with_index_visible(
                    42.0f, -1, 8, viewport.height, 30.0f, 5.0f),
                42.0f));

    std::cout << "ui_scroll smoke test passed\n";
    return 0;
}
