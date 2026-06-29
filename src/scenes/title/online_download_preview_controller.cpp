#include "title/online_download_preview_controller.h"

#include <algorithm>

#include "title/online_download_internal.h"
#include "ui_hit.h"

namespace title_online_view::preview_controller {

bool update_scrub(state& state,
                  const song_entry_state* song,
                  title_audio_controller& audio_controller,
                  Rectangle bar_rect,
                  Vector2 mouse,
                  bool left_pressed) {
    const title_preview_snapshot preview = audio_controller.preview_snapshot(
        song != nullptr ? &song->song : nullptr);
    if (song != nullptr && left_pressed && ui::contains_point(bar_rect, mouse)) {
        state.preview_bar_dragging = true;
        state.preview_bar_resume_after_drag = preview.playing;
        state.preview_bar_drag_position_seconds = preview.position_seconds;
        audio_controller.pause_preview();
    }

    if (song == nullptr || !state.preview_bar_dragging) {
        return false;
    }

    const double preview_length = detail::preview_display_length_seconds(*song, preview);
    if (preview_length > 0.0 &&
        preview.audio.status == song_select::preview_audio_loader::load_status::ready) {
        const float ratio = std::clamp((mouse.x - bar_rect.x) / bar_rect.width, 0.0f, 1.0f);
        state.preview_bar_drag_position_seconds = preview_length * static_cast<double>(ratio);
    }
    if (ui::is_mouse_button_down()) {
        return true;
    }

    audio_controller.seek_preview(state.preview_bar_drag_position_seconds);
    if (state.preview_bar_resume_after_drag) {
        audio_controller.play_preview_from_current();
    }
    state.preview_bar_dragging = false;
    state.preview_bar_resume_after_drag = false;
    return true;
}

requested_action toggle_playback_action(const song_entry_state* song,
                                        const title_audio_controller& audio_controller) {
    return audio_controller.preview_snapshot(song != nullptr ? &song->song : nullptr).playing
        ? requested_action::stop_preview
        : requested_action::restart_preview;
}

}  // namespace title_online_view::preview_controller
