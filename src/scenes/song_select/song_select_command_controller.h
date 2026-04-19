#pragma once

#include <functional>

#include "core/scene_manager.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_overlay_view.h"
#include "song_select/song_select_state.h"
#include "song_select/song_transfer_controller.h"

namespace song_select::commands {

using apply_transfer_result_fn = std::function<void(const transfer_result&)>;
using apply_delete_result_fn = std::function<void(const delete_result&)>;
using reload_song_library_fn = std::function<void(const std::string&, const std::string&)>;
using open_overwrite_song_confirmation_fn = std::function<void(song_import_request)>;
using open_overwrite_chart_confirmation_fn = std::function<void(chart_import_request)>;

void apply_context_menu_command(scene_manager& manager, state& state,
                                transfer::controller& transfer_controller,
                                context_menu_command command,
                                const apply_transfer_result_fn& apply_transfer_result,
                                const reload_song_library_fn& reload_song_library,
                                const open_overwrite_song_confirmation_fn& open_overwrite_song_confirmation,
                                const open_overwrite_chart_confirmation_fn& open_overwrite_chart_confirmation);

void apply_confirmation_command(state& state,
                                preview_controller& preview_controller,
                                transfer::controller& transfer_controller,
                                confirmation_command command,
                                const apply_delete_result_fn& apply_delete_result,
                                const apply_transfer_result_fn& apply_transfer_result);

}  // namespace song_select::commands
