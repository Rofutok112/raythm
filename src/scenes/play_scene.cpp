#include "play_scene.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>

#include "audio_manager.h"
#include "core/app_paths.h"
#include "editor_scene.h"
#include "path_utils.h"
#include "play/play_flow_controller.h"
#include "play/play_renderer.h"
#include "play/play_session_loader.h"
#include "raylib.h"
#include "raymath.h"
#include "result_scene.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "multiplayer/multiplayer_client.h"
#include "multiplayer_result_scene.h"
#include "network/json_helpers.h"
#include "song_select/song_select_navigation.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr float kJudgementLineScreenRatioFromBottom = 0.10f;
constexpr float kCameraHeight = 42.0f;
constexpr float kCameraFovY = 42.0f;
constexpr float kJudgeLineWorldZ = 12.0f;
constexpr float kMaxGroundDistance = 1000.0f;
constexpr float kSoloStartGateSeconds = 0.75f;
constexpr float kMatchLoadedPollSeconds = 1.0f;
constexpr float kFallbackMatchCountdownSeconds = 3.0f;

std::optional<float> seconds_until_iso_utc(const std::string& iso_utc) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int consumed = 0;
    if (std::sscanf(iso_utc.c_str(),
                    "%d-%d-%dT%d:%d:%d%n",
                    &year,
                    &month,
                    &day,
                    &hour,
                    &minute,
                    &second,
                    &consumed) != 6) {
        return std::nullopt;
    }

    int milliseconds = 0;
    if (consumed > 0 && static_cast<size_t>(consumed) < iso_utc.size() && iso_utc[consumed] == '.') {
        int fraction = 0;
        int fraction_digits = 0;
        for (size_t i = static_cast<size_t>(consumed + 1); i < iso_utc.size() && fraction_digits < 3; ++i) {
            if (iso_utc[i] < '0' || iso_utc[i] > '9') {
                break;
            }
            fraction = fraction * 10 + (iso_utc[i] - '0');
            ++fraction_digits;
        }
        while (fraction_digits > 0 && fraction_digits < 3) {
            fraction *= 10;
            ++fraction_digits;
        }
        milliseconds = fraction;
    }

    std::tm utc_tm = {};
    utc_tm.tm_year = year - 1900;
    utc_tm.tm_mon = month - 1;
    utc_tm.tm_mday = day;
    utc_tm.tm_hour = hour;
    utc_tm.tm_min = minute;
    utc_tm.tm_sec = second;
#if defined(_WIN32)
    const std::time_t utc_time = _mkgmtime(&utc_tm);
#else
    const std::time_t utc_time = timegm(&utc_tm);
#endif
    if (utc_time == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    const auto start_time =
        std::chrono::system_clock::from_time_t(utc_time) + std::chrono::milliseconds(milliseconds);
    const auto remaining = std::chrono::duration<float>(start_time - std::chrono::system_clock::now()).count();
    return std::max(0.0f, remaining);
}

float countdown_seconds_from_start_at(const std::string& start_at) {
    return seconds_until_iso_utc(start_at).value_or(kFallbackMatchCountdownSeconds);
}

Vector3 build_camera_forward(float camera_angle_degrees) {
    const float angle_rad = std::clamp(camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    return Vector3{0.0f, -std::sin(angle_rad), std::cos(angle_rad)};
}

Vector3 choose_camera_up(Vector3 forward) {
    return std::fabs(Vector3DotProduct(forward, Vector3{0.0f, 1.0f, 0.0f})) > 0.98f
               ? Vector3{0.0f, 0.0f, 1.0f}
               : Vector3{0.0f, 1.0f, 0.0f};
}

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

void present_virtual_screen() {
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void present_virtual_screen_overlay() {
    virtual_screen::draw_to_screen(true);
}

}  // namespace

play_scene::play_scene(scene_manager& manager, int key_count) : scene(manager) {
    request_.key_count = key_count;
}

play_scene::play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count,
                       float chart_level)
    : scene(manager) {
    request_.key_count = key_count;
    request_.song_data = std::move(song);
    request_.selected_chart_path = std::move(chart_path);
    if (chart_level > 0.0f) {
        request_.selected_chart_level = chart_level;
    }
}

