#pragma once

#include "play_note_draw_queue.h"
#include "play_session_types.h"

namespace play_session_loader {

play_session_state load(const play_start_request& request, play_note_draw_queue& draw_queue);

}  // namespace play_session_loader
