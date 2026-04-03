#pragma once

#include "song_select/song_select_state.h"

namespace song_select {

enum class context_menu_command {
    none,
    edit_song,
    new_chart,
    request_delete_song,
    edit_chart,
    request_delete_chart,
    close_menu,
};

enum class confirmation_command {
    none,
    confirm,
    cancel,
};

context_menu_command draw_context_menu(const state& state);
confirmation_command draw_confirmation_dialog(const state& state);

}  // namespace song_select