play_scene::play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count,
                       float chart_level, std::string multiplayer_room_id, std::string multiplayer_match_id)
    : play_scene(manager, std::move(song), std::move(chart_path), key_count, chart_level) {
    request_.multiplayer_room_id = std::move(multiplayer_room_id);
    request_.multiplayer_match_id = std::move(multiplayer_match_id);
}

play_scene::play_scene(scene_manager& manager, song_data song, chart_data chart, int start_tick,
                       editor_resume_state editor_resume)
    : scene(manager) {
    request_.key_count = chart.meta.key_count;
    request_.song_data = std::move(song);
    request_.chart_data = std::move(chart);
    request_.editor_resume_state = std::move(editor_resume);
    request_.start_tick = std::max(0, start_tick);
}

play_scene::~play_scene() {
    unload_jacket_texture();
}

void play_scene::on_enter() {
    state_.status_text = "Loading...";
    state_ = play_session_loader::load(request_, draw_queue_);
    load_jacket_texture();
    mv_controller_.load_for_song(state_.song_data);
    if (!state_.multiplayer_room_id.empty()) {
        multiplayer_realtime_ = std::make_unique<multiplayer::client::realtime_client>();
        if (!multiplayer_realtime_->connect(auth::load_session_summary(), state_.multiplayer_room_id)) {
            multiplayer_realtime_.reset();
        }
    }
    start_gate_active_ = state_.initialized;
    multiplayer_loaded_sent_ = false;
    multiplayer_countdown_started_ = false;
    start_gate_timer_ = state_.multiplayer_room_id.empty() ? kSoloStartGateSeconds : 0.0f;
    match_loaded_poll_t_ = 0.0f;
    if (start_gate_active_) {
        state_.status_text = state_.multiplayer_room_id.empty()
            ? "Ready"
            : "Loaded. Waiting for players...";
    }
}

void play_scene::on_exit() {
    if (multiplayer_realtime_ != nullptr) {
        multiplayer_realtime_->close();
        multiplayer_realtime_.reset();
    }
    if (multiplayer_loaded_future_.has_value()) {
        (void)multiplayer_loaded_future_->wait_for(std::chrono::milliseconds(0));
        multiplayer_loaded_future_.reset();
    }
    audio_manager::instance().stop_bgm();
    audio_manager::instance().stop_all_se();
    mv_controller_.reset();
    unload_jacket_texture();
}

void play_scene::update(float dt) {
    rebuild_hit_regions();

    play_update_context context;
    context.dt = dt;
    context.escape_pressed = IsKeyPressed(KEY_ESCAPE);
    context.enter_pressed = IsKeyPressed(KEY_ENTER);
    context.left_click_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    context.backspace_pressed = IsKeyPressed(KEY_BACKSPACE);
    context.window_focused = IsWindowFocused();

    if (start_gate_active_) {
        update_start_gate(dt);
        return;
    }

    const audio_clock_snapshot bgm_clock = audio_manager::instance().get_bgm_clock();
    context.bgm_loaded = bgm_clock.loaded;
    context.bgm_playing = bgm_clock.playing;
    if (bgm_clock.loaded && bgm_clock.playing) {
        context.audio_clock_time_ms = bgm_clock.audio_time_seconds * 1000.0;
    }

    if (state_.paused) {
        const auto pause_buttons = play_renderer::pause_button_rects();
        context.pause_resume_clicked = ui::is_clicked(pause_buttons[0], ui::draw_layer::modal);
        context.pause_restart_clicked = ui::is_clicked(pause_buttons[1], ui::draw_layer::modal);
        context.pause_song_select_clicked = ui::is_clicked(pause_buttons[2], ui::draw_layer::modal);
    }
    if (!state_.hitsound_path.empty() || state_.hitsounds.has_any()) {
        context.play_hitsound_immediately = [hitsounds = state_.hitsounds](const judge_event& event) {
            const std::string& path = hitsounds.path_for(event);
            if (!path.empty()) {
                audio_manager::instance().play_se(path);
            }
        };
    }

    if (state_.initialized) {
        const Camera3D camera = make_play_camera();
        float lane_start_z = 0.0f;
        float judgement_z = 0.0f;
        float lane_end_z = 0.0f;
        if (get_lane_view_bounds(camera, lane_start_z, judgement_z, lane_end_z)) {
            context.draw_window = play_draw_window{
                lane_start_z,
                judgement_z,
                lane_end_z,
                get_visual_ms(),
            };
        }
    }

    const play_update_result result = play_flow_controller::update(state_, draw_queue_, context);
    sync_multiplayer_score(dt);

    if (result.request_pause_bgm) {
        audio_manager::instance().pause_bgm();
    }
    if (result.request_fade_out_bgm) {
        audio_manager::instance().fade_out_bgm(result.fade_out_bgm_duration_ms);
    }
    if (result.request_play_bgm && audio_manager::instance().is_bgm_loaded()) {
        audio_manager::instance().play_bgm(false);
    }
    if (result.navigation.has_value()) {
        apply_navigation(result.navigation);
    }
}

