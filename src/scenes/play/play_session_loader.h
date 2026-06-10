#pragma once

#include <functional>
#include <string>

#include "play_note_draw_queue.h"
#include "play_session_types.h"

namespace play_session_loader {

using progress_callback = std::function<void(float progress, const std::string& message)>;

play_session_state load(const play_start_request& request,
                        play_note_draw_queue& draw_queue,
                        progress_callback progress = {});

}  // namespace play_session_loader
