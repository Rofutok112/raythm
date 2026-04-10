#include "play_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <array>

#include "audio_manager.h"
#include "core/app_paths.h"
#include "editor_scene.h"
#include "mv/api/mv_context.h"
#include "mv/mv_storage.h"
#include "mv/mv_runtime.h"
#include "mv/render/mv_renderer.h"
#include "play/play_flow_controller.h"
#include "play/play_renderer.h"
#include "play/play_session_loader.h"
#include "raylib.h"
#include "raymath.h"
#include "result_scene.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr float kJudgementLineScreenRatioFromBottom = 0.10f;
constexpr float kCameraHeight = 42.0f;
constexpr float kCameraFovY = 42.0f;
constexpr float kJudgeLineWorldZ = 12.0f;
constexpr float kMaxGroundDistance = 1000.0f;
constexpr float kMvSpectrumClampMax = 2.0f;

int sample_mv_waveform_index(const std::vector<float>& waveform, double current_ms, double song_length_ms) {
    if (waveform.empty() || song_length_ms <= 0.0) {
        return 0;
    }

    const double progress = std::clamp(current_ms / song_length_ms, 0.0, 1.0);
    const std::size_t last = waveform.size() - 1;
    return static_cast<int>(std::clamp<std::size_t>(
        static_cast<std::size_t>(progress * static_cast<double>(last)), 0, last));
}

std::vector<float> build_mv_spectrum() {
    std::array<float, 128> fft = {};
    if (!audio_manager::instance().get_bgm_fft256(fft)) {
        return {};
    }

    std::vector<float> spectrum;
    spectrum.reserve(fft.size());
    for (float sample : fft) {
        const float shaped = std::sqrt(std::max(0.0f, sample)) * 8.0f;
        spectrum.push_back(std::clamp(shaped, 0.0f, kMvSpectrumClampMax));
    }
    return spectrum;
}

std::vector<float> build_mv_oscilloscope() {
    std::array<float, 256> pcm = {};
    if (!audio_manager::instance().get_bgm_oscilloscope256(pcm)) {
        return {};
    }

    return {pcm.begin(), pcm.end()};
}

float compute_oscilloscope_rms(const std::vector<float>& oscilloscope) {
    if (oscilloscope.empty()) {
        return 0.0f;
    }

    double sum = 0.0;
    for (float sample : oscilloscope) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(oscilloscope.size())));
}

float compute_oscilloscope_peak(const std::vector<float>& oscilloscope) {
    float peak = 0.0f;
    for (float sample : oscilloscope) {
        peak = std::max(peak, std::fabs(sample));
    }
    return peak;
}

float compute_spectrum_band_average(const std::vector<float>& spectrum, std::size_t start, std::size_t end) {
    if (start >= end || start >= spectrum.size()) {
        return 0.0f;
    }

    const std::size_t clamped_end = std::min(end, spectrum.size());
    double sum = 0.0;
    for (std::size_t i = start; i < clamped_end; ++i) {
        sum += spectrum[i];
    }
    return static_cast<float>(sum / static_cast<double>(clamped_end - start));
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

}  // namespace

play_scene::play_scene(scene_manager& manager, int key_count) : scene(manager) {
    request_.key_count = key_count;
}

