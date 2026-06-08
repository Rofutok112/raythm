#pragma once

#include <string>

#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"
#include "title/title_audio_policy.h"
#include "title/title_bgm_controller.h"
#include "title/title_spectrum_visualizer.h"

struct title_preview_snapshot {
    bool loaded = false;
    bool loading = false;
    bool playing = false;
    double position_seconds = 0.0;
    double length_seconds = 0.0;
    bool jacket_loaded = false;
    const Texture2D* jacket_texture = nullptr;
};

class title_audio_controller final {
public:
    void configure(std::string intro_path, std::string loop_path);
    void on_enter();
    void on_exit();
    void update(title_audio_policy::hub_mode mode, const song_select::song_entry* selected_song, float dt);
    void update_preview_only(const song_select::song_entry* selected_song, float dt);
    void update_multiplayer_preview(const song_select::song_entry* selected_song, float dt);
    void draw_spectrum(const Rectangle& rect, float alpha_scale = 1.0f) const;

    void select_preview_song(const song_select::song_entry* song);
    void resume_preview_song(const song_select::song_entry* song);
    void pause_preview();
    void stop_preview();
    void toggle_preview_song(const song_select::song_entry* song);
    void seek_preview(double seconds);
    void play_preview_from_current();

    [[nodiscard]] title_audio_policy::resolved_state current_state() const;
    [[nodiscard]] title_preview_snapshot preview_snapshot(const song_select::song_entry* fallback_song = nullptr) const;

private:
    title_audio_policy::resolved_state resolve_state(title_audio_policy::hub_mode mode) const;

    song_select::preview_controller preview_controller_;
    title_bgm_controller bgm_controller_;
    title_spectrum_visualizer spectrum_visualizer_;
    title_audio_policy::resolved_state current_state_{};
};
