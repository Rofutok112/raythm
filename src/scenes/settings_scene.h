#pragma once

#include <string>

#include "scene.h"

// 設定画面。Gameplay / Audio / Video / Key Config の4ページ構成。
class settings_scene final : public scene {
public:
    enum class return_target { title, song_select };

    explicit settings_scene(scene_manager& manager, return_target target = return_target::title);

    void on_enter() override;
    void update(float dt) override;
    void draw() override;

private:
    enum class page { gameplay, audio, video, key_config };
    enum class general_slider { none, note_speed, camera_angle, lane_width, bgm_volume, se_volume, frame_rate };

    void update_gameplay();
    void update_audio();
    void update_video();
    void update_key_config();
    void draw_gameplay();
    void draw_audio();
    void draw_video();
    void draw_key_config();

    page current_page_ = page::gameplay;

    // キーコンフィグ用状態
    int key_config_mode_ = 0;       // 0 = 4K, 1 = 6K
    int key_config_slot_ = -1;      // 選択中のスロット（-1 = 未選択）
    bool listening_ = false;        // キー入力待ち状態
    std::string key_config_error_;  // 重複エラーメッセージ
    float error_timer_ = 0.0f;     // エラー表示タイマー
    general_slider active_slider_ = general_slider::none;
    return_target return_target_ = return_target::title;
};
