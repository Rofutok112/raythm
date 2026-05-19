#include "editor_timeline_view.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "game_settings.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

Vector2 rect_center(Rectangle rect) {
    return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

void draw_stay_note_marker(Rectangle rect, Color fill, Color outline) {
    const Vector2 center = rect_center(rect);
    const float radius = std::max(3.5f, std::min(rect.width, rect.height) * 0.34f);
    const float bar_width = std::max(12.0f, rect.width * 0.78f);
    const Rectangle bar = {center.x - bar_width * 0.5f, center.y - 1.25f, bar_width, 2.5f};
    DrawRectangleRounded(bar, 0.6f, 4, fill);
    ui::draw_rect_lines(bar, 1.0f, outline);
    DrawCircleV(center, radius, fill);
    DrawCircleLines(static_cast<int>(std::lround(center.x)), static_cast<int>(std::lround(center.y)),
                    radius, outline);
}

void draw_release_note_marker(Rectangle rect, Color fill, Color outline) {
    DrawRectangleRounded(rect, 0.3f, 6, fill);
    ui::draw_rect_lines(rect, 1.5f, outline);

    const float center_x = rect.x + rect.width * 0.5f;
    const float tip_y = rect.y - 17.0f;
    const float wing_y = rect.y - 5.0f;
    const float half_width = std::min(11.0f, rect.width * 0.28f);
    const Rectangle stem = {center_x - 2.0f, rect.y - 5.0f, 4.0f, 9.0f};
    DrawRectangleRounded(stem, 0.4f, 4, fill);
    ui::draw_rect_lines(stem, 1.0f, outline);
    DrawTriangle({center_x, tip_y}, {center_x - half_width, wing_y}, {center_x + half_width, wing_y}, fill);
    DrawTriangleLines({center_x, tip_y}, {center_x - half_width, wing_y}, {center_x + half_width, wing_y}, outline);
}

void draw_note_marker(const editor_timeline_note& note,
                      const editor_timeline_note_draw_info& info,
                      Color fill, Color outline) {
    if (note.type == editor_timeline_note_type::stay) {
        draw_stay_note_marker(info.head_rect, fill, outline);
    } else if (note.type == editor_timeline_note_type::release) {
        draw_release_note_marker(info.head_rect, fill, outline);
    } else {
        DrawRectangleRounded(info.head_rect, 0.3f, 6, fill);
        ui::draw_rect_lines(info.head_rect, 1.5f, outline);
    }
}

void draw_waveform(const editor_timeline_view_model& model, Rectangle content, const ui_theme& t) {
    if (!model.waveform_visible || model.waveform_summary == nullptr || model.timing_engine == nullptr) {
        return;
    }

    const int row_count = std::max(1, static_cast<int>(std::ceil(content.height)));
    std::vector<float> row_amplitudes(static_cast<std::size_t>(row_count), 0.0f);
    for (const audio_waveform_peak& peak : model.waveform_summary->peaks) {
        const double shifted_ms = peak.seconds * 1000.0 + static_cast<double>(model.waveform_offset_ms);
        const int tick = model.timing_engine->ms_to_tick(shifted_ms);
        if (tick < model.min_tick || tick > model.max_tick) {
            continue;
        }

        const float y = model.metrics.tick_to_y(tick);
        const int row = static_cast<int>(std::floor(y - content.y));
        if (row < 0 || row >= row_count) {
            continue;
        }
        row_amplitudes[static_cast<std::size_t>(row)] = std::max(
            row_amplitudes[static_cast<std::size_t>(row)],
            std::clamp(peak.amplitude, 0.0f, 1.0f));
    }

    const float center_x = content.x + content.width * 0.5f;
    const bool dark_theme = t.bg.r < 128;
    const unsigned char primary_alpha = dark_theme ? 70 : 22;
    const unsigned char secondary_alpha = dark_theme ? 26 : 8;
    for (int row = 0; row < row_count; ++row) {
        const float amplitude = row_amplitudes[static_cast<std::size_t>(row)];
        if (amplitude <= 0.001f) {
            continue;
        }

        const float y = content.y + static_cast<float>(row) + 0.5f;
        const float half_width = content.width * 0.5f * amplitude;
        ui::draw_line_f(center_x - half_width, y - 1.0f, center_x + half_width, y - 1.0f,
                        with_alpha(t.accent, secondary_alpha));
        ui::draw_line_f(center_x - half_width, y, center_x + half_width, y,
                        with_alpha(t.accent, primary_alpha));
        ui::draw_line_f(center_x - half_width, y + 1.0f, center_x + half_width, y + 1.0f,
                        with_alpha(t.accent, secondary_alpha));
    }
}

void draw_scroll_events(const editor_timeline_view_model& model, Rectangle content, const ui_theme& t) {
    for (size_t i = 0; i < model.scroll_events.size(); ++i) {
        const editor_timeline_scroll_event& event = model.scroll_events[i];
        if (event.duration <= 0 || event.tick > model.max_tick || event.tick + event.duration < model.min_tick) {
            continue;
        }

        const float start_y = model.metrics.tick_to_y(event.tick);
        const float end_y = model.metrics.tick_to_y(event.tick + event.duration);
        const float top = std::min(start_y, end_y);
        const float height = std::max(8.0f, std::fabs(end_y - start_y));
        const Rectangle band = {content.x, top, content.width, height};
        const bool selected = model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index == i;
        const Color base = event.type == scroll_event_type::speed
            ? Color{26, 188, 196, 255}
            : Color{238, 92, 64, 255};
        ui::draw_rect_f(band, with_alpha(base, selected ? 104 : 54));
        ui::draw_rect_lines(band, selected ? 2.5f : 1.2f, with_alpha(base, selected ? 235 : 150));
        ui::draw_rect_f({band.x, band.y - 3.0f, band.width, 6.0f}, with_alpha(base, selected ? 220 : 135));
        ui::draw_rect_f({band.x, band.y + band.height - 3.0f, band.width, 6.0f}, with_alpha(base, selected ? 220 : 135));

        if (event.type == scroll_event_type::stop) {
            for (float x = band.x - band.height; x < band.x + band.width; x += 18.0f) {
                DrawLineEx({x, band.y + band.height}, {x + band.height, band.y}, 2.0f,
                           with_alpha(base, selected ? 185 : 110));
            }
        }

        const char* label = event.type == scroll_event_type::speed
            ? TextFormat("%.2fx", event.multiplier)
            : "STOP";
        ui::draw_text_f(label, band.x + 10.0f, band.y + 4.0f, 15, selected ? t.text : t.text_secondary);
        ui::draw_text_f("Scroll Region", band.x + band.width - 112.0f, band.y + 4.0f, 13,
                        selected ? t.text : t.text_muted);
    }
}

void draw_ruler(const editor_timeline_view_model& model, Rectangle content, const ui_theme& t) {
    const Rectangle ruler = {content.x, content.y, content.width, 38.0f};
    ui::draw_rect_f(ruler, with_alpha(t.panel, 235));
    ui::draw_rect_lines(ruler, 1.5f, t.border_light);
    ui::draw_text_in_rect("Ruler", 13, {ruler.x + 10.0f, ruler.y + 4.0f, 54.0f, 16.0f},
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect("Measure", 13, {ruler.x + 10.0f, ruler.y + 20.0f, 72.0f, 16.0f},
                          t.text_secondary, ui::text_align::left);

    for (const editor_meter_map::grid_line& line : model.grid_lines) {
        if (!line.major) {
            continue;
        }
        const float y = model.metrics.tick_to_y(line.tick);
        if (y < content.y || y > content.y + content.height) {
            continue;
        }
        ui::draw_line_f(content.x, y, content.x + content.width, y, with_alpha(t.editor_grid_major, 210));
        const Rectangle tag = {content.x + 84.0f, y - 11.0f, 58.0f, 22.0f};
        ui::draw_rect_f(tag, with_alpha(t.section, 220));
        ui::draw_rect_lines(tag, 1.0f, with_alpha(t.border_light, 210));
        ui::draw_text_in_rect(TextFormat("%d", line.measure), 14, tag, t.text_secondary);
    }
}

void draw_timeline_header(const editor_timeline_view_model& model, const ui_theme& t) {
    const Rectangle panel = model.metrics.panel_rect;
    const Rectangle header = {panel.x + 10.0f, panel.y + 10.0f, panel.width - 20.0f, 48.0f};
    ui::draw_rect_f(header, with_alpha(t.panel, 245));
    ui::draw_rect_lines(header, 1.5f, t.border_light);
    ui::draw_text_in_rect("Timeline", 20,
                          {header.x + 14.0f, header.y + 5.0f, 140.0f, 22.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("Notes / loop region / scroll automation",
                          13,
                          {header.x + 14.0f, header.y + 27.0f, 260.0f, 16.0f},
                          t.text_muted, ui::text_align::left);

    const char* status = model.selected_note_indices.empty()
        ? (model.selected_note_index.has_value() ? "1 note selected" : "No selection")
        : TextFormat("%d notes selected", static_cast<int>(model.selected_note_indices.size()));
    const Rectangle selection_badge = {header.x + header.width - 384.0f, header.y + 9.0f, 142.0f, 30.0f};
    ui::draw_rect_f(selection_badge, with_alpha(t.row, 235));
    ui::draw_rect_lines(selection_badge, 1.0f, t.border_light);
    ui::draw_text_in_rect(status, 14, selection_badge,
                          model.selected_note_indices.empty() && !model.selected_note_index.has_value()
                              ? t.text_muted
                              : t.text);

    const Rectangle snap_badge = {selection_badge.x + selection_badge.width + 8.0f, selection_badge.y, 96.0f, 30.0f};
    ui::draw_rect_f(snap_badge, with_alpha(t.row, 235));
    ui::draw_rect_lines(snap_badge, 1.0f, t.border_light);
    ui::draw_text_in_rect(TextFormat("Snap %dt", model.snap_interval), 13, snap_badge, t.text_secondary);

    const Rectangle loop_badge = {snap_badge.x + snap_badge.width + 8.0f, snap_badge.y, 120.0f, 30.0f};
    ui::draw_rect_f(loop_badge, model.loop_enabled ? with_alpha(t.success, 120) : with_alpha(t.row, 235));
    ui::draw_rect_lines(loop_badge, 1.0f, model.loop_enabled ? t.success : t.border_light);
    ui::draw_text_in_rect(model.loop_enabled ? "Loop active" : "Loop off", 13,
                          loop_badge, model.loop_enabled ? t.text : t.text_muted);
}

void draw_lane_header(const editor_timeline_view_model& model, Rectangle content, const ui_theme& t) {
    const Rectangle lane_header = {content.x, content.y + 38.0f, content.width, 28.0f};
    ui::draw_rect_f(lane_header, with_alpha(t.section, 225));
    ui::draw_rect_lines(lane_header, 1.0f, t.border_light);
    for (int lane = 0; lane < std::max(1, model.metrics.key_count); ++lane) {
        const Rectangle lane_rect = model.metrics.lane_rect(lane);
        const Rectangle cell = {lane_rect.x, lane_header.y, lane_rect.width, lane_header.height};
        ui::draw_rect_f(cell, lane % 2 == 0 ? with_alpha(t.row, 58) : with_alpha(t.section, 58));
        ui::draw_rect_lines(cell, 1.0f, with_alpha(t.border_light, 170));
        ui::draw_text_in_rect(TextFormat("Lane %d", lane + 1), 13, cell, t.text_secondary);
    }
}
}

Rectangle editor_timeline_metrics::content_rect() const {
    return {
        panel_rect.x + padding + scrollbar_width + scrollbar_gap,
        panel_rect.y + padding,
        panel_rect.width - padding * 2.0f - scrollbar_gap - scrollbar_width,
        panel_rect.height - padding * 2.0f
    };
}

Rectangle editor_timeline_metrics::scrollbar_track_rect() const {
    return {
        panel_rect.x + padding,
        panel_rect.y + padding,
        scrollbar_width,
        panel_rect.height - padding * 2.0f
    };
}

float editor_timeline_metrics::visible_tick_span() const {
    return content_rect().height * ticks_per_pixel;
}

float editor_timeline_metrics::tick_to_y(int tick) const {
    const Rectangle content = content_rect();
    return content.y + content.height - (static_cast<float>(tick) - bottom_tick) / ticks_per_pixel;
}

int editor_timeline_metrics::y_to_tick(float y) const {
    const Rectangle content = content_rect();
    return static_cast<int>(std::lround(bottom_tick + (content.y + content.height - y) * ticks_per_pixel));
}

float editor_timeline_metrics::lane_width() const {
    const int clamped_key_count = std::max(1, key_count);
    const float content_width = content_rect().width;
    return (content_width - lane_gap * static_cast<float>(clamped_key_count - 1)) / static_cast<float>(clamped_key_count);
}

Rectangle editor_timeline_metrics::lane_rect(int lane) const {
    const Rectangle content = content_rect();
    const float width = lane_width();
    return {
        content.x + lane * (width + lane_gap),
        content.y,
        width,
        content.height
    };
}

editor_timeline_note_draw_info editor_timeline_metrics::note_rects(const editor_timeline_note& note) const {
    const int clamped_width = std::max(1, note.lane_width);
    const Rectangle first_lane = lane_rect(note.lane);
    const Rectangle last_lane = lane_rect(std::min(key_count - 1, note.lane + clamped_width - 1));
    const Rectangle lane = {
        std::min(first_lane.x, last_lane.x),
        first_lane.y,
        std::fabs((last_lane.x + last_lane.width) - first_lane.x),
        first_lane.height,
    };
    const float start_y = tick_to_y(note.tick);
    editor_timeline_note_draw_info info;
    const float single_lane_width = lane_width();
    const float note_body_width = std::max(single_lane_width * 0.2f, lane.width - single_lane_width * 0.08f);
    const float hold_body_width = std::max(single_lane_width * 0.18f, lane.width - single_lane_width * 0.20f);
    const float tap_height = std::clamp(28.0f * g_settings.note_height / std::max(0.001f, ticks_per_pixel),
                                        4.0f,
                                        72.0f);
    const float stay_width = std::max(single_lane_width * 0.54f, note_body_width * 1.04f);
    const float stay_height = std::max(3.0f, tap_height * (0.28f / 0.78f));

    const float head_width = note.type == editor_timeline_note_type::stay ? stay_width : note_body_width;
    const float head_height = note.type == editor_timeline_note_type::stay ? stay_height : tap_height;
    info.head_rect = {lane.x + (lane.width - head_width) * 0.5f,
                      start_y - head_height * 0.5f,
                      head_width,
                      head_height};
    const float handle_width = std::min(9.0f, std::max(5.0f, lane.width * 0.18f));
    const float handle_height = head_height + 8.0f;
    info.left_resize_rect = {info.head_rect.x - handle_width * 0.5f,
                             start_y - handle_height * 0.5f,
                             handle_width,
                             handle_height};
    info.right_resize_rect = {info.head_rect.x + info.head_rect.width - handle_width * 0.5f,
                              start_y - handle_height * 0.5f,
                              handle_width,
                              handle_height};

    if (note.type == editor_timeline_note_type::hold) {
        const float end_y = tick_to_y(note.end_tick);
        const float top = std::min(start_y, end_y);
        const float height = std::fabs(end_y - start_y);
        info.body_rect = {lane.x + (lane.width - hold_body_width) * 0.5f,
                          top,
                          hold_body_width,
                          std::max(height, 6.0f)};
        info.tail_rect = {lane.x + (lane.width - note_body_width) * 0.5f,
                          end_y - tap_height * 0.5f,
                          note_body_width,
                          tap_height};
        info.start_resize_rect = {lane.x + (lane.width - note_body_width) * 0.5f,
                                  start_y - tap_height * 0.5f,
                                  note_body_width,
                                  tap_height};
        info.end_resize_rect = {lane.x + (lane.width - note_body_width) * 0.5f,
                                end_y - tap_height * 0.5f,
                                note_body_width,
                                tap_height};
        info.has_body = true;
    }

    return info;
}

void editor_timeline_view::draw(const editor_timeline_view_model& model) {
    const auto& t = *g_theme;
    const Rectangle content = model.metrics.content_rect();
    const Rectangle track = model.metrics.scrollbar_track_rect();

    ui::draw_rect_f(ui::inset(model.metrics.panel_rect, 10.0f), t.section);
    draw_timeline_header(model, t);
    {
        ui::scoped_clip_rect clip_scope(content);
        const std::set<size_t> selected_note_indices(
            model.selected_note_indices.begin(), model.selected_note_indices.end());

        draw_waveform(model, content, t);
        if (model.loop_end_tick > model.loop_start_tick &&
            model.loop_end_tick >= model.min_tick && model.loop_start_tick <= model.max_tick) {
            const float start_y = model.metrics.tick_to_y(model.loop_start_tick);
            const float end_y = model.metrics.tick_to_y(model.loop_end_tick);
            const float top = std::min(start_y, end_y);
            const float height = std::max(8.0f, std::fabs(end_y - start_y));
            const Rectangle band = {content.x, top, content.width, height};
            ui::draw_rect_f(band, with_alpha(t.success, model.loop_enabled ? 38 : 18));
            ui::draw_rect_lines(band, model.loop_enabled ? 2.0f : 1.0f,
                                with_alpha(t.success, model.loop_enabled ? 210 : 120));
            ui::draw_rect_f({band.x, start_y - 4.0f, band.width, 8.0f},
                            with_alpha(t.success, model.loop_enabled ? 230 : 130));
            ui::draw_rect_f({band.x, end_y - 4.0f, band.width, 8.0f},
                            with_alpha(t.success, model.loop_enabled ? 230 : 130));
            ui::draw_text_f(model.loop_enabled ? "LOOP" : "loop", band.x + band.width - 58.0f, band.y + 5.0f,
                            14, model.loop_enabled ? t.text : t.text_secondary);
        }
        draw_scroll_events(model, content, t);
        draw_lane_header(model, content, t);

        for (int lane = 0; lane < std::max(1, model.metrics.key_count); ++lane) {
            const Rectangle rect = model.metrics.lane_rect(lane);
            ui::draw_rect_f(rect, lane % 2 == 0 ? with_alpha(t.row, 20) : with_alpha(t.section, 20));
            ui::draw_rect_lines(rect, 1.0f, with_alpha(t.border_light, 180));
            ui::draw_text_in_rect(TextFormat("L%d", lane + 1), 16,
                                  {rect.x, model.metrics.panel_rect.y + 4.0f, rect.width, 20.0f},
                                  t.text_hint);
        }

        const int first_snap_tick = std::max(0, (model.min_tick / model.snap_interval) * model.snap_interval);
        for (int tick = first_snap_tick; tick <= model.max_tick; tick += model.snap_interval) {
            const float y = model.metrics.tick_to_y(tick);
            ui::draw_line_f(content.x, y, content.x + content.width, y, t.editor_grid_snap);
        }

        for (const editor_meter_map::grid_line& line : model.grid_lines) {
            const float y = model.metrics.tick_to_y(line.tick);
            const Color color = line.major ? t.editor_grid_major : t.editor_grid_minor;
            ui::draw_line_f(content.x, y, content.x + content.width, y, color);
            if (line.major) {
                ui::draw_line_f(content.x, y + 1.0f, content.x + content.width, y + 1.0f, t.editor_grid_major_glow);
            }
            ui::draw_text_f(TextFormat("%d:%d", line.measure, line.beat), content.x + 8.0f, y - 10.0f,
                            line.major ? 16 : 14, line.major ? t.text : t.text_secondary);
        }

        for (size_t i = 0; i < model.notes.size(); ++i) {
            if (model.preview_note_index.has_value() && *model.preview_note_index == i) {
                continue;
            }
            const editor_timeline_note& note = model.notes[i];
            if (note.lane < 0 || note.lane >= model.metrics.key_count) {
                continue;
            }

            const editor_timeline_note_draw_info info = model.metrics.note_rects(note);
            const bool selected = selected_note_indices.find(i) != selected_note_indices.end() ||
                                  (model.selected_note_index.has_value() && *model.selected_note_index == i);
            Color head_fill = selected ? t.row_active : t.note_color;
            if (!selected && note.type == editor_timeline_note_type::release) {
                head_fill = lerp_color(t.note_color, t.judge_great, 0.35f);
            } else if (!selected && note.type == editor_timeline_note_type::stay) {
                head_fill = lerp_color(t.note_color, t.judge_perfect, 0.25f);
            }
            if (note.is_ray) {
                head_fill = lerp_color(head_fill, {180, 132, 255, 255}, 0.58f);
            }
            const Color outline = selected ? t.border_active : t.note_outline;
            const Color hold_fill = selected ? with_alpha(t.row_active, 200) : with_alpha(t.accent, 170);

            if (info.has_body) {
                DrawRectangleRounded(info.body_rect, 0.4f, 6, hold_fill);
            } else {
                draw_note_marker(note, info, head_fill, outline);
            }

            if (selected) {
                if (!info.has_body) {
                    DrawRectangleRounded(info.left_resize_rect, 0.45f, 4, with_alpha(t.border_active, 210));
                    DrawRectangleRounded(info.right_resize_rect, 0.45f, 4, with_alpha(t.border_active, 210));
                    ui::draw_rect_lines(info.left_resize_rect, 1.0f, t.text);
                    ui::draw_rect_lines(info.right_resize_rect, 1.0f, t.text);
                }
            }
        }

        if (model.preview_note.has_value()) {
            const editor_timeline_note_draw_info info = model.metrics.note_rects(*model.preview_note);
            const Color fill = model.preview_has_overlap ? with_alpha(t.error, 150) : with_alpha(t.success, 150);
            const Color outline = model.preview_has_overlap ? t.error : t.success;
            if (info.has_body) {
                DrawRectangleRounded(info.body_rect, 0.4f, 6, fill);
            } else {
                draw_note_marker(*model.preview_note, info, fill, outline);
            }
        }

        if (model.playback_tick.has_value()) {
            const float y = model.metrics.tick_to_y(*model.playback_tick);
            ui::draw_line_f(content.x, y, content.x + content.width, y, with_alpha(t.accent, 240));
            ui::draw_line_f(content.x, y + 1.0f, content.x + content.width, y + 1.0f, with_alpha(t.text, 170));
        }

        draw_ruler(model, content, t);
    }

    ui::draw_scrollbar(track, model.content_height_pixels, model.scroll_offset_pixels,
                       t.scrollbar_track, t.scrollbar_thumb, 40.0f);
    ui::draw_rect_lines(model.metrics.panel_rect, 2.0f, t.border);
}