void play_scene::rebuild_hit_regions() const {
    ui::begin_hit_regions();
    if (state_.paused) {
        ui::register_hit_region({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                                ui::draw_layer::overlay);
        ui::register_hit_region(play_renderer::pause_panel_rect(), ui::draw_layer::modal);
    }
}

Camera3D play_scene::make_play_camera() const {
    const float angle_rad = std::clamp(state_.camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    constexpr float half_fov_rad = kCameraFovY * DEG2RAD * 0.5f;

    constexpr float judge_ndc_y = (kJudgementLineScreenRatioFromBottom - 0.5f) * 2.0f;
    const std::optional<float> judge_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, judge_ndc_y);
    const float camera_z = judge_offset.has_value() ? (kJudgeLineWorldZ - *judge_offset) : 0.0f;

    const Vector3 forward = build_camera_forward(state_.camera_angle_degrees);
    const Vector3 up = choose_camera_up(forward);

    Camera3D camera = {};
    camera.position = {0.0f, kCameraHeight, camera_z};
    camera.target = Vector3Add(camera.position, forward);
    camera.up = up;
    camera.fovy = kCameraFovY;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

bool play_scene::get_lane_view_bounds(const Camera3D& camera, float& lane_start_z, float& judgement_z,
                                      float& lane_end_z) const {
    const float angle_rad = std::clamp(state_.camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    constexpr float half_fov_rad = kCameraFovY * DEG2RAD * 0.5f;

    const std::optional<float> near_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, -1.0f);
    if (!near_offset.has_value()) {
        return false;
    }

    judgement_z = kJudgeLineWorldZ;
    lane_start_z = camera.position.z + *near_offset;

    const std::optional<float> far_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, 1.0f);
    if (far_offset.has_value()) {
        lane_end_z = std::min(camera.position.z + *far_offset, camera.position.z + kMaxGroundDistance);
    } else {
        lane_end_z = camera.position.z + kMaxGroundDistance;
    }

    if (lane_end_z <= lane_start_z) {
        return false;
    }

    lane_start_z = std::min(lane_start_z, judgement_z - 0.5f);
    lane_end_z = std::max(lane_end_z, judgement_z + 8.0f);
    return true;
}

void play_scene::load_jacket_texture() {
    unload_jacket_texture();
    if (!state_.song_data.has_value() || state_.song_data->meta.jacket_file.empty()) {
        return;
    }

    const std::filesystem::path jacket_path =
        path_utils::join_utf8(state_.song_data->directory, state_.song_data->meta.jacket_file);
    if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
        return;
    }

    const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
    jacket_texture_ = LoadTexture(jacket_path_utf8.c_str());
    jacket_texture_loaded_ = jacket_texture_.id != 0;
    if (jacket_texture_loaded_) {
        SetTextureFilter(jacket_texture_, TEXTURE_FILTER_BILINEAR);
    }
}

void play_scene::unload_jacket_texture() {
    if (!jacket_texture_loaded_) {
        return;
    }

    UnloadTexture(jacket_texture_);
    jacket_texture_ = {};
    jacket_texture_loaded_ = false;
}

