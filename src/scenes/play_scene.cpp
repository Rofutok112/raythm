#include "play_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "game_settings.h"
#include "raylib.h"
#include "raymath.h"
#include "result_scene.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "virtual_screen.h"

namespace {

constexpr float kLaneGap = 0.2f;
constexpr float kJudgeLineGlowHeight = 0.04f;
constexpr float kIntroDurationSeconds = 2.0f;
constexpr float kFailureFadeDurationSeconds = 1.0f;
constexpr float kFailureHoldDurationSeconds = 1.0f;
constexpr float kFailureTransitionDurationSeconds
    = kFailureFadeDurationSeconds + kFailureHoldDurationSeconds;
constexpr float kJudgementLineScreenRatioFromBottom = 0.10f;
constexpr float kCameraHeight = 42.0f;
constexpr float kCameraFovY = 42.0f;
constexpr float kJudgeLineWorldZ = 12.0f;
constexpr float kMaxGroundDistance = 1000.0f;

constexpr Color kLaneColor = {182, 186, 194, 255};

// assets/songs から最初の曲パッケージを読み込んで返す。
std::optional<song_data> load_sample_song() {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    song_load_result result = song_loader::load_all((repo_root / "assets" / "songs").string());
    if (result.songs.empty()) {
        return std::nullopt;
    }
    return result.songs.front();
}

// 指定キー数に一致する譜面をパースして返す。見つからなければ nullopt。
std::optional<chart_data> load_chart_for_key_count(const song_data& song, int key_count) {
    for (const std::string& chart_path : song.chart_paths) {
        const chart_parse_result parse_result = song_loader::load_chart(chart_path);
        if (parse_result.success && parse_result.data.has_value() &&
            parse_result.data->meta.key_count == key_count) {
            return parse_result.data;
        }
    }
    return std::nullopt;
}

// レーン番号からワールド空間のX座標中心を返す。レーン群は原点を中心に左右対称に配置される。
float lane_center_x(int lane, int key_count) {
    const float total_width = key_count * g_settings.lane_width + (key_count - 1) * kLaneGap;
    const float left = -total_width * 0.5f + g_settings.lane_width * 0.5f;
    const int visual_lane = key_count - 1 - lane;
    return left + visual_lane * (g_settings.lane_width + kLaneGap);
}

// キー押下中のレーン色（やや暗くする）。
Color darkened_lane_color(Color color) {
    return {static_cast<unsigned char>(color.r * 0.7f), static_cast<unsigned char>(color.g * 0.7f),
            static_cast<unsigned char>(color.b * 0.72f), color.a};
}

// 判定結果に対応する表示色を返す。
Color judge_color(judge_result result) {
    switch (result) {
        case judge_result::perfect:
            return {239, 244, 255, 255};
        case judge_result::great:
            return {123, 211, 255, 255};
        case judge_result::good:
            return {141, 211, 173, 255};
        case judge_result::bad:
            return {255, 190, 92, 255};
        case judge_result::miss:
            return {255, 107, 107, 255};
    }
    return RAYWHITE;
}

// 判定結果に対応する表示テキストを返す。
const char* judge_text(judge_result result) {
    switch (result) {
        case judge_result::perfect:
            return "PERFECT";
        case judge_result::great:
            return "GREAT";
        case judge_result::good:
            return "GOOD";
        case judge_result::bad:
            return "BAD";
        case judge_result::miss:
            return "MISS";
    }
    return "";
}

// カメラ角度（度）から視線方向ベクトルを生成する。
// 90度で真下、0度で水平。Y軸が上方向の座標系。
Vector3 build_camera_forward(float camera_angle_degrees) {
    const float angle_rad = std::clamp(camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    return Vector3{0.0f, -std::sin(angle_rad), std::cos(angle_rad)};
}

// 視線方向がほぼ真上/真下のとき、upベクトルが平行にならないよう切り替える。
Vector3 choose_camera_up(Vector3 forward) {
    return std::fabs(Vector3DotProduct(forward, Vector3{0.0f, 1.0f, 0.0f})) > 0.98f ? Vector3{0.0f, 0.0f, 1.0f}
                                                                                    : Vector3{0.0f, 1.0f, 0.0f};
}

// カメラの高さ・俯角・FOVから、画面上の縦位置に対応する地面上のZオフセットを求める。
// screen_ndc_y: -1（画面下端）～ +1（画面上端）
// 地面と交差しない（水平以上のレイ）場合は nullopt を返す。
//
// 導出:
//   カメラ俯角 a（水平から下向き）、半画角 h とすると、
//   画面位置 ndc_y におけるレイ方向は forward + corrected_up * (ndc_y * tan(h))。
//   corrected_up = {0, cos(a), sin(a)} なので:
//     dir = {0, -sin(a) + ndc_y*tan(h)*cos(a), cos(a) + ndc_y*tan(h)*sin(a)}
//   Y=0 平面との交点から z_offset = H * (cos(a) + k*sin(a)) / (sin(a) - k*cos(a))
//   ただし k = ndc_y * tan(h)
std::optional<float> ground_z_offset(float height, float angle_rad, float half_fov_rad, float screen_ndc_y) {
    const float k = screen_ndc_y * std::tan(half_fov_rad);
    const float sin_a = std::sin(angle_rad);
    const float cos_a = std::cos(angle_rad);
    const float denominator = sin_a - k * cos_a;
    if (denominator <= 0.0001f) {
        return std::nullopt;
    }
    return height * (cos_a + k * sin_a) / denominator;
}

}  // namespace

play_scene::play_scene(scene_manager& manager, int key_count) : scene(manager), key_count_(key_count) {
}

play_scene::play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count)
    : scene(manager), key_count_(key_count), song_data_(std::move(song)), selected_chart_path_(std::move(chart_path)) {
}

