#include "play_renderer.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <string>

#include "game_settings.h"
#include "localization/localization.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "raymath.h"
#include "rlgl.h"

namespace {

constexpr float kLaneGap = 0.2f;
constexpr float kJudgeLineGlowHeight = 0.04f;
constexpr float kResultFadeMaxAlpha = 0.65f;
constexpr float kUiFontScale = 1.5f;
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kPausePanelRect = ui::center(kScreenRect, 630.0f, 480.0f);
constexpr Rectangle kPauseTitleRect = {kPausePanelRect.x, kPausePanelRect.y, kPausePanelRect.width, 96.0f};
constexpr Rectangle kPauseButtonArea = {
    kPausePanelRect.x + 60.0f,
    kPausePanelRect.y + 132.0f,
    510.0f,
    3.0f * 63.0f + 2.0f * 24.0f
};
constexpr Rectangle kPauseHintRect = {
    kPausePanelRect.x + 36.0f,
    kPausePanelRect.y + kPausePanelRect.height - 75.0f,
    kPausePanelRect.width - 72.0f,
    45.0f
};
constexpr Rectangle kScoreRect = ui::place(kScreenRect, 600.0f, 90.0f,
                                           ui::anchor::top_left, ui::anchor::top_left,
                                           Vector2{72.0f, 30.0f});
constexpr Rectangle kRcRect = ui::place(kScreenRect, 360.0f, 42.0f,
                                        ui::anchor::top_left, ui::anchor::top_left,
                                        Vector2{72.0f, 100.0f});
constexpr Rectangle kTimeRect = ui::place(kScreenRect, 300.0f, 45.0f,
                                          ui::anchor::top_center, ui::anchor::top_center,
                                          Vector2{0.0f, 51.0f});
constexpr Rectangle kFpsRect = ui::place(kScreenRect, 180.0f, 30.0f,
                                         ui::anchor::bottom_right, ui::anchor::bottom_right,
                                         Vector2{-15.0f, 0.0f});
constexpr Rectangle kSongInfoRect = ui::place(kScreenRect, 540.0f, 144.0f,
                                              ui::anchor::top_right, ui::anchor::top_right,
                                              Vector2{-30.0f, 30.0f});
constexpr Rectangle kSongInfoJacketRect = {
    kSongInfoRect.x + 16.0f,
    kSongInfoRect.y + 16.0f,
    112.0f,
    112.0f
};
constexpr Rectangle kSongInfoTitleRect = {
    kSongInfoRect.x + 148.0f,
    kSongInfoRect.y + 28.0f,
    kSongInfoRect.width - 172.0f,
    45.0f
};
constexpr Rectangle kSongInfoDifficultyRect = {
    kSongInfoRect.x + 148.0f,
    kSongInfoRect.y + 83.0f,
    kSongInfoRect.width - 172.0f,
    34.0f
};
constexpr Rectangle kComboNumberRect = ui::place(kScreenRect, 450.0f, 129.0f,
                                                 ui::anchor::center, ui::anchor::center,
                                                 Vector2{0.0f, -120.0f});
constexpr Rectangle kComboLabelRect = ui::place(kScreenRect, 300.0f, 36.0f,
                                                ui::anchor::center, ui::anchor::center,
                                                {0.0f, 0.0f});
constexpr Rectangle kJudgeFeedbackRect = ui::place(kScreenRect, 480.0f, 63.0f,
                                                   ui::anchor::center, ui::anchor::center,
                                                   Vector2{0.0f, 51.0f});
constexpr Rectangle kFailureTextRect = ui::place(kScreenRect, 540.0f, 66.0f,
                                                 ui::anchor::center, ui::anchor::center);
constexpr float kTapNoteBaseLength = 0.78f;
constexpr float kJudgeLineY = 0.40f;
constexpr float kJudgeLineGlowY = 0.46f;
constexpr float kTapNoteY = 0.15f;
constexpr float kHoldNoteY = 0.15f;
constexpr float kReleaseMarkerLift = 2.5f;
constexpr float kStayDotY = 0.24f;
constexpr float kLowHealthVignetteThreshold = 35.0f;
constexpr float kDamageVignetteEdgeWidth = 220.0f;
constexpr float kDamageVignetteMaxAlpha = 86.0f;

float lane_center_x(int lane, int key_count) {
    const float total_width = key_count * g_settings.lane_width + (key_count - 1) * kLaneGap;
    const float left = -total_width * 0.5f + g_settings.lane_width * 0.5f;
    const int visual_lane = key_count - 1 - lane;
    return left + visual_lane * (g_settings.lane_width + kLaneGap);
}

float note_center_x(const note_data& note, int key_count) {
    const float first = lane_center_x(note.lane, key_count);
    const float last = lane_center_x(note_last_lane(note), key_count);
    return (first + last) * 0.5f;
}

float note_visual_width(const note_data& note) {
    const int width = note_lane_width(note);
    return static_cast<float>(width) * g_settings.lane_width +
           static_cast<float>(width - 1) * kLaneGap;
}

float note_body_width(const note_data& note) {
    const float single_lane_inset = g_settings.lane_width * 0.08f;
    return std::max(g_settings.lane_width * 0.2f, note_visual_width(note) - single_lane_inset);
}

float note_hold_body_width(const note_data& note) {
    const float hold_inset = g_settings.lane_width * 0.20f;
    return std::max(g_settings.lane_width * 0.18f, note_visual_width(note) - hold_inset);
}

int ui_font(int font_size) {
    return static_cast<int>(std::lround(static_cast<float>(font_size) * kUiFontScale));
}

void draw_note_plane(float center_x, float y, float center_z, float width, float length, Color fill) {
    DrawPlane({center_x, y, center_z}, {width, length}, fill);
}

Color tap_gradient_color(Color base, float t) {
    const float edge_factor = std::pow(std::fabs(t - 0.5f) * 2.0f, 1.25f);
    const float highlight = 0.26f + (1.0f - edge_factor) * 0.26f;
    const unsigned char alpha = static_cast<unsigned char>(166.0f + edge_factor * 42.0f);
    return with_alpha(lerp_color(base, WHITE, highlight), alpha);
}

void draw_tap_gradient_plane(float center_x, float center_z, float width, float length, Color base) {
    constexpr int kGradientSteps = 16;
    const float left = center_x - width * 0.5f;
    const float near_z = center_z - length * 0.5f;
    const float far_z = center_z + length * 0.5f;
    const float y = kTapNoteY + 0.014f;

    rlBegin(RL_QUADS);
    for (int i = 0; i < kGradientSteps; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(kGradientSteps);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(kGradientSteps);
        const float x0 = left + width * t0;
        const float x1 = left + width * t1;
        const Color c0 = tap_gradient_color(base, t0);
        const Color c1 = tap_gradient_color(base, t1);

        rlColor4ub(c0.r, c0.g, c0.b, c0.a);
        rlVertex3f(x0, y, near_z);
        rlVertex3f(x0, y, far_z);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a);
        rlVertex3f(x1, y, far_z);
        rlVertex3f(x1, y, near_z);
    }
    rlEnd();
}

