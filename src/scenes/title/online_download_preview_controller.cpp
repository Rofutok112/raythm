#include "title/online_download_preview_controller.h"

#include <algorithm>

#include "audio_manager.h"
#include "title/online_download_internal.h"

namespace title_online_view::preview_controller {

bool update_scrub(state& state,
                  const song_entry_state* song,
                  Rectangle bar_rect,
                  Vector2 mouse,
                  bool left_pressed) {
    audio_manager& audio = audio_manager::instance();
    if (song != nullptr && left_pressed && CheckCollisionPointRec(mouse, bar_rect)) {
        state.preview_bar_dragging = true;
        state.preview_bar_resume_after_drag = audio.is_preview_playing();
        state.preview_bar_drag_position_seconds = audio.get_preview_position_seconds();
        audio.pause_preview();
    }

    if (song == nullptr || !state.preview_bar_dragging) {
        return false;
    }

    const double preview_length = detail::preview_display_length_seconds(*song);
    if (preview_length > 0.0 && audio.is_preview_loaded()) {
        const float ratio = std::clamp((mouse.x - bar_rect.x) / bar_rect.width, 0.0f, 1.0f);
        state.preview_bar_drag_position_seconds = preview_length * static_cast<double>(ratio);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        return true;
    }

    audio.seek_preview(state.preview_bar_drag_position_seconds);
    if (state.preview_bar_resume_after_drag) {
        audio.play_preview(false);
    }
    state.preview_bar_dragging = false;
    state.preview_bar_resume_after_drag = false;
    return true;
}

requested_action toggle_playback_action() {
    return audio_manager::instance().is_preview_playing()
        ? requested_action::stop_preview
        : requested_action::restart_preview;
}

}  // namespace title_online_view::preview_controller