// 譜面・オーディオの読み込みとゲーム状態の初期化を行う。
void play_scene::on_enter() {
    camera_angle_degrees_ = g_settings.camera_angle_degrees;
    lane_speed_ = g_settings.note_speed;
    input_handler_ = input_handler(g_settings.keys);
    input_handler_.set_key_count(key_count_);

    if (!song_data_.has_value()) {
        song_data_ = load_sample_song();
        if (!song_data_.has_value()) {
            status_text_ = "No playable song package found";
            initialized_ = false;
            return;
        }
    }

    if (selected_chart_path_.has_value()) {
        const chart_parse_result parse_result = song_loader::load_chart(*selected_chart_path_);
        if (parse_result.success && parse_result.data.has_value()) {
            chart_data_ = parse_result.data;
        } else {
            status_text_ = "Failed to load selected chart";
            initialized_ = false;
            return;
        }
    } else {
        chart_data_ = load_chart_for_key_count(*song_data_, key_count_);
    }

    if (!chart_data_.has_value()) {
        status_text_ = "No chart found for selected key mode";
        initialized_ = false;
        return;
    }

    timing_engine_.init(chart_data_->timing_events, chart_data_->meta.resolution);
    judge_system_.init(chart_data_->notes, timing_engine_);
    score_system_.init(static_cast<int>(chart_data_->notes.size()));
    gauge_ = gauge{};

    const std::filesystem::path audio_path =
        std::filesystem::path(song_data_->directory) / song_data_->meta.audio_file;
    audio_player_.load(audio_path.string());

    // 描画キューの初期化: 各レーンごとにノート index を集め、target_ms 昇順で inactive に積む
    const std::vector<note_state>& init_states = judge_system_.note_states();
    note_target_ms_.resize(init_states.size());
    inactive_draw_notes_by_lane_.assign(static_cast<size_t>(key_count_), {});
    active_draw_notes_by_lane_.assign(static_cast<size_t>(key_count_), {});
    for (size_t i = 0; i < init_states.size(); ++i) {
        note_target_ms_[i] = init_states[i].target_ms;
        const int lane = init_states[i].note_ref.lane;
        if (lane >= 0 && lane < key_count_) {
            inactive_draw_notes_by_lane_[static_cast<size_t>(lane)].push_back(i);
        }
    }

    for (int lane = 0; lane < key_count_; ++lane) {
        std::vector<size_t> sorted_indices(inactive_draw_notes_by_lane_[static_cast<size_t>(lane)].begin(),
                                           inactive_draw_notes_by_lane_[static_cast<size_t>(lane)].end());
        std::sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t left, size_t right) {
            if (note_target_ms_[left] != note_target_ms_[right]) {
                return note_target_ms_[left] < note_target_ms_[right];
            }
            return left < right;
        });
        inactive_draw_notes_by_lane_[static_cast<size_t>(lane)] =
            std::deque<size_t>(sorted_indices.begin(), sorted_indices.end());
    }

    // 譜面中の最後のノート位置から曲終了時刻を算出する
    int last_tick = 0;
    for (const note_data& note : chart_data_->notes) {
        last_tick = std::max(last_tick, note.type == note_type::hold ? note.end_tick : note.tick);
    }

    song_end_ms_ = std::max(timing_engine_.tick_to_ms(last_tick) + 2000.0,
                            audio_player_.get_length_seconds() * 1000.0);
    current_ms_ = 0.0;
    paused_ms_ = 0.0;
    paused_ = false;
    ranking_enabled_ = true;
    auto_paused_by_focus_ = false;
    initialized_ = true;
    status_text_.clear();
    last_judge_.reset();
    display_judge_.reset();
    final_result_ = {};
    intro_playing_ = true;
    intro_timer_ = kIntroDurationSeconds;
    failure_transition_playing_ = false;
    failure_transition_timer_ = 0.0f;
}

