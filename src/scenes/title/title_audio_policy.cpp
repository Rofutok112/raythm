#include "title/title_audio_policy.h"

namespace title_audio_policy {

resolved_state resolve(hub_mode mode, bool preview_active) {
    switch (mode) {
        case hub_mode::play:
        case hub_mode::online:
        case hub_mode::create:
            return {
                .music = music_source::preview_song,
                .spectrum = title_spectrum_visualizer::source::preview,
                .update_preview = true,
            };
        case hub_mode::title:
        case hub_mode::home:
            if (preview_active) {
                return {
                    .music = music_source::preview_song,
                    .spectrum = title_spectrum_visualizer::source::preview,
                    .update_preview = true,
                };
            }
            return {
                .music = music_source::theme_song,
                .spectrum = title_spectrum_visualizer::source::bgm,
                .update_preview = false,
            };
    }

    return {};
}

}  // namespace title_audio_policy