void play_scene::draw() {
    if (!state_.initialized || start_gate_active_) {
        virtual_screen::begin_ui();
        play_renderer::draw_status(state_);
        virtual_screen::end();

        present_virtual_screen();
        return;
    }

    virtual_screen::begin();
    play_renderer::draw_world_background();
    const double visual_time_ms = get_visual_ms();
    mv_controller_.draw(state_, visual_time_ms);

    const Camera3D camera = make_play_camera();
    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    const bool has_bounds = get_lane_view_bounds(camera, lane_start_z, judgement_z, lane_end_z);

    BeginMode3D(camera);
    if (has_bounds) {
        play_renderer::draw_world(state_, draw_queue_, camera, lane_start_z, judgement_z, lane_end_z, visual_time_ms);
    }
    EndMode3D();

    virtual_screen::end();

    present_virtual_screen();

    virtual_screen::begin_ui();
    ClearBackground(BLANK);
    rebuild_hit_regions();
    ui::begin_draw_queue();
    play_renderer::draw_overlay(state_, jacket_texture_loaded_ ? &jacket_texture_ : nullptr);
    ui::flush_draw_queue();
    virtual_screen::end();

    present_virtual_screen_overlay();
}

void play_scene::update_start_gate(float dt) {
    if (!state_.initialized) {
        return;
    }

    if (state_.multiplayer_room_id.empty()) {
        start_gate_timer_ -= dt;
        state_.status_text = "Starting...";
        if (start_gate_timer_ <= 0.0f) {
            start_gate_active_ = false;
            state_.status_text.clear();
        }
        return;
    }

    if (multiplayer_realtime_ != nullptr) {
        for (const multiplayer::room_operation_result& event : multiplayer_realtime_->poll_room_events()) {
            if (event.match_id == state_.multiplayer_match_id && !event.match_start_at.empty()) {
                multiplayer_countdown_started_ = true;
                start_gate_timer_ = countdown_seconds_from_start_at(event.match_start_at);
            }
        }
    }

    if (!multiplayer_countdown_started_) {
        if (!multiplayer_loaded_sent_ && multiplayer_realtime_ != nullptr && multiplayer_realtime_->connected()) {
            const std::string body = "{\"matchId\":\"" +
                network::json::escape_string(state_.multiplayer_match_id) + "\"}";
            multiplayer_loaded_sent_ = multiplayer_realtime_->send_command("match.loaded", body);
        }

        if (multiplayer_loaded_future_.has_value() &&
            multiplayer_loaded_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            const multiplayer::room_operation_result result = multiplayer_loaded_future_->get();
            multiplayer_loaded_future_.reset();
            multiplayer_loaded_sent_ = multiplayer_loaded_sent_ || result.success;
            if (result.match_id == state_.multiplayer_match_id && !result.match_start_at.empty()) {
                multiplayer_countdown_started_ = true;
                start_gate_timer_ = countdown_seconds_from_start_at(result.match_start_at);
            }
        }

        match_loaded_poll_t_ += dt;
        if (!multiplayer_loaded_future_.has_value() &&
            (multiplayer_realtime_ == nullptr || !multiplayer_realtime_->connected()) &&
            match_loaded_poll_t_ >= kMatchLoadedPollSeconds) {
            match_loaded_poll_t_ = 0.0f;
            const auth::session_summary session = auth::load_session_summary();
            const std::string room_id = state_.multiplayer_room_id;
            const std::string match_id = state_.multiplayer_match_id;
            multiplayer_loaded_future_ = std::async(std::launch::async, [session, room_id, match_id]() {
                return multiplayer::client::mark_match_loaded(session, room_id, match_id);
            });
        }

        state_.status_text = multiplayer_loaded_sent_
            ? "Waiting for other players..."
            : "Loaded. Waiting for players...";
        return;
    }

    start_gate_timer_ -= dt;
    const int countdown = std::max(1, static_cast<int>(std::ceil(start_gate_timer_)));
    state_.status_text = "Starting in " + std::to_string(countdown) + "...";
    if (start_gate_timer_ <= 0.0f) {
        start_gate_active_ = false;
        state_.status_text.clear();
    }
}