// オーディオを停止する。
void play_scene::on_exit() {
    audio_player_.stop();
}

// 時刻進行・入力処理・判定更新・シーン遷移を行う。
void play_scene::update(float dt) {
    judge_feedback_timer_ = std::max(0.0f, judge_feedback_timer_ - dt);

    if (!initialized_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        }
        return;
    }

    // ウィンドウフォーカスを失ったら自動ポーズ
    if (!IsWindowFocused()) {
        if (!paused_) {
            paused_ = true;
            auto_paused_by_focus_ = true;
            ranking_enabled_ = false;
            paused_ms_ = current_ms_;
            audio_player_.pause();
        }
    }

    // ESC でポーズ切り替え
    if (IsKeyPressed(KEY_ESCAPE)) {
        paused_ = !paused_;
        paused_ms_ = current_ms_;
        if (paused_) {
            ranking_enabled_ = false;
            audio_player_.pause();
        } else if (audio_player_.is_loaded() && !intro_playing_) {
            audio_player_.play(false);
            auto_paused_by_focus_ = false;
        }
    }

    if (paused_) {
        return;
    }

    if (failure_transition_playing_) {
        failure_transition_timer_ = std::max(0.0f, failure_transition_timer_ - dt);
        if (failure_transition_timer_ <= 0.0f) {
            manager_.change_scene(std::make_unique<result_scene>(manager_, final_result_, ranking_enabled_));
        }
        return;
    }

    if (intro_playing_) {
        intro_timer_ = std::max(0.0f, intro_timer_ - dt);
        input_handler_.update();
        const Camera3D cam = make_play_camera();
        float ls = 0.0f, jz = 0.0f, le = 0.0f;
        if (get_lane_view_bounds(cam, ls, jz, le)) {
            update_draw_queues(jz, ls, le, get_visual_ms());
        }

        if (intro_timer_ <= 0.0f) {
            intro_playing_ = false;
            if (audio_player_.is_loaded()) {
                audio_player_.play();
            }
        }
        return;
    }

    // 時刻をオーディオ位置から取得（オーディオ無しなら dt で進行）
    current_ms_ = audio_player_.is_loaded() ? audio_player_.get_position_seconds() * 1000.0 : current_ms_ + dt * 1000.0;
    input_handler_.update();
    judge_system_.update(current_ms_, input_handler_);
    last_judge_ = judge_system_.get_last_judge();
    if (last_judge_.has_value()) {
        score_system_.on_judge(*last_judge_);
        gauge_.on_judge(last_judge_->result);
        combo_display_ = score_system_.get_combo();
        display_judge_ = last_judge_;
        judge_feedback_timer_ = 1.0f;
    }

    if (gauge_.get_value() <= 0.0f) {
        final_result_ = score_system_.get_result_data();
        final_result_.failed = true;
        ranking_enabled_ = false;
        failure_transition_playing_ = true;
        failure_transition_timer_ = kFailureTransitionDurationSeconds;
        audio_player_.pause();
        return;
    }

    // 描画キューの更新
    {
        const Camera3D cam = make_play_camera();
        float ls = 0.0f, jz = 0.0f, le = 0.0f;
        if (get_lane_view_bounds(cam, ls, jz, le)) {
            update_draw_queues(jz, ls, le, get_visual_ms());
        }
    }

    // 曲終了でリザルト画面へ、Backspace で曲選択へ戻る
    if (current_ms_ >= song_end_ms_) {
        final_result_ = score_system_.get_result_data();
        manager_.change_scene(std::make_unique<result_scene>(manager_, final_result_, ranking_enabled_));
    } else if (IsKeyPressed(KEY_BACKSPACE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    }
}

