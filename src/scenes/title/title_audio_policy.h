#pragma once

#include "title/title_spectrum_visualizer.h"

namespace title_audio_policy {

enum class hub_mode {
    title,
    home,
    play,
    online,
    create,
};

enum class music_source {
    theme_song,
    preview_song,
};

struct resolved_state {
    music_source music = music_source::theme_song;
    title_spectrum_visualizer::source spectrum = title_spectrum_visualizer::source::bgm;
    bool update_preview = false;
};

resolved_state resolve(hub_mode mode, bool preview_active);

}  // namespace title_audio_policy
