#pragma once

#include <optional>
#include <string>

#include "title/home_menu_view.h"

class title_home_input_controller {
public:
    struct result {
        bool consumed = false;
        bool enter_title = false;
        std::optional<title_home_view::action> selected_action;
    };

    static result update(int& selected_index,
                         std::string& status_message,
                         float home_menu_anim,
                         bool suppress_pointer_this_frame);
};