// 判定ラインが画面下端から指定比率の位置に来るようカメラを構築する。
Camera3D play_scene::make_play_camera() const {
    const float angle_rad = std::clamp(camera_angle_degrees_, 5.0f, 90.0f) * DEG2RAD;
    const float half_fov_rad = kCameraFovY * DEG2RAD * 0.5f;

    // 判定ラインの画面位置を NDC に変換 (画面下端=-1, 上端=+1)
    const float judge_ndc_y = (kJudgementLineScreenRatioFromBottom - 0.5f) * 2.0f;
    const std::optional<float> judge_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, judge_ndc_y);
    const float camera_z = judge_offset.has_value() ? (kJudgeLineWorldZ - *judge_offset) : 0.0f;

    const Vector3 forward = build_camera_forward(camera_angle_degrees_);
    const Vector3 up = choose_camera_up(forward);

    Camera3D camera = {};
    camera.position = {0.0f, kCameraHeight, camera_z};
    camera.target = Vector3Add(camera.position, forward);
    camera.up = up;
    camera.fovy = kCameraFovY;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

// 画面の上端・下端が地面と交差するZ座標からレーンの描画範囲を決定する。
bool play_scene::get_lane_view_bounds(const Camera3D& camera, float& lane_start_z, float& judgement_z,
                                      float& lane_end_z) const {
    const float angle_rad = std::clamp(camera_angle_degrees_, 5.0f, 90.0f) * DEG2RAD;
    const float half_fov_rad = kCameraFovY * DEG2RAD * 0.5f;

    // 画面下端 (ndc_y=-1) → 手前側
    const std::optional<float> near_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, -1.0f);
    if (!near_offset.has_value()) {
        return false;
    }

    judgement_z = kJudgeLineWorldZ;
    lane_start_z = camera.position.z + *near_offset;

    // 画面上端 (ndc_y=+1) → 奥側。低角度だと交差しないため上限で打ち切る
    const std::optional<float> far_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, 1.0f);
    if (far_offset.has_value()) {
        lane_end_z = std::min(camera.position.z + *far_offset, camera.position.z + kMaxGroundDistance);
    } else {
        lane_end_z = camera.position.z + kMaxGroundDistance;
    }

    if (lane_end_z <= lane_start_z) {
        return false;
    }

    // 判定ラインが必ずレーン範囲内に収まるよう補正
    lane_start_z = std::min(lane_start_z, judgement_z - 0.5f);
    lane_end_z = std::max(lane_end_z, judgement_z + 8.0f);
    return true;
}

