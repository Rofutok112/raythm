#pragma once

#include <string>

#include "song_select/song_select_state.h"

namespace song_select {

enum class confirmation_command {
    none,
    confirm,
    cancel,
};

void open_confirmation_dialog(state& state, pending_confirmation_action action,
                              std::string title = "", std::string message = "",
                              std::string hint = "", std::string confirm_label = "CONFIRM",
                              int song_index = -1, int chart_index = -1);
confirmation_command draw_confirmation_dialog(const state& state);

}  // namespace song_select