play_scene::play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count)
    : scene(manager) {
    request_.key_count = key_count;
    request_.song_data = std::move(song);
    request_.selected_chart_path = std::move(chart_path);
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

play_scene::~play_scene() = default;

void play_scene::on_enter() {
    state_ = play_session_loader::load(request_, draw_queue_);

    // Try to load MV script for this song
    if (state_.song_data.has_value()) {
        if (const auto package = mv::find_first_package_for_song(state_.song_data->meta.song_id); package.has_value()) {
            const auto script_file = mv::script_path(*package);
            TraceLog(LOG_INFO, "MV: looking for script at %s", script_file.string().c_str());
            mv_runtime_ = std::make_unique<mv::mv_runtime>();
            if (!mv_runtime_->load_file(script_file.string())) {
                TraceLog(LOG_WARNING, "MV: compile failed");
                for (const auto& e : mv_runtime_->last_errors()) {
                    TraceLog(LOG_WARNING, "MV:   L%d: %s (%s)", e.line, e.message.c_str(), e.phase.c_str());
                }
                mv_runtime_.reset();
            } else {
                TraceLog(LOG_INFO, "MV: script loaded OK");
            }
        } else {
            TraceLog(LOG_INFO, "MV: script file not found");
        }
    } else {
        TraceLog(LOG_INFO, "MV: no song_data, skipping script load");
    }
}

void play_scene::on_exit() {
    audio_manager::instance().stop_bgm();
    audio_manager::instance().stop_all_se();
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

    const audio_clock_snapshot bgm_clock = audio_manager::instance().get_bgm_clock();
    context.bgm_loaded = bgm_clock.loaded;
    if (bgm_clock.loaded) {
        context.bgm_audio_time_ms = bgm_clock.audio_time_seconds * 1000.0;
    }

    if (state_.paused) {
        const auto pause_buttons = play_renderer::pause_button_rects();
        context.pause_resume_clicked = ui::is_clicked(pause_buttons[0], ui::draw_layer::modal);
        context.pause_restart_clicked = ui::is_clicked(pause_buttons[1], ui::draw_layer::modal);
        context.pause_song_select_clicked = ui::is_clicked(pause_buttons[2], ui::draw_layer::modal);
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

    if (result.request_pause_bgm) {
        audio_manager::instance().pause_bgm();
    }
    if (result.request_play_bgm && audio_manager::instance().is_bgm_loaded()) {
        audio_manager::instance().play_bgm(false);
    }
    for (int i = 0; i < result.hitsound_count; ++i) {
        audio_manager::instance().play_se(state_.hitsound_path);
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

void play_scene::draw() {
    if (!state_.initialized) {
        virtual_screen::begin();
        play_renderer::draw_status(state_);
        virtual_screen::end();

        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    play_renderer::draw_world_background();

    // MV script layer (2D, behind notes)
    if (mv_runtime_ && mv_runtime_->is_loaded()) {
        mv::context_input mv_input;
        mv_input.current_ms = get_visual_ms();
        mv_input.song_length_ms = state_.song_end_ms;

        int current_tick = state_.timing_engine.ms_to_tick(state_.current_ms);
        mv_input.bpm = state_.timing_engine.get_bpm_at(current_tick);
        mv_input.meter_numerator = state_.timing_engine.get_meter_numerator_at(current_tick);
        mv_input.meter_denominator = state_.timing_engine.get_meter_denominator_at(current_tick);

        double beat_duration_ms = 60000.0 / mv_input.bpm;
        if (beat_duration_ms > 0) {
            double ms_in_beat = std::fmod(state_.current_ms, beat_duration_ms);
            if (ms_in_beat < 0) ms_in_beat += beat_duration_ms;
            mv_input.beat_phase = static_cast<float>(ms_in_beat / beat_duration_ms);
            mv_input.beat_number = static_cast<int>(state_.current_ms / beat_duration_ms);
        }

        mv_input.combo = state_.combo_display;
        mv_input.key_count = state_.key_count;
        mv_input.spectrum = build_mv_spectrum();
        std::vector<float> oscilloscope = build_mv_oscilloscope();
        mv_input.oscilloscope = &oscilloscope;
        mv_input.rms = compute_oscilloscope_rms(oscilloscope);
        mv_input.peak = compute_oscilloscope_peak(oscilloscope);
        const std::size_t spectrum_size = mv_input.spectrum.size();
        const std::size_t first_split = spectrum_size / 3;
        const std::size_t second_split = (spectrum_size * 2) / 3;
        mv_input.low = compute_spectrum_band_average(mv_input.spectrum, 0, first_split);
        mv_input.mid = compute_spectrum_band_average(mv_input.spectrum, first_split, second_split);
        mv_input.high = compute_spectrum_band_average(mv_input.spectrum, second_split, spectrum_size);
        mv_input.waveform = &state_.mv_waveform;
        mv_input.waveform_index =
            sample_mv_waveform_index(state_.mv_waveform, mv_input.current_ms, mv_input.song_length_ms);
        if (!state_.mv_waveform.empty()) {
            mv_input.level = state_.mv_waveform[static_cast<std::size_t>(mv_input.waveform_index)];
        }
        if (state_.song_data.has_value()) {
            mv_input.song_id = state_.song_data->meta.song_id;
            mv_input.song_title = state_.song_data->meta.title;
            mv_input.song_artist = state_.song_data->meta.artist;
            mv_input.song_base_bpm = state_.song_data->meta.base_bpm;
        }
        if (state_.chart_data.has_value()) {
            mv_input.chart_id = state_.chart_data->meta.chart_id;
            mv_input.chart_song_id = state_.chart_data->meta.song_id;
            mv_input.chart_difficulty = state_.chart_data->meta.difficulty;
            mv_input.chart_level = state_.chart_data->meta.level;
            mv_input.chart_author = state_.chart_data->meta.chart_author;
            mv_input.chart_resolution = state_.chart_data->meta.resolution;
            mv_input.chart_offset = state_.chart_data->meta.offset;
            mv_input.total_notes = static_cast<int>(state_.chart_data->notes.size());
        }
        mv_input.screen_w = static_cast<float>(kScreenWidth);
        mv_input.screen_h = static_cast<float>(kScreenHeight);

        auto scene_opt = mv_runtime_->tick(mv_input);
        if (scene_opt.has_value()) {
            static int mv_log_count = 0;
            if (mv_log_count < 3) {
                TraceLog(LOG_INFO, "MV: tick OK, %d nodes", static_cast<int>(scene_opt->nodes.size()));
                mv_log_count++;
            }
            virtual_screen::begin();
            mv::render_scene(*scene_opt);
            virtual_screen::end();
            virtual_screen::draw_to_screen(true);
        } else {
            static int mv_fail_count = 0;
            if (mv_fail_count < 3) {
                TraceLog(LOG_WARNING, "MV: tick returned nullopt");
                for (const auto& e : mv_runtime_->last_errors()) {
                    TraceLog(LOG_WARNING, "MV:   L%d: %s (%s)", e.line, e.message.c_str(), e.phase.c_str());
                }
                mv_fail_count++;
            }
        }
    }

    const Camera3D camera = make_play_camera();
    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    const bool has_bounds = get_lane_view_bounds(camera, lane_start_z, judgement_z, lane_end_z);

    BeginMode3D(camera);
    if (has_bounds) {
        play_renderer::draw_world(state_, draw_queue_, lane_start_z, judgement_z, lane_end_z, get_visual_ms());
    }
    EndMode3D();

    virtual_screen::begin();
    rebuild_hit_regions();
    ui::begin_draw_queue();
    ClearBackground(BLANK);
    play_renderer::draw_overlay(state_);
    ui::flush_draw_queue();
    virtual_screen::end();

    virtual_screen::draw_to_screen(true);
}

double play_scene::get_visual_ms() const {
    if (state_.intro_playing) {
        return state_.start_ms - static_cast<double>(state_.intro_timer) * 1000.0;
    }
    return state_.current_ms;
}

void play_scene::apply_navigation(play_navigation_request navigation) {
    switch (navigation.target) {
        case play_navigation_target::none:
            return;
        case play_navigation_target::song_select:
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
            return;
        case play_navigation_target::result:
            if (state_.song_data.has_value() && state_.chart_data.has_value()) {
                manager_.change_scene(std::make_unique<result_scene>(
                    manager_, state_.final_result, state_.ranking_enabled,
                    *state_.song_data, state_.selected_chart_path.value_or(""), state_.chart_data->meta, state_.key_count));
            } else {
                manager_.change_scene(std::make_unique<song_select_scene>(manager_));
            }
            return;
        case play_navigation_target::editor:
            if (state_.editor_resume_state.has_value() && state_.song_data.has_value()) {
                manager_.change_scene(std::make_unique<editor_scene>(
                    manager_, *state_.song_data, std::move(*state_.editor_resume_state)));
            } else {
                manager_.change_scene(std::make_unique<song_select_scene>(manager_));
            }
            return;
        case play_navigation_target::restart:
            if (state_.editor_resume_state.has_value() && state_.song_data.has_value() && state_.chart_data.has_value()) {
                manager_.change_scene(std::make_unique<play_scene>(
                    manager_, *state_.song_data, *state_.chart_data, state_.start_tick, std::move(*state_.editor_resume_state)));
            } else if (state_.song_data.has_value() && state_.selected_chart_path.has_value()) {
                manager_.change_scene(std::make_unique<play_scene>(
                    manager_, *state_.song_data, *state_.selected_chart_path, state_.key_count));
            } else {
                manager_.change_scene(std::make_unique<play_scene>(manager_, state_.key_count));
            }
            return;
    }
}