void play_scene::sync_multiplayer_score(float dt) {
    if (state_.multiplayer_room_id.empty()) {
        return;
    }

    if (multiplayer_realtime_ != nullptr) {
        for (const multiplayer::room_operation_result& event : multiplayer_realtime_->poll_room_events()) {
            if (!event.live_scores.empty()) {
                state_.multiplayer_scores.clear();
                for (const multiplayer::live_score& score : event.live_scores) {
                    state_.multiplayer_scores.push_back({
                        score.user_id,
                        score.display_name,
                        score.score,
                        score.combo,
                        score.accuracy,
                        score.failed,
                    });
                }
            }
            if (event.room.has_value()) {
                state_.multiplayer_scores.clear();
                for (const multiplayer::live_score& score : event.room->live_scores) {
                    state_.multiplayer_scores.push_back({
                        score.user_id,
                        score.display_name,
                        score.score,
                        score.combo,
                        score.accuracy,
                        score.failed,
                    });
                }
            }
        }
    }

    if (multiplayer_score_future_.has_value() &&
        multiplayer_score_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        (void)multiplayer_score_future_->get();
        multiplayer_score_future_.reset();
    }
    if (multiplayer_room_future_.has_value() &&
        multiplayer_room_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const multiplayer::room_operation_result result = multiplayer_room_future_->get();
        multiplayer_room_future_.reset();
        if (!result.live_scores.empty()) {
            state_.multiplayer_scores.clear();
            for (const multiplayer::live_score& score : result.live_scores) {
                state_.multiplayer_scores.push_back({
                    score.user_id,
                    score.display_name,
                    score.score,
                    score.combo,
                    score.accuracy,
                    score.failed,
                });
            }
        }
        if (result.room.has_value()) {
            state_.multiplayer_scores.clear();
            for (const multiplayer::live_score& score : result.room->live_scores) {
                state_.multiplayer_scores.push_back({
                    score.user_id,
                    score.display_name,
                    score.score,
                    score.combo,
                    score.accuracy,
                    score.failed,
                });
            }
        }
    }

    multiplayer_score_sync_t_ += dt;
    if (multiplayer_score_sync_t_ < 0.5f) {
        return;
    }
    multiplayer_score_sync_t_ = 0.0f;

    const auth::session_summary session = auth::load_session_summary();
    const std::string room_id = state_.multiplayer_room_id;
    const std::string match_id = state_.multiplayer_match_id;
    const int score = state_.score_system.get_score();
    const int combo = state_.score_system.get_combo();
    const float accuracy = state_.score_system.get_live_accuracy();
    const bool failed = state_.multiplayer_failed;
    if (multiplayer_realtime_ != nullptr && multiplayer_realtime_->connected()) {
        const std::string body = "{\"score\":" + std::to_string(score) +
            ",\"combo\":" + std::to_string(combo) +
            ",\"accuracy\":" + std::to_string(accuracy) +
            ",\"failed\":" + std::string(state_.multiplayer_failed ? "true" : "false") +
            (match_id.empty() ? "" : ",\"matchId\":\"" + network::json::escape_string(match_id) + "\"") +
            "}";
        if (multiplayer_realtime_->send_command("score.update", body)) {
            return;
        }
    }
    if (!multiplayer_score_future_.has_value()) {
        multiplayer_score_future_ = std::async(std::launch::async, [session, room_id, match_id, score, combo, accuracy, failed]() {
            return multiplayer::client::update_score(session, room_id, match_id, score, combo, accuracy, failed);
        });
    }
    if (!multiplayer_room_future_.has_value()) {
        multiplayer_room_future_ = std::async(std::launch::async, [session, room_id]() {
            return multiplayer::client::fetch_room(session, room_id);
        });
    }
}

double play_scene::get_visual_ms() const {
    double source_ms = state_.chart_time_ms;
    if (state_.intro_playing) {
        source_ms = state_.start_ms - static_cast<double>(state_.intro_timer) * 1000.0;
    }
    return state_.scroll_map.visual_ms_at(source_ms);
}