// 3Dシーン・2D HUD・ポーズオーバーレイの描画を統括する。
// 3D は物理解像度で直接描画し、2D HUD は仮想スクリーン経由でスケーリングする。
void play_scene::draw() {
    if (!initialized_) {
        virtual_screen::begin();
        ClearBackground(RAYWHITE);
        DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {255, 255, 255, 255}, {241, 243, 246, 255});
        DrawText("Play", 96, 90, 44, {220, 38, 38, 255});
        DrawText(status_text_.c_str(), 96, 170, 28, {230, 232, 238, 255});
        DrawText("ESC: Back to Song Select", 96, 225, 22, {138, 148, 166, 255});
        virtual_screen::end();

        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    // 3D を物理解像度で直接描画
    ClearBackground(RAYWHITE);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), {255, 255, 255, 255}, {241, 243, 246, 255});

    const Camera3D camera = make_play_camera();
    BeginMode3D(camera);
    draw_lanes(camera);
    draw_notes(camera);
    EndMode3D();

    // 2D HUD を仮想スクリーンに描画して透過合成
    virtual_screen::begin();
    ClearBackground(BLANK);
    draw_hud();
    draw_judge_feedback();
    if (intro_playing_) {
        draw_intro_overlay();
    }
    if (failure_transition_playing_) {
        draw_failure_overlay();
    }
    if (paused_) {
        draw_pause_overlay();
    }
    virtual_screen::end();

    virtual_screen::draw_to_screen(true);
}

void play_scene::draw_lanes(const Camera3D& camera) const {
    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    if (!get_lane_view_bounds(camera, lane_start_z, judgement_z, lane_end_z)) {
        return;
    }

    // 各レーンの板を描画（押下中は暗くする）
    for (int lane = 0; lane < key_count_; ++lane) {
        const float center_x = lane_center_x(lane, key_count_);
        const bool lane_held = input_handler_.is_lane_held(lane);
        const Color lane_fill = lane_held ? darkened_lane_color(kLaneColor) : kLaneColor;
        DrawCube({center_x, -0.08f, (lane_start_z + lane_end_z) * 0.5f}, g_settings.lane_width, 0.05f,
                 lane_end_z - lane_start_z, lane_fill);
        DrawCubeWires({center_x, -0.08f, (lane_start_z + lane_end_z) * 0.5f}, g_settings.lane_width, 0.05f,
                      lane_end_z - lane_start_z, {120, 130, 148, 180});
    }

    // 判定ライン（半透明の板 + グロー）を描画
    const float total_width = key_count_ * g_settings.lane_width + (key_count_ - 1) * kLaneGap;
    DrawCube({0.0f, 0.01f, judgement_z}, total_width + 0.9f, 0.01f, 0.62f, {90, 150, 190, 110});
    DrawCube({0.0f, 0.02f, judgement_z}, total_width + 0.5f, kJudgeLineGlowHeight, 0.38f, {180, 230, 255, 200});
}

void play_scene::update_draw_queues(float judgement_z, float lane_start_z, float lane_end_z, double visual_ms) {
    // Inactive → Active: 各レーンの先頭ノートのZ座標が画面奥端以内なら移動
    for (int lane = 0; lane < key_count_; ++lane) {
        std::deque<size_t>& inactive = inactive_draw_notes_by_lane_[static_cast<size_t>(lane)];
        std::vector<size_t>& active = active_draw_notes_by_lane_[static_cast<size_t>(lane)];
        while (!inactive.empty()) {
            const size_t idx = inactive.front();
            const float head_z = static_cast<float>(judgement_z + lane_speed_ * (note_target_ms_[idx] - visual_ms));
            if (head_z > lane_end_z) {
                break;
            }
            inactive.pop_front();
            active.push_back(idx);
        }
    }

    // Active から除去: 各レーンについて、判定済み（ホールド中を除く）または画面手前端を超えたノート
    const std::vector<note_state>& states = judge_system_.note_states();
    for (int lane = 0; lane < key_count_; ++lane) {
        std::vector<size_t>& active = active_draw_notes_by_lane_[static_cast<size_t>(lane)];
        std::erase_if(active, [&](size_t idx) {
            const note_state& s = states[idx];
            if (s.judged && !s.holding) {
                return true;
            }
            if (s.holding) {
                return false;
            }
            const float head_z = static_cast<float>(judgement_z + lane_speed_ * (note_target_ms_[idx] - visual_ms));
            return head_z < lane_start_z;
        });
    }
}

