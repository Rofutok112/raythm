#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "audio_manager.h"
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
    const chart_option* selected_chart_for(const std::vector<const chart_option*>& filtered) const;
    void draw_song_details(const song_entry& song, const chart_option* selected_chart,
                           float content_offset_x, unsigned char content_alpha) const;
    void draw_song_list(const std::vector<const chart_option*>& filtered) const;
    void draw_song_row(const song_entry& song, float item_y, bool is_selected, double now) const;
    void draw_chart_rows(const std::vector<const chart_option*>& filtered, float item_y) const;

    // スクロールに必要な総コンテンツ高さを計算する。
    float compute_content_height() const;

    std::vector<song_entry> songs_;
    std::vector<std::string> load_errors_;
    int selected_song_index_ = 0;
    int difficulty_index_ = 0;
    float scroll_y_ = 0.0f;
    float scroll_y_target_ = 0.0f;
    float song_change_anim_t_ = 0.0f;
    float scene_fade_in_t_ = 1.0f;
    std::optional<song_data> pending_preview_song_;
    std::optional<song_data> active_preview_song_;
    std::string preview_song_id_;
    float preview_volume_ = 0.0f;
    int preview_fade_direction_ = 0;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
    Texture2D jacket_texture_{};
    bool jacket_loaded_ = false;
};
