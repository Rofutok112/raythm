#include "settings/settings_gameplay_preview.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "data_models.h"
#include "play/play_renderer.h"
#include "play/play_speed_compensation.h"
#include "play/play_view_geometry.h"
#include "scene_common.h"
#include "theme.h"
#include "timing_engine.h"
#include "ui_draw.h"

namespace {

constexpr double kPreviewCycleMs = 4200.0;
constexpr int kPreviewResolution = 480;

int preview_key_count(const game_settings& settings) {
    return std::clamp(settings.selected_key_count, 1, judge_system::kMaxLanes);
}

int lane_for(int requested_lane, int requested_width, int key_count) {
    const int width = std::max(1, std::min(requested_width, key_count));
    return std::clamp(requested_lane, 0, std::max(0, key_count - width));
}

note_data make_note(note_type type, int tick, int lane, int lane_width, int key_count, bool is_ray = false) {
    note_data note;
    note.type = type;
    note.tick = tick;
    note.end_tick = tick;
    note.lane_width = std::max(1, std::min(lane_width, key_count));
    note.lane = lane_for(lane, note.lane_width, key_count);
    note.is_ray = is_ray;
    return note;
}

std::vector<note_data> build_preview_notes(int key_count) {
    std::vector<note_data> notes;
    notes.reserve(12);
    notes.push_back(make_note(note_type::tap, 240, 0, 1, key_count));
    notes.push_back(make_note(note_type::tap, 520, key_count - 1, 1, key_count));
    notes.push_back(make_note(note_type::tap, 840, key_count / 2, 2, key_count));

    note_data hold = make_note(note_type::hold, 1160, 1, key_count >= 4 ? 2 : 1, key_count, true);
    hold.end_tick = 1760;
    notes.push_back(hold);

    notes.push_back(make_note(note_type::stay, 1520, std::max(0, key_count - 2), 1, key_count));
    notes.push_back(make_note(note_type::release, 1900, 0, 1, key_count));
    notes.push_back(make_note(note_type::tap, 2240, key_count - 1, 1, key_count, true));
    notes.push_back(make_note(note_type::tap, 2580, key_count / 2, key_count >= 5 ? 3 : 2, key_count));

    note_data second_hold = make_note(note_type::hold, 2920, 0, 1, key_count);
    second_hold.end_tick = 3400;
    notes.push_back(second_hold);

    notes.push_back(make_note(note_type::stay, 3260, key_count - 1, 1, key_count, true));
    notes.push_back(make_note(note_type::tap, 3620, 1, 1, key_count));
    notes.push_back(make_note(note_type::release, 3920, key_count / 2, 1, key_count, true));
    return notes;
}

std::array<timing_event, 1> preview_timing_events() {
    timing_event event;
    event.type = timing_event_type::bpm;
    event.tick = 0;
    event.bpm = 120.0f;
    event.numerator = 4;
    event.denominator = 4;
    return {event};
}

}  // namespace

settings_gameplay_preview::settings_gameplay_preview(game_settings& settings) : settings_(settings) {
}

settings_gameplay_preview::~settings_gameplay_preview() {
    unload_textures();
}

void settings_gameplay_preview::prepare_frame() {
    if (!ensure_textures()) {
        return;
    }

    const double preview_time_ms = std::fmod(GetTime() * 1000.0, kPreviewCycleMs);
    rebuild_preview_state(preview_time_ms);

    const Camera3D camera = play_view_geometry::make_camera(state_.camera_angle_degrees);
    const std::optional<play_view_geometry::lane_view> lane_view =
        play_view_geometry::resolve_lane_view(camera, state_.key_count, state_.camera_angle_degrees, state_.lane_width);
    if (!lane_view.has_value()) {
        return;
    }

    draw_queue_.update_visible_window(state_.judge_system.note_states(),
                                      static_cast<float>(state_.lane_speed),
                                      lane_view->judgement_z,
                                      lane_view->lane_start_z,
                                      lane_view->lane_end_z,
                                      preview_time_ms);

    BeginTextureMode(lane_texture_);
    ClearBackground(BLANK);
    BeginMode3D(camera);
    play_renderer::draw_world(state_,
                              draw_queue_,
                              camera,
                              lane_view->lane_start_z,
                              lane_view->judgement_z,
                              lane_view->lane_end_z,
                              preview_time_ms,
                              lane_view->lane_width);
    EndMode3D();
    EndTextureMode();

    BeginTextureMode(scene_texture_);
    play_renderer::draw_world_background();
    play_renderer::draw_lane_layer(lane_texture_.texture, state_.lane_fog_hidden_percent);
    EndTextureMode();
}

