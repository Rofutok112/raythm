#pragma once

#include <optional>
#include <string>

#include "data_models.h"
#include "raylib.h"
#include "song_select/song_select_state.h"

namespace song_select {

class preview_controller {
public:
    preview_controller() = default;
    ~preview_controller();

    void select_song(const song_entry* song);
    void update(float dt, const song_entry* selected_song);
    void resume(const song_entry* song);
    void pause();
    void fade_out();
    void stop();
    [[nodiscard]] bool is_audio_active() const;
    [[nodiscard]] bool is_playing() const;

    [[nodiscard]] bool jacket_loaded() const {
        return jacket_loaded_;
    }

    [[nodiscard]] const Texture2D& jacket_texture() const {
        return jacket_texture_;
    }

private:
    void unload_jacket();
    void load_jacket(const song_entry* song);
    void queue_preview(const song_entry* song);
    void start_preview(const song_entry& song);

    std::optional<song_data> pending_preview_song_;
    std::optional<song_data> active_preview_song_;
    std::string preview_song_id_;
    std::string jacket_song_id_;
    float preview_volume_ = 0.0f;
    int preview_fade_direction_ = 0;
    Texture2D jacket_texture_{};
    bool jacket_loaded_ = false;
};

}  // namespace song_select