void draw_horizontal_gradient_plane(float center_x, float y, float center_z,
                                    float width, float length, Color left_color, Color right_color) {
    const float left = center_x - width * 0.5f;
    const float right = center_x + width * 0.5f;
    const float near_z = center_z - length * 0.5f;
    const float far_z = center_z + length * 0.5f;

    rlBegin(RL_QUADS);
    rlColor4ub(left_color.r, left_color.g, left_color.b, left_color.a);
    rlVertex3f(left, y, near_z);
    rlVertex3f(left, y, far_z);
    rlColor4ub(right_color.r, right_color.g, right_color.b, right_color.a);
    rlVertex3f(right, y, far_z);
    rlVertex3f(right, y, near_z);
    rlEnd();
}

void draw_depth_gradient_plane(float center_x, float y, float center_z,
                               float width, float length, Color near_color, Color far_color) {
    const float left = center_x - width * 0.5f;
    const float right = center_x + width * 0.5f;
    const float near_z = center_z - length * 0.5f;
    const float far_z = center_z + length * 0.5f;

    rlBegin(RL_QUADS);
    rlColor4ub(near_color.r, near_color.g, near_color.b, near_color.a);
    rlVertex3f(left, y, near_z);
    rlVertex3f(right, y, near_z);
    rlColor4ub(far_color.r, far_color.g, far_color.b, far_color.a);
    rlVertex3f(right, y, far_z);
    rlVertex3f(left, y, far_z);
    rlEnd();
}

Color hold_gradient_color(Color base, float t) {
    const float edge_factor = std::pow(std::fabs(t - 0.5f) * 2.0f, 1.55f);
    const unsigned char alpha = static_cast<unsigned char>(84.0f + edge_factor * 110.0f);
    return with_alpha(lerp_color(base, WHITE, edge_factor * 0.10f), alpha);
}

