#include "song_select/song_select_overlay_view.h"

#include <filesystem>
#include <vector>

#include "core/app_paths.h"
#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace song_select {

namespace {

context_menu_item_entry make_entry(const char* label, bool enabled, context_menu_command command) {
    return {{label, enabled, ui::context_menu_item::kind::action}, command};
}

std::vector<context_menu_item_entry> build_context_menu_entries(const state& state) {
    std::vector<context_menu_item_entry> entries;

    switch (state.context_menu.target) {
        case context_menu_target::list_background: {
            const bool has_any_song = !state.songs.empty();
            if (state.context_menu.section == context_menu_section::root) {
                entries = {
                    make_entry("SONG >", true, context_menu_command::open_song_section),
                    make_entry("CHART >", has_any_song, context_menu_command::open_chart_section),
                };
            } else if (state.context_menu.section == context_menu_section::song) {
                entries = {
                    make_entry("< BACK", true, context_menu_command::back_to_root),
                    make_entry("NEW SONG", true, context_menu_command::new_song),
                    make_entry("IMPORT SONG", true, context_menu_command::import_song),
                };
            } else if (state.context_menu.section == context_menu_section::chart) {
                entries = {
                    make_entry("< BACK", true, context_menu_command::back_to_root),
                    make_entry("IMPORT CHART", has_any_song, context_menu_command::import_chart),
                };
            }
            break;
        }
        case context_menu_target::song:
        case context_menu_target::chart: {
            const bool valid_song = state.context_menu.song_index >= 0 &&
                                    state.context_menu.song_index < static_cast<int>(state.songs.size());

            const bool can_edit_song = valid_song &&
                                       state.songs[static_cast<size_t>(state.context_menu.song_index)].song.can_edit;
            const bool can_export_song = valid_song &&
                state.songs[static_cast<size_t>(state.context_menu.song_index)].song.source != content_source::official;
            const bool can_delete_song = valid_song &&
                                         state.songs[static_cast<size_t>(state.context_menu.song_index)].song.can_delete;
            const bool can_add_chart_to_song = valid_song;

            bool valid_chart = false;
            bool can_edit_chart = false;
            bool can_delete_chart = false;
            if (valid_song) {
                const auto& charts = state.songs[static_cast<size_t>(state.context_menu.song_index)].charts;
                valid_chart = state.context_menu.chart_index >= 0 &&
                              state.context_menu.chart_index < static_cast<int>(charts.size());
                if (valid_chart) {
                    can_edit_chart = charts[static_cast<size_t>(state.context_menu.chart_index)].source != content_source::official;
                    can_delete_chart = charts[static_cast<size_t>(state.context_menu.chart_index)].can_delete;
                }
            }

            const bool has_mv = valid_song &&
                std::filesystem::exists(app_paths::script_path(
                    state.songs[static_cast<size_t>(state.context_menu.song_index)].song.meta.song_id));

            if (state.context_menu.target == context_menu_target::song) {
                if (state.context_menu.section == context_menu_section::root) {
                    entries = {
                        make_entry("SONG >", valid_song, context_menu_command::open_song_section),
                        make_entry("CHART >", valid_song, context_menu_command::open_chart_section),
                        make_entry("MV >", valid_song, context_menu_command::open_mv_section),
                    };
                } else if (state.context_menu.section == context_menu_section::song) {
                    entries = {
                        make_entry("< BACK", true, context_menu_command::back_to_root),
                        make_entry("EDIT META", can_edit_song, context_menu_command::edit_song),
                        make_entry("EXPORT SONG", can_export_song, context_menu_command::export_song),
                        make_entry("DELETE SONG", can_delete_song, context_menu_command::request_delete_song),
                    };
                } else if (state.context_menu.section == context_menu_section::chart) {
                    entries = {
                        make_entry("< BACK", true, context_menu_command::back_to_root),
                        make_entry("NEW CHART", can_add_chart_to_song, context_menu_command::new_chart),
                        make_entry("IMPORT CHART", valid_song, context_menu_command::import_chart),
                    };
                } else if (state.context_menu.section == context_menu_section::mv) {
                    entries = {
                        make_entry("< BACK", true, context_menu_command::back_to_root),
                        make_entry("IMPORT MV", valid_song, context_menu_command::import_mv),
                        make_entry("NEW MV", valid_song && !has_mv, context_menu_command::new_mv),
                        make_entry("EDIT MV", has_mv, context_menu_command::edit_mv),
                        make_entry("EXPORT MV", has_mv, context_menu_command::export_mv),
                        make_entry("DELETE MV", has_mv, context_menu_command::delete_mv),
                    };
                }
            } else if (state.context_menu.target == context_menu_target::chart) {
                if (state.context_menu.section == context_menu_section::root) {
                    entries = {
                        make_entry("CHART >", valid_song && valid_chart, context_menu_command::open_chart_section),
                    };
                } else if (state.context_menu.section == context_menu_section::chart) {
                    entries = {
                        make_entry("< BACK", true, context_menu_command::back_to_root),
                        make_entry("EDIT CHART", can_edit_chart, context_menu_command::edit_chart),
                        make_entry("EXPORT CHART", valid_song && valid_chart, context_menu_command::export_chart),
                        make_entry("DELETE CHART", can_delete_chart, context_menu_command::request_delete_chart),
                    };
                }
            }
            break;
        }
        default:
            break;
    }

    return entries;
}

}  // namespace

int context_menu_item_count(const state& state, context_menu_target target, context_menu_section section,
                            int song_index, int chart_index) {
    song_select::state temp_state = state;
    temp_state.context_menu.target = target;
    temp_state.context_menu.section = section;
    temp_state.context_menu.song_index = song_index;
    temp_state.context_menu.chart_index = chart_index;
    return static_cast<int>(build_context_menu_entries(temp_state).size());
}

context_menu_command draw_context_menu(const state& state) {
    if (!state.context_menu.open) {
        return context_menu_command::none;
    }

    const std::vector<context_menu_item_entry> entries = build_context_menu_entries(state);
    if (entries.empty()) {
        return context_menu_command::none;
    }

    std::vector<ui::context_menu_item> items;
    items.reserve(entries.size());
    for (const auto& e : entries) {
        items.push_back(e.item);
    }

    const auto [clicked_index] = ui::enqueue_context_menu(state.context_menu.rect, items,
                                                          layout::kContextMenuLayer, 16,
                                                          layout::kContextMenuItemHeight,
                                                          layout::kContextMenuItemSpacing);

    if (clicked_index >= 0 && clicked_index < static_cast<int>(entries.size())) {
        return entries[static_cast<size_t>(clicked_index)].command_on_click;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !ui::is_hovered(state.context_menu.rect, layout::kContextMenuLayer)) {
        return context_menu_command::close_menu;
    }
    return context_menu_command::none;
}

}  // namespace song_select
