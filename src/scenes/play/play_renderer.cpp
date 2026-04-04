#include "play_renderer.h"

#include <algorithm>

#include "game_settings.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"

namespace {

constexpr float kLaneGap = 0.2f;
constexpr float kJudgeLineGlowHeight = 0.04f;
constexpr float kResultFadeMaxAlpha = 0.65f;
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kPausePanelRect = ui::center(kScreenRect, 420.0f, 320.0f);
constexpr Rectangle kPauseTitleRect = {kPausePanelRect.x, kPausePanelRect.y, kPausePanelRect.width, 64.0f};
constexpr Rectangle kPauseButtonArea = {
    kPausePanelRect.x + 40.0f,
    kPausePanelRect.y + 88.0f,
    340.0f,
    3.0f * 42.0f + 2.0f * 16.0f
};
constexpr Rectangle kPauseHintRect = {
    kPausePanelRect.x + 24.0f,
    kPausePanelRect.y + kPausePanelRect.height - 50.0f,
    kPausePanelRect.width - 48.0f,
    30.0f
};
constexpr Rectangle kScoreRect = ui::place(kScreenRect, 400.0f, 60.0f,
                                           ui::anchor::top_left, ui::anchor::top_left,
                                           {48.0f, 34.0f});
constexpr Rectangle kTimeRect = ui::place(kScreenRect, 200.0f, 30.0f,
                                          ui::anchor::top_center, ui::anchor::top_center,
                                          {0.0f, 34.0f});
constexpr Rectangle kFpsRect = ui::place(kScreenRect, 120.0f, 20.0f,
                                         ui::anchor::bottom_right, ui::anchor::bottom_right,
                                         {-10.0f, 0.0f});
constexpr Rectangle kHealthLabelRect = ui::place(kScreenRect, 100.0f, 24.0f,
                                                 ui::anchor::top_right, ui::anchor::top_right,
                                                 {-48.0f, 34.0f});
constexpr Rectangle kHealthBarRect = ui::place(kScreenRect, 260.0f, 24.0f,
                                               ui::anchor::top_right, ui::anchor::top_right,
                                               {-48.0f, 58.0f});
constexpr Rectangle kComboNumberRect = ui::place(kScreenRect, 300.0f, 86.0f,
                                                 ui::anchor::center, ui::anchor::center,
                                                 {0.0f, -80.0f});
constexpr Rectangle kComboLabelRect = ui::place(kScreenRect, 200.0f, 24.0f,
                                                ui::anchor::center, ui::anchor::center,
                                                {0.0f, 0.0f});
constexpr Rectangle kJudgeFeedbackRect = ui::place(kScreenRect, 320.0f, 42.0f,
                                                   ui::anchor::center, ui::anchor::center,
                                                   {0.0f, 34.0f});
constexpr Rectangle kFailureTextRect = ui::place(kScreenRect, 360.0f, 44.0f,
                                                 ui::anchor::center, ui::anchor::center);
constexpr float kTapNoteBaseHeight = 0.2f;
constexpr float kTapNoteBaseLength = 0.78f;
constexpr float kTapNoteWireHeight = 0.22f;
constexpr float kTapNoteWireLength = 0.82f;
constexpr float kHoldNoteBaseHeight = 0.12f;
constexpr float kHoldNoteWireHeight = 0.14f;

float lane_center_x(int lane, int key_count) {
    const float total_width = key_count * g_settings.lane_width + (key_count - 1) * kLaneGap;
    const float left = -total_width * 0.5f + g_settings.lane_width * 0.5f;
    const int visual_lane = key_count - 1 - lane;
    return left + visual_lane * (g_settings.lane_width + kLaneGap);
}

Color darkened_lane_color(Color color) {
    return {static_cast<unsigned char>(color.r * 0.7f), static_cast<unsigned char>(color.g * 0.7f),
            static_cast<unsigned char>(color.b * 0.72f), color.a};
}

Color judge_color(judge_result result) {
    switch (result) {
        case judge_result::perfect: return g_theme->judge_perfect;
        case judge_result::great: return g_theme->judge_great;
        case judge_result::good: return g_theme->judge_good;
        case judge_result::bad: return g_theme->judge_bad;
        case judge_result::miss: return g_theme->judge_miss;
    }
    return g_theme->text;
}

const char* judge_text(judge_result result) {
    switch (result) {
        case judge_result::perfect: return "PERFECT";
        case judge_result::great: return "GREAT";
        case judge_result::good: return "GOOD";
        case judge_result::bad: return "BAD";
        case judge_result::miss: return "MISS";
    }
    return "";
}

const char* input_source_text(input_update_source source) {
    switch (source) {
        case input_update_source::simulated: return "simulated";
        case input_update_source::native_windows: return "native_windows";
        case input_update_source::polling: return "polling";
    }
    return "unknown";
}

void draw_hud(const play_session_state& state) {
    const result_data result = state.score_system.get_result_data();
    const float live_accuracy = state.score_system.get_live_accuracy();
    Rectangle score_rows[2];
    ui::vstack(kScoreRect, 30.0f, 0.0f, score_rows);
    ui::enqueue_text_in_rect(TextFormat("SCORE %07d", result.score), 30,
                             score_rows[0], g_theme->hud_score, ui::text_align::left);
    ui::enqueue_text_in_rect(TextFormat("Accuracy %.2f %%", live_accuracy), 22,
                             score_rows[1], g_theme->hud_score, ui::text_align::left);

    ui::enqueue_text_in_rect(TextFormat("FPS: %d", GetFPS()), 20,
                             kFpsRect, g_theme->hud_fps, ui::text_align::right);
    ui::enqueue_text_in_rect(TextFormat("%.2f", state.current_ms / 1000.0), 30,
                             kTimeRect, g_theme->hud_time);
    ui::enqueue_text_in_rect(TextFormat("INPUT %s (%d)", input_source_text(state.input_handler.last_update_source()),
                                        state.input_handler.last_update_event_count()),
                             20, {48.0f, 92.0f, 360.0f, 24.0f}, g_theme->hud_fps, ui::text_align::left);

    ui::enqueue_text_in_rect("HEALTH", 24, kHealthLabelRect,
                             g_theme->hud_health_label, ui::text_align::right);
    const float gauge_ratio = state.gauge.get_value() / 100.0f;
    const Color gauge_color = state.gauge.get_value() >= 70.0f ? g_theme->health_high : g_theme->health_low;
    ui::enqueue_draw_command(ui::draw_layer::base, [gauge_ratio, gauge_color]() {
        ui::draw_progress_bar(kHealthBarRect, gauge_ratio, g_theme->hud_health_bg, gauge_color,
                              g_theme->hud_health_border);
    });

    if (state.combo_display > 0) {
        ui::enqueue_text_in_rect(TextFormat("%03d", state.combo_display), 86,
                                 kComboNumberRect, g_theme->hud_combo);
        ui::enqueue_text_in_rect("COMBO", 24, kComboLabelRect, g_theme->hud_combo);
    }
}

void draw_pause_overlay() {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(kPausePanelRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("PAUSED", 42, kPauseTitleRect, g_theme->text,
                             ui::text_align::center, ui::draw_layer::modal);

    const std::array<Rectangle, 3> buttons = play_renderer::pause_button_rects();
    const char* labels[] = {"RESUME", "RESTART", "SONG SELECT"};
    for (int i = 0; i < 3; ++i) {
        ui::enqueue_button(buttons[static_cast<size_t>(i)], labels[i], 24, ui::draw_layer::modal);
    }

    ui::enqueue_text_in_rect("ESC: Resume", 20, kPauseHintRect, g_theme->text_muted,
                             ui::text_align::left, ui::draw_layer::modal);
}

void draw_judge_feedback(const play_session_state& state) {
    if (!state.display_judge.has_value() || state.judge_feedback_timer <= 0.0f) {
        return;
    }

    const Color color = Fade(judge_color(state.display_judge->result),
                             std::min(state.judge_feedback_timer / 1.0f, 1.0f));
    ui::enqueue_text_in_rect(judge_text(state.display_judge->result), 42, kJudgeFeedbackRect, color);
}

void draw_intro_overlay(const play_session_state& state) {
    const float progress = 1.0f - std::clamp(state.intro_timer / play_session_constants::kIntroDurationSeconds, 0.0f, 0.7f);
    const unsigned char alpha = static_cast<unsigned char>((1.0f - progress) * 255.0f);
    ui::enqueue_fullscreen_overlay({0, 0, 0, alpha}, ui::draw_layer::overlay);
}

void draw_failure_overlay(const play_session_state& state) {
    const float elapsed = play_session_constants::kFailureTransitionDurationSeconds - state.failure_transition_timer;
    const float fade_progress = std::clamp(elapsed / play_session_constants::kFailureFadeDurationSeconds, 0.0f, 0.7f);
    const unsigned char alpha = static_cast<unsigned char>(fade_progress * 255.0f);
    ui::enqueue_fullscreen_overlay({0, 0, 0, alpha}, ui::draw_layer::overlay);
    ui::enqueue_text_in_rect("FAILED...", 44, kFailureTextRect,
                             Fade(g_theme->hud_failure_text, std::min(fade_progress * 1.15f, 1.0f)),
                             ui::text_align::center, ui::draw_layer::modal);
}

void draw_result_transition_overlay(const play_session_state& state) {
    const float progress = std::clamp(state.result_transition_timer / play_session_constants::kResultTransitionDurationSeconds, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(progress * kResultFadeMaxAlpha * 255.0f);
    ui::enqueue_fullscreen_overlay({0, 0, 0, alpha}, ui::draw_layer::overlay);
}

}  // namespace

namespace play_renderer {

Rectangle pause_panel_rect() {
    return kPausePanelRect;
}

std::array<Rectangle, 3> pause_button_rects() {
    std::array<Rectangle, 3> buttons = {};
    ui::vstack(kPauseButtonArea, 42.0f, 16.0f, buttons);
    return buttons;
}

void draw_status(const play_session_state& state) {
    ClearBackground(g_theme->bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, g_theme->bg, g_theme->bg_alt);
    DrawText("Play", 96, 90, 44, g_theme->error);
    DrawText(state.status_text.c_str(), 96, 170, 28, g_theme->text);
    DrawText("ESC: Back to Song Select", 96, 225, 22, g_theme->text_hint);
}

void draw_world_background() {
    ClearBackground(g_theme->bg);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), g_theme->bg, g_theme->bg_alt);
}

void draw_world(const play_session_state& state, const play_note_draw_queue& draw_queue,
                float lane_start_z, float judgement_z, float lane_end_z, double visual_ms) {
    for (int lane = 0; lane < state.key_count; ++lane) {
        const float center_x = lane_center_x(lane, state.key_count);
        const bool lane_held = state.input_handler.is_lane_held(lane);
        const Color lane_fill = lane_held ? darkened_lane_color(g_theme->lane) : g_theme->lane;
        DrawCube({center_x, -0.08f, (lane_start_z + lane_end_z) * 0.5f}, g_settings.lane_width, 0.05f,
                 lane_end_z - lane_start_z, lane_fill);
        DrawCubeWires({center_x, -0.08f, (lane_start_z + lane_end_z) * 0.5f}, g_settings.lane_width, 0.05f,
                      lane_end_z - lane_start_z, g_theme->lane_wire);
    }

    const float total_width = state.key_count * g_settings.lane_width + (state.key_count - 1) * kLaneGap;
    DrawCube({0.0f, 0.01f, judgement_z}, total_width + 0.9f, 0.01f, 0.62f, g_theme->judge_line);
    DrawCube({0.0f, 0.02f, judgement_z}, total_width + 0.5f, kJudgeLineGlowHeight, 0.38f, g_theme->judge_line_glow);

    if (!draw_queue.has_active_notes()) {
        return;
    }

    const std::vector<note_state>& note_states = state.judge_system.note_states();
    const Color note_color = g_theme->note_color;
    const Color note_outline = g_theme->note_outline;

    for (int lane = 0; lane < state.key_count; ++lane) {
        for (const size_t idx : draw_queue.active_indices_for_lane(lane)) {
            const note_state& note_state = note_states[idx];
            const float head_z = static_cast<float>(judgement_z + state.lane_speed * (note_state.target_ms - visual_ms));
            const float center_x = lane_center_x(note_state.note_ref.lane, state.key_count);

            if (note_state.note_ref.type == note_type::hold) {
                const double tail_target_ms = state.timing_engine.tick_to_ms(note_state.note_ref.end_tick);
                const float tail_z = static_cast<float>(judgement_z + state.lane_speed * (tail_target_ms - visual_ms));
                const float visual_head_z = note_state.holding ? judgement_z : head_z;
                const float segment_start = std::max(std::min(visual_head_z, tail_z), lane_start_z);
                const float segment_end = std::min(std::max(head_z, tail_z), lane_end_z);
                const float hold_height = kHoldNoteBaseHeight * g_settings.note_height;
                const float hold_wire_height = kHoldNoteWireHeight * g_settings.note_height;
                if (segment_end > segment_start) {
                    DrawCube({center_x, 0.30f, (segment_start + segment_end) * 0.5f}, g_settings.lane_width * 0.92f,
                             hold_height, segment_end - segment_start, note_color);
                    DrawCubeWires({center_x, 0.30f, (segment_start + segment_end) * 0.5f},
                                  g_settings.lane_width * 0.94f, hold_wire_height, segment_end - segment_start + 0.04f,
                                  note_outline);
                }
            }

            if (note_state.note_ref.type == note_type::tap) {
                DrawCube({center_x, 0.22f, head_z}, g_settings.lane_width * 0.92f,
                         kTapNoteBaseHeight * g_settings.note_height,
                         kTapNoteBaseLength * g_settings.note_height, note_color);
                DrawCubeWires({center_x, 0.22f, head_z}, g_settings.lane_width * 0.92f,
                              kTapNoteWireHeight * g_settings.note_height,
                              kTapNoteWireLength * g_settings.note_height, note_outline);
            }
        }
    }
}

void draw_overlay(const play_session_state& state) {
    draw_hud(state);
    draw_judge_feedback(state);
    if (state.intro_playing) {
        draw_intro_overlay(state);
    }
    if (state.failure_transition_playing) {
        draw_failure_overlay(state);
    }
    if (state.result_transition_playing) {
        draw_result_transition_overlay(state);
    }
    if (state.paused) {
        draw_pause_overlay();
    }
}

}  // namespace play_renderer