// active_draw_notes_ に含まれるノートだけを描画する。
// キューの更新は update() 内の update_draw_queues() で済んでいるため、ここではフィルタ不要。
void play_scene::draw_notes(const Camera3D& camera) const {
    bool has_active_notes = false;
    for (const std::vector<size_t>& active : active_draw_notes_by_lane_) {
        if (!active.empty()) {
            has_active_notes = true;
            break;
        }
    }
    if (!has_active_notes) {
        return;
    }

    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    if (!get_lane_view_bounds(camera, lane_start_z, judgement_z, lane_end_z)) {
        return;
    }

    const std::vector<note_state>& note_states = judge_system_.note_states();
    const Color note_color = {233, 238, 244, 255};
    const Color note_outline = {120, 128, 138, 255};
    const double visual_ms = get_visual_ms();

    for (int lane = 0; lane < key_count_; ++lane) {
        for (const size_t idx : active_draw_notes_by_lane_[static_cast<size_t>(lane)]) {
            const note_state& state = note_states[idx];
            const float head_z = static_cast<float>(judgement_z + lane_speed_ * (state.target_ms - visual_ms));
            const float center_x = lane_center_x(state.note_ref.lane, key_count_);

            // ホールドノート: 頭～尾のセグメントをレーン範囲内にクランプして描画
            if (state.note_ref.type == note_type::hold) {
                const double tail_target_ms = timing_engine_.tick_to_ms(state.note_ref.end_tick);
                const float tail_z = static_cast<float>(judgement_z + lane_speed_ * (tail_target_ms - visual_ms));
                // ホールド中は頭を判定ライン位置に固定
                const float visual_head_z = state.holding ? judgement_z : head_z;
                const float segment_start = std::max(std::min(visual_head_z, tail_z), lane_start_z);
                const float segment_end = std::min(std::max(head_z, tail_z), lane_end_z);
                if (segment_end > segment_start) {
                    DrawCube({center_x, 0.30f, (segment_start + segment_end) * 0.5f}, g_settings.lane_width * 0.92f,
                             0.12f, segment_end - segment_start, note_color);
                    DrawCubeWires({center_x, 0.30f, (segment_start + segment_end) * 0.5f},
                                  g_settings.lane_width * 0.94f, 0.14f, segment_end - segment_start + 0.04f,
                                  note_outline);
                }
            }

            // タップノート: 単体の直方体として描画
            if (state.note_ref.type == note_type::tap) {
                DrawCube({center_x, 0.22f, head_z}, g_settings.lane_width * 0.92f, 0.2f, 0.78f, note_color);
                DrawCubeWires({center_x, 0.22f, head_z}, g_settings.lane_width * 0.92f, 0.22f, 0.82f, note_outline);
            }
        }
    }
}

