#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "audio.h"
#include "data_models.h"
#include "scene.h"

// 曲選択画面。難易度・キーモードを選んでプレイ画面に遷移する。
class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    struct chart_option {
        std::string path;
        chart_meta meta;
    };

    struct song_entry {
        song_data song;
        std::vector<chart_option> charts;
    };

    const song_entry* selected_song() const;
    std::vector<const chart_option*> filtered_charts_for_selected_song() const;
    void load_jacket_for_selected_song();
    void queue_preview_for_selected_song();
    void start_preview(const song_entry& song);
    void update_preview(float dt);

    // スクロールに必要な総コンテンツ高さを計算する。
    float compute_content_height() const;

    std::vector<song_entry> songs_;
    std::vector<std::string> load_errors_;
    int selected_song_index_ = 0;
    int difficulty_index_ = 0;
    float scroll_y_ = 0.0f;
    float scroll_y_target_ = 0.0f;
    float song_change_anim_t_ = 0.0f;
    audio preview_audio_;
    std::optional<song_data> pending_preview_song_;
    std::optional<song_data> active_preview_song_;
    std::string preview_song_id_;
    float preview_volume_ = 0.0f;
    int preview_fade_direction_ = 0;
    Texture2D jacket_texture_{};
    bool jacket_loaded_ = false;
};
