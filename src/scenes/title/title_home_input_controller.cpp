#include "title/title_home_input_controller.h"

#include "raylib.h"
#include "ui_hit.h"

namespace {

constexpr const char* kDisabledRouteMessage = "This route is still warming up.";

}  // namespace

title_home_input_controller::result title_home_input_controller::update(
    int& selected_index,
    std::string& status_message,
    float home_menu_anim,
    bool suppress_pointer_this_frame) {
    result output;
    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        output.consumed = true;
        output.enter_title = true;
        return output;
    }

    if (!suppress_pointer_this_frame) {
        for (int index = 0; index < static_cast<int>(title_home_view::entry_count()); ++index) {
            const Rectangle rect = title_home_view::button_rect(index, home_menu_anim);
            if (ui::is_hovered(rect)) {
                selected_index = index;
            }
            if (ui::is_clicked(rect)) {
                const title_home_view::entry& entry =
                    title_home_view::entry_at(static_cast<std::size_t>(index));
                output.consumed = true;
                if (entry.enabled) {
                    output.selected_action = entry.target;
                } else {
                    status_message = kDisabledRouteMessage;
                }
                return output;
            }
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        selected_index = (selected_index + 1) % static_cast<int>(title_home_view::entry_count());
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        selected_index = (selected_index - 1 + static_cast<int>(title_home_view::entry_count())) %
                         static_cast<int>(title_home_view::entry_count());
    }
    if (IsKeyPressed(KEY_ENTER)) {
        const title_home_view::entry& entry =
            title_home_view::entry_at(static_cast<std::size_t>(selected_index));
        output.consumed = true;
        if (entry.enabled) {
            output.selected_action = entry.target;
        } else {
            status_message = kDisabledRouteMessage;
        }
        return output;
    }

    return output;
}