void settings_gameplay_preview::draw(Rectangle frame) const {
    ui::draw_rect_f(frame, g_theme->panel);
    if (scene_texture_loaded_) {
        const Rectangle source = {
            0.0f,
            0.0f,
            static_cast<float>(scene_texture_.texture.width),
            -static_cast<float>(scene_texture_.texture.height)
        };
        DrawTexturePro(scene_texture_.texture, source, frame, {0.0f, 0.0f}, 0.0f, WHITE);
    }
    ui::draw_rect_lines(frame, 2.0f, with_alpha(g_theme->border_light, 220));
}

bool settings_gameplay_preview::ensure_textures() {
    if (!scene_texture_loaded_) {
        scene_texture_ = LoadRenderTexture(kScreenWidth, kScreenHeight);
        scene_texture_loaded_ = scene_texture_.id != 0;
        if (scene_texture_loaded_) {
            SetTextureFilter(scene_texture_.texture, TEXTURE_FILTER_BILINEAR);
        }
    }
    if (!lane_texture_loaded_) {
        lane_texture_ = LoadRenderTexture(kScreenWidth, kScreenHeight);
        lane_texture_loaded_ = lane_texture_.id != 0;
        if (lane_texture_loaded_) {
            SetTextureFilter(lane_texture_.texture, TEXTURE_FILTER_POINT);
        }
    }
    return scene_texture_loaded_ && lane_texture_loaded_;
}

void settings_gameplay_preview::unload_textures() {
    if (scene_texture_loaded_) {
        UnloadRenderTexture(scene_texture_);
        scene_texture_ = {};
        scene_texture_loaded_ = false;
    }
    if (lane_texture_loaded_) {
        UnloadRenderTexture(lane_texture_);
        lane_texture_ = {};
        lane_texture_loaded_ = false;
    }
}

void settings_gameplay_preview::rebuild_preview_state(double preview_time_ms) {
    const int key_count = preview_key_count(settings_);
    state_ = play_session_state();
    state_.key_count = key_count;
    state_.initialized = true;
    state_.camera_angle_degrees = settings_.camera_angle_degrees;
    state_.lane_width = settings_.lane_width;
    state_.lane_fog_hidden_percent = settings_.lane_fog_hidden_percent;
    state_.note_height = settings_.note_height;
    state_.lane_speed = std::max(0.001f,
                                 play_speed_compensation::compensated_lane_speed(
                                     settings_.note_speed,
                                     state_.camera_angle_degrees));
    state_.chart_time_ms = preview_time_ms;
    state_.paused_chart_time_ms = preview_time_ms;
    state_.gauge = gauge{};
    state_.lane_hold_dim_amounts.fill(0.0f);
    state_.lane_judge_effects.fill(lane_judge_effect{});

    timing_engine engine;
    const std::array<timing_event, 1> timing = preview_timing_events();
    engine.init(std::vector<timing_event>(timing.begin(), timing.end()), kPreviewResolution, 0);
    const std::vector<note_data> notes = build_preview_notes(key_count);
    state_.judge_system.init(notes, engine);
    draw_queue_.init_from_note_states(key_count, state_.judge_system.note_states());
}