void draw_hold_gradient_plane(float center_x, float center_z, float width, float length, Color base) {
    constexpr int kGradientSteps = 24;
    const float left = center_x - width * 0.5f;
    const float near_z = center_z - length * 0.5f;
    const float far_z = center_z + length * 0.5f;
    const float y = kHoldNoteY + 0.006f;

    rlBegin(RL_QUADS);
    for (int i = 0; i < kGradientSteps; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(kGradientSteps);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(kGradientSteps);
        const float x0 = left + width * t0;
        const float x1 = left + width * t1;
        const Color c0 = hold_gradient_color(base, t0);
        const Color c1 = hold_gradient_color(base, t1);

        rlColor4ub(c0.r, c0.g, c0.b, c0.a);
        rlVertex3f(x0, y, near_z);
        rlVertex3f(x0, y, far_z);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a);
        rlVertex3f(x1, y, far_z);
        rlVertex3f(x1, y, near_z);
    }
    rlEnd();
}

void draw_tap_slab(float center_x, float center_z, float width, float length,
                   Color fill, bool release_style = false, bool ray_style = false) {
    const Color tap_base = ray_style
                               ? lerp_color(fill, {180, 132, 255, 255}, 0.72f)
                               : release_style
                               ? lerp_color(fill, {255, 118, 156, 255}, 0.38f)
                               : lerp_color(fill, WHITE, 0.56f);
    const Color edge = lerp_color(tap_base, WHITE, 0.30f);

    const float rim_length = std::max(0.024f, length * 0.16f);
    const float side_width = std::max(0.024f, std::min(width * 0.055f, length * 0.18f));
    const float frame_y = kTapNoteY + 0.060f;
    const Color frame_left = with_alpha(lerp_color(edge, WHITE, 0.12f), 232);
    const Color frame_right = with_alpha(lerp_color(edge, BLACK, 0.08f), 218);
    const Color frame_near = with_alpha(lerp_color(edge, WHITE, 0.18f), 238);
    const Color frame_far = with_alpha(lerp_color(edge, BLACK, 0.12f), 210);
    draw_tap_gradient_plane(center_x, center_z, width, length, tap_base);
    draw_horizontal_gradient_plane(center_x, frame_y, center_z - length * 0.5f + rim_length * 0.5f,
                                   width, rim_length, frame_left, frame_right);
    draw_horizontal_gradient_plane(center_x, frame_y, center_z + length * 0.5f - rim_length * 0.5f,
                                   width, rim_length, frame_left, frame_right);
    draw_depth_gradient_plane(center_x - width * 0.5f + side_width * 0.5f, frame_y, center_z,
                              side_width, length, frame_near, frame_far);
    draw_depth_gradient_plane(center_x + width * 0.5f - side_width * 0.5f, frame_y, center_z,
                              side_width, length, frame_near, frame_far);
}

void draw_hold_body(float center_x, float center_z, float width, float length, Color fill, bool ray_style = false) {
    const Color hold_base = ray_style
                                ? lerp_color(fill, g_theme->accent, 0.68f)
                                : lerp_color(fill, WHITE, 0.94f);
    const Color edge = lerp_color(hold_base, WHITE, 0.24f);
    const float cap_length = std::min(0.32f, std::max(0.08f, length * 0.16f));
    const float cap_overhang = std::min(g_settings.lane_width * 0.035f, width * 0.015f);
    const float cap_width = width + cap_overhang * 2.0f;

    draw_hold_gradient_plane(center_x, center_z, width, length, hold_base);
    const Color cap_left = with_alpha(lerp_color(edge, WHITE, 0.16f), 230);
    const Color cap_right = with_alpha(lerp_color(edge, BLACK, 0.10f), 214);
    draw_horizontal_gradient_plane(center_x, kHoldNoteY + 0.074f,
                                   center_z - length * 0.5f + cap_length * 0.5f,
                                   cap_width, cap_length, cap_left, cap_right);
    draw_horizontal_gradient_plane(center_x, kHoldNoteY + 0.074f,
                                   center_z + length * 0.5f - cap_length * 0.5f,
                                   cap_width, cap_length, cap_left, cap_right);
}

Color stay_gradient_color(Color base, float t) {
    const float center_factor = 1.0f - std::pow(std::fabs(t - 0.5f) * 2.0f, 1.35f);
    const float highlight = 0.16f + center_factor * 0.52f;
    const unsigned char alpha = static_cast<unsigned char>(208.0f + center_factor * 24.0f);
    return with_alpha(lerp_color(base, WHITE, highlight), alpha);
}

void draw_stay_gradient_bar(float center_x, float center_z, float width, float length, Color base) {
    constexpr int kGradientSteps = 18;
    const float left = center_x - width * 0.5f;
    const float near_z = center_z - length * 0.5f;
    const float far_z = center_z + length * 0.5f;
    const float y = kStayDotY + 0.018f;

    rlBegin(RL_QUADS);
    for (int i = 0; i < kGradientSteps; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(kGradientSteps);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(kGradientSteps);
        const float x0 = left + width * t0;
        const float x1 = left + width * t1;
        const Color c0 = stay_gradient_color(base, t0);
        const Color c1 = stay_gradient_color(base, t1);

        rlColor4ub(c0.r, c0.g, c0.b, c0.a);
        rlVertex3f(x0, y, near_z);
        rlVertex3f(x0, y, far_z);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a);
        rlVertex3f(x1, y, far_z);
        rlVertex3f(x1, y, near_z);
    }
    rlEnd();
}

