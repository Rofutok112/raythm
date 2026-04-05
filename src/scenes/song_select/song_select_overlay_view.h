#pragma once

#include "ui_draw.h"
#include "song_select/song_select_state.h"

namespace song_select {

enum class context_menu_command {
    none,
    new_song,
    import_song,
    edit_song,
    new_chart,
    import_chart,
    export_song,
    request_delete_song,
    edit_chart,
    export_chart,
    request_delete_chart,
    close_menu,
};

struct context_menu_item_entry {
    ui::context_menu_item item;
    context_menu_command command_on_click;
};

context_menu_command draw_context_menu(const state& state);

}  // namespace song_select
