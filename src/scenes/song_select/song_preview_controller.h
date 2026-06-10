#pragma once

#include <optional>

#include "data_models.h"
#include "raylib.h"
#include "song_select/selection_key.h"
#include "song_select/song_jacket_loader.h"
#include "song_select/song_preview_audio_loader.h"
#include "song_select/song_select_state.h"

namespace song_select {

class preview_controller {
public:
    preview_controller() = default;
    ~preview_controller();

    void select_song(const song_entry* song);
    void select_song(const selection_key& key, const song_entry* song);
    void request_jacket(const song_entry* song);
    void request_jacket(const selection_key& key, const song_entry* song);
    void request_audio(const song_entry* song);
    void request_audio(const selection_key& key, const song_entry* song);
    void update(float dt, const song_entry* selected_song);
    void update(float dt, const selection_key& current_key, const song_entry* selected_song);
    void resume(const song_entry* song);
    void resume(const selection_key& key, const song_entry* song);
    void pause();
    void fade_out();
    void stop();
    [[nodiscard]] bool is_audio_active() const;
    [[nodiscard]] bool is_playing() const;
    [[nodiscard]] bool is_loading() const;
    [[nodiscard]] preview_audio_loader::snapshot audio_snapshot() const;
    [[nodiscard]] jacket_loader::snapshot jacket_snapshot() const;

    [[nodiscard]] bool jacket_loaded() const {
        return jacket_loader_.loaded();
    }

    [[nodiscard]] const Texture2D& jacket_texture() const {
        return jacket_loader_.texture();
    }

private:
    void queue_preview(const song_entry* song);
    void queue_preview(const selection_key& key, const song_entry* song);
    void start_preview(const selection_key& key, const song_entry& song);

    std::optional<song_data> pending_preview_song_;
    std::optional<song_data> active_preview_song_;
    jacket_loader jacket_loader_;
    preview_audio_loader audio_loader_;
    float preview_volume_ = 0.0f;
    int preview_fade_direction_ = 0;
};

}  // namespace song_select