void draw_stay_dot(float center_x, float center_z, float width, Color fill, bool ray_style = false) {
    const Color stay_base = ray_style
                                ? lerp_color(WHITE, {224, 214, 255, 255}, 0.14f)
                                : lerp_color({70, 236, 224, 255}, fill, 0.14f);
    const Color end_edge = with_alpha(lerp_color(stay_base, WHITE, 0.34f), 226);
    const Color end_inner = with_alpha(lerp_color(stay_base, WHITE, 0.12f), 178);
    const float bar_width = std::max(g_settings.lane_width * 0.54f, width * 1.04f);
    const float bar_length = 0.28f;
    const float cap_width = std::min(g_settings.lane_width * 0.070f, bar_width * 0.055f);
    const float cap_length = bar_length * 1.55f;

    draw_stay_gradient_bar(center_x, center_z, bar_width, bar_length, stay_base);
    draw_depth_gradient_plane(center_x - bar_width * 0.5f + cap_width * 0.5f, kStayDotY + 0.032f,
                              center_z, cap_width, cap_length, end_inner, end_edge);
    draw_depth_gradient_plane(center_x + bar_width * 0.5f - cap_width * 0.5f, kStayDotY + 0.032f,
                              center_z, cap_width, cap_length, end_inner, end_edge);
}

void draw_release_polygon_triangle(Vector3 a, Vector3 b, Vector3 c, Color color) {
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(c, b, a, color);
}

void draw_release_contour_segment(Vector3 origin, Vector3 axis_x, Vector3 axis_y,
                                  float x0, float y0, float x1, float y1,
                                  float thickness, Color color) {
    const auto point = [&](float x, float y) {
        return Vector3Add(origin, Vector3Add(Vector3Scale(axis_x, x), Vector3Scale(axis_y, y)));
    };
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        return;
    }

    const float nx = -dy / length * thickness * 0.5f;
    const float ny = dx / length * thickness * 0.5f;
    const Vector3 p0 = point(x0 + nx, y0 + ny);
    const Vector3 p1 = point(x0 - nx, y0 - ny);
    const Vector3 p2 = point(x1 - nx, y1 - ny);
    const Vector3 p3 = point(x1 + nx, y1 + ny);
    draw_release_polygon_triangle(p0, p1, p2, color);
    draw_release_polygon_triangle(p0, p2, p3, color);
}

void draw_release_chevron_polygon(Vector3 origin, Vector3 axis_x, Vector3 axis_y,
                                  float width, float height, Color color, Color contour_color) {
    const auto point = [&](float x, float y) {
        return Vector3Add(origin, Vector3Add(Vector3Scale(axis_x, x), Vector3Scale(axis_y, y)));
    };
    const Vector3 left_outer_bottom = point(-width * 0.50f, -height * 0.38f);
    const Vector3 left_outer_top = point(-width * 0.50f, -height * 0.10f);
    const Vector3 center_top = point(0.0f, height * 0.36f);
    const Vector3 center_bottom = point(0.0f, height * 0.08f);
    const Vector3 right_outer_top = point(width * 0.50f, -height * 0.10f);
    const Vector3 right_outer_bottom = point(width * 0.50f, -height * 0.38f);

    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    draw_release_polygon_triangle(left_outer_bottom, left_outer_top, center_top, color);
    draw_release_polygon_triangle(left_outer_bottom, center_top, center_bottom, color);
    draw_release_polygon_triangle(center_bottom, center_top, right_outer_top, color);
    draw_release_polygon_triangle(center_bottom, right_outer_top, right_outer_bottom, color);
    const float contour = std::clamp(height * 0.110f, 0.040f, 0.082f);
    draw_release_contour_segment(origin, axis_x, axis_y, -width * 0.50f, -height * 0.38f,
                                 -width * 0.50f, -height * 0.10f, contour, contour_color);
    draw_release_contour_segment(origin, axis_x, axis_y, -width * 0.50f, -height * 0.10f,
                                 0.0f, height * 0.36f, contour, contour_color);
    draw_release_contour_segment(origin, axis_x, axis_y, 0.0f, height * 0.36f,
                                 width * 0.50f, -height * 0.10f, contour, contour_color);
    draw_release_contour_segment(origin, axis_x, axis_y, width * 0.50f, -height * 0.10f,
                                 width * 0.50f, -height * 0.38f, contour, contour_color);
    draw_release_contour_segment(origin, axis_x, axis_y, width * 0.50f, -height * 0.38f,
                                 0.0f, height * 0.08f, contour, contour_color);
    draw_release_contour_segment(origin, axis_x, axis_y, 0.0f, height * 0.08f,
                                 -width * 0.50f, -height * 0.38f, contour, contour_color);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();
}