void play_scene::draw_hud() const {
    const result_data result = score_system_.get_result_data();
    const std::string time_text = TextFormat("%.2f", current_ms_ / 1000.0);
    const float health_left = static_cast<float>(kScreenWidth - 350);
    const float health_top = 42.0f;
    const float health_width = 260.0f;
    const float health_height = 24.0f;
    const float inset = 4.0f;
    const float fill_width = (health_width - inset * 2.0f) * (gauge_.get_value() / 100.0f);

    // スコアと経過時間
    DrawText(TextFormat("SCORE %07d", result.score), 48, 34, 30, {230, 232, 238, 255});
    DrawText(time_text.c_str(), kScreenWidth / 2 - MeasureText(time_text.c_str(), 30) / 2, 34, 30,
             {214, 218, 228, 255});
    DrawText(TextFormat("FPS %d", GetFPS()), 48, 70, 22, {160, 166, 178, 255});

    // ヘルスゲージ（70%以上で緑、未満で赤）
    DrawText("HEALTH", kScreenWidth - 238, 10, 24, {72, 78, 90, 255});
    DrawRectangle(static_cast<int>(health_left), static_cast<int>(health_top), static_cast<int>(health_width),
                  static_cast<int>(health_height), {235, 238, 242, 255});
    DrawRectangleLinesEx({health_left, health_top, health_width, health_height}, 3.0f, {96, 104, 116, 255});

    if (fill_width > 0.0f) {
        const Color fill_color = gauge_.get_value() >= 70.0f ? Color{99, 204, 161, 255} : Color{228, 109, 98, 255};
        DrawRectangle(static_cast<int>(health_left + inset), static_cast<int>(health_top + inset),
                      static_cast<int>(fill_width), static_cast<int>(health_height - inset * 2.0f), fill_color);
    }

    // コンボ数（画面中央に大きく表示）
    if (combo_display_ > 0) {
        const Color combo_color = Fade({240, 244, 250, 255}, paused_ ? 0.18f : 0.32f);
        const std::string combo_text = TextFormat("%03d", combo_display_);
        DrawText(combo_text.c_str(), kScreenWidth / 2 - MeasureText(combo_text.c_str(), 86) / 2, 228, 86, combo_color);
        DrawText("COMBO", kScreenWidth / 2 - MeasureText("COMBO", 24) / 2, 306, 24, Fade({209, 214, 224, 255}, 0.55f));
    }
}

void play_scene::draw_pause_overlay() const {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, {3, 6, 10, 150});
    DrawText("PAUSED", kScreenWidth / 2 - 88, 174, 46, {244, 246, 250, 255});
    DrawText(auto_paused_by_focus_ ? "Focus lost: press ESC to resume" : "ESC: Resume", kScreenWidth / 2 - 150, 234, 24,
             {201, 206, 217, 255});
}

void play_scene::draw_judge_feedback() const {
    if (!display_judge_.has_value() || judge_feedback_timer_ <= 0.0f) {
        return;
    }

    const Color color = Fade(judge_color(display_judge_->result), std::min(judge_feedback_timer_ / 1.0f, 1.0f));
    const char* text = judge_text(display_judge_->result);
    DrawText(text, kScreenWidth / 2 - MeasureText(text, 42) / 2, 394, 42, color);
}

void play_scene::draw_intro_overlay() const {
    const float progress = 1.0f - std::clamp(intro_timer_ / kIntroDurationSeconds, 0.0f, 0.7f);
    const unsigned char alpha = static_cast<unsigned char>((1.0f - progress) * 255.0f);
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, {0, 0, 0, alpha});
}

void play_scene::draw_failure_overlay() const {
    const float elapsed = kFailureTransitionDurationSeconds - failure_transition_timer_;
    const float fade_progress = std::clamp(elapsed / kFailureFadeDurationSeconds, 0.0f, 0.7f);
    const unsigned char alpha = static_cast<unsigned char>(fade_progress * 255.0f);
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, {0, 0, 0, alpha});
    const char* text = "FAILED...";
    DrawText(text, kScreenWidth / 2 - MeasureText(text, 44) / 2, kScreenHeight / 2 - 22, 44,
             Fade({244, 246, 250, 255}, std::min(fade_progress * 1.15f, 1.0f)));
}

double play_scene::get_visual_ms() const {
    if (intro_playing_) {
        return -static_cast<double>(intro_timer_) * 1000.0;
    }
    return current_ms_;
}