void play_scene::apply_navigation(play_navigation_request navigation) {
    switch (navigation.target) {
        case play_navigation_target::none:
            return;
        case play_navigation_target::song_select:
            if (!state_.multiplayer_room_id.empty()) {
                manager_.change_scene(song_select::make_multiplayer_title_scene(
                    manager_,
                    state_.multiplayer_room_id,
                    state_.song_data.has_value() ? state_.song_data->meta.song_id : "",
                    state_.chart_data.has_value() ? state_.chart_data->meta.chart_id : ""));
                return;
            }
            manager_.change_scene(song_select::make_seamless_song_select_scene(
                manager_,
                state_.song_data.has_value() ? state_.song_data->meta.song_id : "",
                state_.chart_data.has_value() ? state_.chart_data->meta.chart_id : ""));
            return;
        case play_navigation_target::result:
            if (!state_.multiplayer_room_id.empty()) {
                const auth::session_summary session = auth::load_session_summary();
                result_data result_payload = state_.final_result;
                result_payload.failed = result_payload.failed || state_.multiplayer_failed;
                if (!state_.multiplayer_match_id.empty()) {
                    (void)multiplayer::client::update_score(
                        session,
                        state_.multiplayer_room_id,
                        state_.multiplayer_match_id,
                        result_payload.score,
                        result_payload.max_combo,
                        result_payload.accuracy,
                        result_payload.failed);
                }
                const std::optional<auth::session> saved_session = auth::load_saved_session();
                if (state_.song_data.has_value() && state_.chart_data.has_value()) {
                    manager_.change_scene(std::make_unique<multiplayer_result_scene>(
                        manager_,
                        result_payload,
                        *state_.song_data,
                        state_.chart_data->meta,
                        state_.key_count,
                        state_.multiplayer_room_id,
                        state_.multiplayer_match_id,
                        saved_session.has_value() ? saved_session->user.id : "",
                        state_.multiplayer_scores));
                } else {
                    manager_.change_scene(song_select::make_multiplayer_title_scene(
                        manager_,
                        state_.multiplayer_room_id,
                        state_.song_data.has_value() ? state_.song_data->meta.song_id : "",
                        state_.chart_data.has_value() ? state_.chart_data->meta.chart_id : ""));
                }
                return;
            }
            if (state_.song_data.has_value() && state_.chart_data.has_value()) {
                manager_.change_scene(std::make_unique<result_scene>(
                    manager_, state_.final_result, state_.ranking_enabled,
                    *state_.song_data, state_.selected_chart_path.value_or(""), state_.chart_data->meta, state_.key_count));
            } else {
                manager_.change_scene(song_select::make_seamless_song_select_scene(manager_));
            }
            return;
        case play_navigation_target::editor:
            if (state_.editor_resume_state.has_value() && state_.song_data.has_value()) {
                manager_.change_scene(std::make_unique<editor_scene>(
                    manager_, *state_.song_data, std::move(*state_.editor_resume_state)));
            } else {
                manager_.change_scene(song_select::make_seamless_song_select_scene(manager_));
            }
            return;
        case play_navigation_target::restart:
            if (!state_.multiplayer_room_id.empty()) {
                manager_.change_scene(song_select::make_multiplayer_title_scene(
                    manager_,
                    state_.multiplayer_room_id,
                    state_.song_data.has_value() ? state_.song_data->meta.song_id : "",
                    state_.chart_data.has_value() ? state_.chart_data->meta.chart_id : ""));
                return;
            }
            if (state_.editor_resume_state.has_value() && state_.song_data.has_value() && state_.chart_data.has_value()) {
                manager_.change_scene(std::make_unique<play_scene>(
                    manager_, *state_.song_data, *state_.chart_data, state_.start_tick, std::move(*state_.editor_resume_state)));
            } else if (state_.song_data.has_value() && state_.selected_chart_path.has_value()) {
                manager_.change_scene(std::make_unique<play_scene>(
                    manager_, *state_.song_data, *state_.selected_chart_path, state_.key_count,
                    state_.chart_data.has_value() ? state_.chart_data->meta.level : 0.0f));
            } else {
                manager_.change_scene(std::make_unique<play_scene>(manager_, state_.key_count));
            }
            return;
    }
}