void draw_release_marker(float center_x, float center_z, float width, Color fill,
                         const Camera3D& camera, bool ray_style = false) {
    const Color release_seed = ray_style ? Color{190, 112, 255, 255} : Color{255, 90, 132, 255};
    const Color release_base = lerp_color(release_seed, fill, ray_style ? 0.24f : 0.16f);
    const Color marker_color = with_alpha(lerp_color(release_base, WHITE, 0.28f), 255);
    const Color contour_color = with_alpha(lerp_color(release_base, BLACK, 0.16f), 255);

    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 axis_x_raw = Vector3CrossProduct(forward, camera.up);
    Vector3 axis_x = {};
    if (Vector3Length(axis_x_raw) <= 0.0001f) {
        axis_x = {1.0f, 0.0f, 0.0f};
    } else {
        axis_x = Vector3Normalize(axis_x_raw);
    }
    const Vector3 axis_y = Vector3Normalize(Vector3CrossProduct(axis_x, forward));
    Vector3 origin = {center_x, kTapNoteY, center_z};
    origin = Vector3Add(origin, Vector3Scale(axis_y, kReleaseMarkerLift));
    origin = Vector3Add(origin, Vector3Scale(forward, -0.14f));

    const float marker_width = std::max(g_settings.lane_width * 0.76f, width * 0.78f);
    const float marker_height = g_settings.lane_width * 0.40f;

    const float marker_bottom_offset = marker_height * 0.50f;
    const Vector3 marker_origin = Vector3Add(origin, Vector3Scale(axis_y, marker_bottom_offset));
    draw_release_chevron_polygon(marker_origin, axis_x, axis_y,
                                 marker_width, marker_height, marker_color, contour_color);
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

Color effect_judge_color(judge_result result) {
    if (result == judge_result::perfect) {
        return lerp_color(g_theme->judge_line_glow, WHITE, 0.35f);
    }
    return judge_color(result);
}

float ease_out(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - clamped) * (1.0f - clamped);
}

Color fade_to_alpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 255.0f));
    return color;
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

std::string difficulty_text(const play_session_state& state) {
    if (!state.chart_data.has_value()) {
        return "";
    }

    const chart_meta& meta = state.chart_data->meta;
    if (meta.level > 0.0f) {
        char level_text[32] = {};
        std::snprintf(level_text, sizeof(level_text), "%.1f", meta.level);
        return meta.difficulty.empty()
                   ? std::string("Lv. ") + level_text
                   : meta.difficulty + "  Lv. " + level_text;
    }
    return meta.difficulty;
}

void draw_song_info_panel(const play_session_state& state, const Texture2D* jacket_texture) {
    if (!state.song_data.has_value() && !state.chart_data.has_value()) {
        return;
    }

    const std::string title =
        state.song_data.has_value() && !state.song_data->meta.title.empty()
            ? state.song_data->meta.title
            : "Unknown Title";
    const std::string difficulty = difficulty_text(state);

    ui::enqueue_draw_command(ui::draw_layer::base, [title, difficulty, jacket_texture]() {
        ui::draw_rect_f(kSongInfoRect, with_alpha(g_theme->panel, 212));
        ui::draw_rect_lines(kSongInfoRect, 2.0f, with_alpha(g_theme->border, 214));

        if (jacket_texture != nullptr && jacket_texture->id != 0) {
            const Rectangle source = {
                0.0f,
                0.0f,
                static_cast<float>(jacket_texture->width),
                static_cast<float>(jacket_texture->height)
            };
            DrawTexturePro(*jacket_texture, source, kSongInfoJacketRect, {0.0f, 0.0f}, 0.0f, WHITE);
        } else {
            ui::draw_rect_f(kSongInfoJacketRect, with_alpha(g_theme->section, 235));
            ui::draw_rect_lines(kSongInfoJacketRect, 2.0f, with_alpha(g_theme->border_light, 220));
            ui::draw_text_in_rect("NO JACKET", ui_font(16), kSongInfoJacketRect,
                                  g_theme->text_muted, ui::text_align::center);
        }

        draw_marquee_text(title.c_str(), kSongInfoTitleRect, ui_font(25), g_theme->text, GetTime());
        if (!difficulty.empty()) {
            draw_marquee_text(difficulty.c_str(), kSongInfoDifficultyRect, ui_font(20),
                              g_theme->text_secondary, GetTime());
        }
    });
}

