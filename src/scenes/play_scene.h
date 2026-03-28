#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "audio.h"
#include "judge_system.h"
#include "scene.h"
#include "score_system.h"
#include "song_loader.h"
#include "timing_engine.h"

// プレイ画面。譜面を読み込み、ノートの描画・入力判定・スコア計算を行うメインのゲームシーン。
class play_scene final : public scene {
public:
    explicit play_scene(scene_manager& manager, int key_count);
    play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    // 3D空間にレーンの板と判定ラインを描画する。
    void draw_lanes(const Camera3D& camera) const;
    // 譜面上のノート（タップ・ホールド）を3D空間に描画する。
    void draw_notes(const Camera3D& camera) const;
    // スコア・タイマー・ヘルスゲージ・コンボを2Dオーバーレイとして描画する。
    void draw_hud() const;
    // ポーズ中の半透明オーバーレイを描画する。
    void draw_pause_overlay() const;
    // 判定結果テキスト（PERFECT, GREAT 等）をフェード付きで描画する。
    void draw_judge_feedback() const;
    // 開始前のフェードアウト演出を描画する。
    void draw_intro_overlay() const;
    // 失敗時の暗転演出を描画する。
    void draw_failure_overlay() const;
    // カメラの高さ・角度・FOVから、判定ラインが画面上の指定位置に来るようカメラを構築する。
    Camera3D make_play_camera() const;
    // カメラの可視範囲からレーンのZ方向の手前端・判定位置・奥端を計算する。
    bool get_lane_view_bounds(const Camera3D& camera, float& lane_start_z, float& judgement_z, float& lane_end_z) const;
    // 描画キューを更新する。inactive から active へのノート移動と、active からの除去を行う。
    void update_draw_queues(float judgement_z, float lane_start_z, float lane_end_z, double visual_ms);
    // 開始前演出を含めた描画用の時刻を返す。
    double get_visual_ms() const;

    int key_count_ = 4;
    bool initialized_ = false;
    bool paused_ = false;
    bool ranking_enabled_ = true;
    bool auto_paused_by_focus_ = false;
    float camera_angle_degrees_ = 45.0f;
    double current_ms_ = 0.0;
    double paused_ms_ = 0.0;
    double song_end_ms_ = 0.0;
    double lane_speed_ = 0.045;
    int combo_display_ = 0;
    score_system score_system_;
    gauge gauge_;
    input_handler input_handler_;
    timing_engine timing_engine_;
    judge_system judge_system_;
    audio audio_player_;
    std::optional<chart_data> chart_data_;
    std::optional<song_data> song_data_;
    std::optional<std::string> selected_chart_path_;
    std::optional<judge_event> last_judge_;
    std::optional<judge_event> display_judge_;
    result_data final_result_;
    std::string status_text_;
    float judge_feedback_timer_ = 0.0f;
    bool intro_playing_ = true;
    float intro_timer_ = 2.0f;
    bool failure_transition_playing_ = false;
    float failure_transition_timer_ = 0.0f;

    // 描画用スライディングウィンドウ。
    // 各レーンごとに inactive / active を持ち、レーン内では target_ms 昇順を保証する。
    std::vector<double> note_target_ms_;
    std::vector<std::deque<size_t>> inactive_draw_notes_by_lane_;
    std::vector<std::vector<size_t>> active_draw_notes_by_lane_;
};
