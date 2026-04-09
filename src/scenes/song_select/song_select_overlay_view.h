#pragma once

#include "ui_draw.h"
#include "song_select/song_select_state.h"

namespace song_select {

enum class context_menu_command {
    none,
    open_song_section,
    open_chart_section,
    open_mv_section,
    back_to_root,
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
    import_mv,
    new_mv,
    edit_mv,
    delete_mv,
    export_mv,
    close_menu,
};

struct context_menu_item_entry {
    ui::context_menu_item item;
    context_menu_command command_on_click;
};

int context_menu_item_count(const state& state, context_menu_target target,
                            context_menu_section section = context_menu_section::root,
                            int song_index = -1, int chart_index = -1);
context_menu_command draw_context_menu(const state& state);

}  // namespace song_select