void draw_lane_judge_effect_segment(float center_x, float start_z, float end_z,
                                    float width, Color color, float remaining) {
    constexpr int kEffectSegments = 18;
    const float length = std::fabs(end_z - start_z);
    if (length <= 0.0f) {
        return;
    }
    const float delta_z = end_z - start_z;

    const float segment_length = length / static_cast<float>(kEffectSegments);
    for (int segment = 0; segment < kEffectSegments; ++segment) {
        const float near_t = static_cast<float>(segment) / static_cast<float>(kEffectSegments);
        const float far_t = static_cast<float>(segment + 1) / static_cast<float>(kEffectSegments);
        const float fade = 1.0f - near_t;
        const Color segment_color = fade_to_alpha(color, 214.0f * remaining * fade * fade);
        const float segment_start_z = start_z + delta_z * near_t;
        const float segment_end_z = start_z + delta_z * far_t;
        DrawCube({center_x, kJudgeLineY - 0.018f, (segment_start_z + segment_end_z) * 0.5f},
                 width * (0.96f - near_t * 0.08f),
                 0.026f,
                 segment_length,
                 segment_color);
    }
}

void draw_lane_judge_effect(const play_session_state& state, int lane,
                            float lane_start_z, float judgement_z, float lane_end_z) {
    const lane_judge_effect& effect = state.lane_judge_effects[static_cast<std::size_t>(lane)];
    if (effect.timer <= 0.0f) {
        return;
    }

    const float remaining =
        effect.timer / play_session_constants::kLaneJudgeEffectDurationSeconds;
    const float age = 1.0f - std::clamp(remaining, 0.0f, 1.0f);
    const float pop = ease_out(age);
    const Color color = effect_judge_color(effect.result);
    const int lane_width_count = std::max(1, std::min(effect.lane_width, state.key_count - lane));
    const float first_x = lane_center_x(lane, state.key_count);
    const float last_x = lane_center_x(lane + lane_width_count - 1, state.key_count);
    const float center_x = (first_x + last_x) * 0.5f;
    const float effect_width =
        static_cast<float>(lane_width_count) * g_settings.lane_width +
        static_cast<float>(lane_width_count - 1) * kLaneGap;
    const Color bright_color = effect.result == judge_result::perfect
                                   ? color
                                   : lerp_color(color, WHITE, 0.12f);
    const float effect_length = std::max(0.0f, lane_end_z - judgement_z) * 0.10f * (0.72f + pop * 0.28f);

    draw_lane_judge_effect_segment(center_x, judgement_z, judgement_z + effect_length,
                                   effect_width, bright_color, remaining);
    draw_lane_judge_effect_segment(center_x, judgement_z, judgement_z - effect_length,
                                   effect_width, bright_color, remaining);
}

Color note_draw_color(const note_state& note_state, Color base) {
    if (note_state.note_ref.is_ray) {
        switch (note_state.note_ref.type) {
            case note_type::hold:
                return lerp_color(base, {142, 92, 236, 255}, 0.72f);
            case note_type::release:
                return lerp_color(base, {198, 116, 255, 255}, 0.76f);
            case note_type::stay:
                return lerp_color(base, WHITE, 0.90f);
            case note_type::tap:
                return lerp_color(base, {180, 132, 255, 255}, 0.68f);
        }
    }
    switch (note_state.note_ref.type) {
        case note_type::release:
            return lerp_color(base, {255, 105, 148, 255}, 0.42f);
        case note_type::stay:
            return lerp_color(base, g_theme->judge_perfect, 0.25f);
        case note_type::tap:
            return lerp_color(base, WHITE, 0.42f);
        case note_type::hold:
            return lerp_color(base, WHITE, 0.96f);
    }
    return base;
}

bool should_draw_note_in_pass(note_type type, int pass) {
    switch (pass) {
        case 0:
            return type == note_type::hold;
        case 1:
            return type == note_type::tap;
        case 2:
            return type == note_type::stay;
        case 3:
            return type == note_type::release;
    }
    return false;
}

