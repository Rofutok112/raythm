#pragma once

#include <string>

#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"
#include "title/title_audio_policy.h"
#include "title/title_bgm_controller.h"
#include "title/title_spectrum_visualizer.h"

class title_audio_controller final {
public:
    void configure(std::string intro_path, std::string loop_path);
    void on_enter();
    void on_exit();
    void update(title_audio_policy::hub_mode mode, const song_select::song_entry* selected_song, float dt);
    void draw_spectrum(const Rectangle& rect, float alpha_scale = 1.0f) const;

    [[nodiscard]] title_audio_policy::resolved_state current_state() const;
    [[nodiscard]] song_select::preview_controller& preview();
    [[nodiscard]] const song_select::preview_controller& preview() const;

private:
    title_audio_policy::resolved_state resolve_state(title_audio_policy::hub_mode mode) const;

    song_select::preview_controller preview_controller_;
    title_bgm_controller bgm_controller_;
    title_spectrum_visualizer spectrum_visualizer_;
    title_audio_policy::resolved_state current_state_{};
};