void draw_hud(const play_session_state& state) {
    const result_data result = state.score_system.get_result_data();
    const float live_accuracy = state.score_system.get_live_accuracy();
    ui::enqueue_text_in_rect(TextFormat("SCORE %07d", result.score), ui_font(30),
                             kScoreRect, g_theme->hud_score, ui::text_align::left);
    ui::enqueue_text_in_rect(TextFormat("RC %.2f", state.performance_system.current_rc()), ui_font(24),
                             kRcRect, g_theme->text_secondary, ui::text_align::left);

    ui::enqueue_text_in_rect(TextFormat("FPS: %d", GetFPS()), ui_font(20),
                             kFpsRect, g_theme->hud_fps, ui::text_align::right);
    ui::enqueue_text_in_rect(TextFormat("%.2f%%", live_accuracy), ui_font(30),
                             kTimeRect, g_theme->hud_time);

    if (state.combo_display > 0) {
        ui::enqueue_text_in_rect(TextFormat("%03d", state.combo_display), ui_font(86),
                                 kComboNumberRect, g_theme->hud_combo);
        ui::enqueue_text_in_rect("COMBO", ui_font(24), kComboLabelRect, g_theme->hud_combo);
    }
}

void draw_low_health_vignette(const play_session_state& state) {
    const float health = state.gauge.get_value();
    if (health >= kLowHealthVignetteThreshold) {
        return;
    }

    const float danger = 1.0f - std::clamp(health / kLowHealthVignetteThreshold, 0.0f, 1.0f);
    const float pulse = 0.85f + 0.15f * std::sin(static_cast<float>(GetTime()) * 6.0f);
    const unsigned char alpha =
        static_cast<unsigned char>(std::clamp(danger * pulse * kDamageVignetteMaxAlpha, 0.0f, 255.0f));
    if (alpha == 0) {
        return;
    }

    ui::enqueue_draw_command(ui::draw_layer::overlay, [alpha]() {
        const Color edge = with_alpha(g_theme->health_low, alpha);
        constexpr Color transparent = {0, 0, 0, 0};
        DrawRectangleGradientV(0, 0, kScreenWidth, static_cast<int>(kDamageVignetteEdgeWidth),
                               edge, transparent);
        DrawRectangleGradientV(0, kScreenHeight - static_cast<int>(kDamageVignetteEdgeWidth),
                               kScreenWidth, static_cast<int>(kDamageVignetteEdgeWidth),
                               transparent, edge);
        DrawRectangleGradientH(0, 0, static_cast<int>(kDamageVignetteEdgeWidth), kScreenHeight,
                               edge, transparent);
        DrawRectangleGradientH(kScreenWidth - static_cast<int>(kDamageVignetteEdgeWidth), 0,
                               static_cast<int>(kDamageVignetteEdgeWidth), kScreenHeight,
                               transparent, edge);

        const Color corner = with_alpha(g_theme->health_low, static_cast<unsigned char>(alpha * 0.55f));
        const float corner_size = kDamageVignetteEdgeWidth * 1.2f;
        DrawRectangleGradientEx({0.0f, 0.0f, corner_size, corner_size}, corner, transparent, transparent, transparent);
        DrawRectangleGradientEx({kScreenWidth - corner_size, 0.0f, corner_size, corner_size},
                                transparent, corner, transparent, transparent);
        DrawRectangleGradientEx({0.0f, kScreenHeight - corner_size, corner_size, corner_size},
                                transparent, transparent, corner, transparent);
        DrawRectangleGradientEx({kScreenWidth - corner_size, kScreenHeight - corner_size, corner_size, corner_size},
                                transparent, transparent, transparent, corner);
    });
}

void draw_pause_overlay() {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(kPausePanelRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("PAUSED", ui_font(42), kPauseTitleRect, g_theme->text,
                             ui::text_align::center, ui::draw_layer::modal);

    const std::array<Rectangle, 3> buttons = play_renderer::pause_button_rects();
    const char* labels[] = {"RESUME", "RESTART", "SONG SELECT"};
    for (int i = 0; i < 3; ++i) {
        ui::enqueue_button(buttons[static_cast<size_t>(i)], labels[i], ui_font(24), ui::draw_layer::modal);
    }

    ui::enqueue_text_in_rect("ESC: Resume", ui_font(20), kPauseHintRect, g_theme->text_muted,
                             ui::text_align::left, ui::draw_layer::modal);
}

void draw_judge_feedback(const play_session_state& state) {
    if (!state.display_judge.has_value() || state.judge_feedback_timer <= 0.0f) {
        return;
    }

    const Color color = Fade(judge_color(state.display_judge->result),
                             std::min(state.judge_feedback_timer / 1.0f, 1.0f));
    ui::enqueue_text_in_rect(judge_text(state.display_judge->result), ui_font(42), kJudgeFeedbackRect, color);
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
    ui::enqueue_text_in_rect("FAILED...", ui_font(44), kFailureTextRect,
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
    ui::vstack(kPauseButtonArea, 63.0f, 24.0f, buttons);
    return buttons;
}

void draw_status(const play_session_state& state) {
    draw_scene_background(*g_theme);
    ui::draw_text_f(localization::tr_literal("Play"), 144.0f, 135.0f, ui_font(44), g_theme->error);
    ui::draw_text_f(state.status_text.c_str(), 144.0f, 255.0f, ui_font(28), g_theme->text);
    ui::draw_text_f(localization::tr_literal("ESC: Back to Song Select"), 144.0f, 337.5f, ui_font(22), g_theme->text_hint);
}

void draw_world_background() {
    draw_scene_background(*g_theme);
}

void draw_world(const play_session_state& state, const play_note_draw_queue& draw_queue,
                const Camera3D& camera, float lane_start_z, float judgement_z, float lane_end_z, double visual_ms) {
    for (int lane = 0; lane < state.key_count; ++lane) {
        const float center_x = lane_center_x(lane, state.key_count);
        const float lane_dim = std::clamp(state.lane_hold_dim_amounts[static_cast<std::size_t>(lane)], 0.0f, 1.0f);
        const Color lane_fill = lerp_color(g_theme->lane, g_theme->lane_pressed, lane_dim);
        DrawCube({center_x, -0.08f, (lane_start_z + lane_end_z) * 0.5f}, g_settings.lane_width, 0.05f,
                 lane_end_z - lane_start_z, lane_fill);
        DrawCubeWires({center_x, -0.08f, (lane_start_z + lane_end_z) * 0.5f}, g_settings.lane_width, 0.05f,
                      lane_end_z - lane_start_z, g_theme->lane_wire);
    }

    const float total_width = state.key_count * g_settings.lane_width + (state.key_count - 1) * kLaneGap;

    if (draw_queue.has_active_notes()) {
        const std::vector<note_state>& note_states = state.judge_system.note_states();
        const Color note_color = g_theme->note_color;

        for (int pass = 0; pass < 4; ++pass) {
            for (int lane = 0; lane < state.key_count; ++lane) {
                for (const size_t idx : draw_queue.active_indices_for_lane(lane)) {
                    const note_state& note_state = note_states[idx];
                    if (!should_draw_note_in_pass(note_state.note_ref.type, pass)) {
                        continue;
                    }

                    const Color note_color_for_type = note_draw_color(note_state, note_color);
                    const float head_z = static_cast<float>(judgement_z + state.lane_speed * (note_state.target_ms - visual_ms));
                    const float center_x = note_center_x(note_state.note_ref, state.key_count);
                    const float visual_width = note_visual_width(note_state.note_ref);
                    const float body_width = note_body_width(note_state.note_ref);
                    const float hold_body_width = note_hold_body_width(note_state.note_ref);

                    if (note_state.note_ref.type == note_type::hold) {
                        const double tail_target_ms = state.timing_engine.tick_to_ms(note_state.note_ref.end_tick);
                        const float tail_z = static_cast<float>(judgement_z + state.lane_speed * (tail_target_ms - visual_ms));
                        const float visual_head_z = note_state.is_holding() ? judgement_z : head_z;
                        const float segment_start = std::max(std::min(visual_head_z, tail_z), lane_start_z);
                        const float segment_end = std::min(std::max(head_z, tail_z), lane_end_z);
                        if (segment_end > segment_start) {
                            draw_hold_body(center_x, (segment_start + segment_end) * 0.5f,
                                           hold_body_width, segment_end - segment_start, note_color_for_type,
                                           note_state.note_ref.is_ray);
                        }
                    } else if (note_state.note_ref.type == note_type::stay) {
                        draw_stay_dot(center_x, head_z, body_width, note_color_for_type,
                                      note_state.note_ref.is_ray);
                    } else {
                        draw_tap_slab(center_x, head_z, body_width,
                                      kTapNoteBaseLength * g_settings.note_height,
                                      note_color_for_type,
                                      note_state.note_ref.type == note_type::release,
                                      note_state.note_ref.is_ray);
                        if (note_state.note_ref.type == note_type::release) {
                            draw_release_marker(center_x, head_z, body_width, note_color_for_type, camera,
                                                note_state.note_ref.is_ray);
                        }
                    }
                }
            }
        }
    }

    for (int lane = 0; lane < state.key_count; ++lane) {
        draw_lane_judge_effect(state, lane, lane_start_z, judgement_z, lane_end_z);
    }

    DrawCube({0.0f, kJudgeLineY, judgement_z}, total_width + 0.9f, 0.01f, 0.62f, g_theme->judge_line);
    DrawCube({0.0f, kJudgeLineGlowY, judgement_z}, total_width + 0.5f, kJudgeLineGlowHeight, 0.38f, g_theme->judge_line_glow);
}

void draw_overlay(const play_session_state& state, const Texture2D* jacket_texture) {
    draw_hud(state);
    draw_song_info_panel(state, jacket_texture);
    draw_judge_feedback(state);
    draw_low_health_vignette(state);
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
