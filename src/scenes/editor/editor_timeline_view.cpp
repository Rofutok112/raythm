#include "editor_timeline_view.h"

#include <algorithm>
#include <cmath>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

Rectangle editor_timeline_metrics::content_rect() const {
    return {
        panel_rect.x + padding,
        panel_rect.y + padding,
        panel_rect.width - padding * 2.0f - scrollbar_gap - scrollbar_width,
        panel_rect.height - padding * 2.0f
    };
}

Rectangle editor_timeline_metrics::scrollbar_track_rect() const {
    const Rectangle content = content_rect();
    return {
        content.x + content.width + scrollbar_gap,
        content.y,
        scrollbar_width,
        content.height
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
    const Rectangle lane = lane_rect(note.lane);
    const float start_y = tick_to_y(note.tick);
    editor_timeline_note_draw_info info;
    info.head_rect = {lane.x + 6.0f, start_y - note_head_height * 0.5f, lane.width - 12.0f, note_head_height};

    if (note.type == editor_timeline_note_type::hold) {
        const float end_y = tick_to_y(note.end_tick);
        const float top = std::min(start_y, end_y);
        const float height = std::fabs(end_y - start_y);
        info.body_rect = {lane.x + lane.width * 0.3f, top, lane.width * 0.4f, std::max(height, 6.0f)};
        info.tail_rect = {lane.x + 10.0f, end_y - 5.0f, lane.width - 20.0f, 10.0f};
        info.has_body = true;
    }

    return info;
}

void editor_timeline_view::draw(const editor_timeline_view_model& model) {
    const auto& t = *g_theme;
    const Rectangle content = model.metrics.content_rect();
    const Rectangle track = model.metrics.scrollbar_track_rect();

    DrawRectangleRec(ui::inset(model.metrics.panel_rect, 10.0f), t.section);
    {
        ui::scoped_clip_rect clip_scope(content);

        if (model.waveform_visible && model.waveform_summary != nullptr && model.timing_engine != nullptr) {
            const float center_x = content.x + content.width * 0.5f;
            const bool dark_theme = t.bg.r < 128;
            const unsigned char primary_alpha = dark_theme ? 84 : 24;
            const unsigned char secondary_alpha = dark_theme ? 38 : 10;
            for (const audio_waveform_peak& peak : model.waveform_summary->peaks) {
                const double shifted_ms =
                    peak.seconds * 1000.0 + static_cast<double>(model.waveform_offset_ms);
                const int tick = model.timing_engine->ms_to_tick(shifted_ms);
                if (tick < model.min_tick || tick > model.max_tick) {
                    continue;
                }

                const float y = model.metrics.tick_to_y(tick);
                const float half_width = content.width * 0.5f * std::clamp(peak.amplitude, 0.0f, 1.0f);
                ui::draw_line_f(center_x - half_width, y - 1.0f, center_x + half_width, y - 1.0f, with_alpha(t.accent, secondary_alpha));
                ui::draw_line_f(center_x - half_width, y, center_x + half_width, y, with_alpha(t.accent, primary_alpha));
                ui::draw_line_f(center_x - half_width, y + 1.0f, center_x + half_width, y + 1.0f, with_alpha(t.accent, secondary_alpha));
            }
        }

        for (int lane = 0; lane < std::max(1, model.metrics.key_count); ++lane) {
            const Rectangle rect = model.metrics.lane_rect(lane);
            DrawRectangleRec(rect, lane % 2 == 0 ? with_alpha(t.row, 20) : with_alpha(t.section, 20));
            DrawRectangleLinesEx(rect, 1.0f, with_alpha(t.border_light, 180));
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
            const editor_timeline_note& note = model.notes[i];
            if (note.lane < 0 || note.lane >= model.metrics.key_count) {
                continue;
            }

            const editor_timeline_note_draw_info info = model.metrics.note_rects(note);
            const bool selected = model.selected_note_index.has_value() && *model.selected_note_index == i;
            const Color head_fill = selected ? t.row_active : t.note_color;
            const Color outline = selected ? t.border_active : t.note_outline;
            const Color hold_fill = selected ? with_alpha(t.row_active, 200) : with_alpha(t.accent, 170);

            if (info.has_body) {
                DrawRectangleRounded(info.body_rect, 0.4f, 6, hold_fill);
                DrawRectangleRounded(info.tail_rect, 0.4f, 6, selected ? with_alpha(t.row_active, 230) : with_alpha(t.accent, 220));
                DrawRectangleLinesEx(info.tail_rect, 1.5f, outline);
            }

            DrawRectangleRounded(info.head_rect, 0.3f, 6, head_fill);
            DrawRectangleLinesEx(info.head_rect, 1.5f, outline);
        }

        if (model.preview_note.has_value()) {
            const editor_timeline_note_draw_info info = model.metrics.note_rects(*model.preview_note);
            const Color fill = model.preview_has_overlap ? with_alpha(t.error, 150) : with_alpha(t.success, 150);
            const Color outline = model.preview_has_overlap ? t.error : t.success;
            if (info.has_body) {
                DrawRectangleRounded(info.body_rect, 0.4f, 6, fill);
                DrawRectangleRounded(info.tail_rect, 0.4f, 6, fill);
                DrawRectangleLinesEx(info.tail_rect, 1.5f, outline);
            }
            DrawRectangleRounded(info.head_rect, 0.3f, 6, fill);
            DrawRectangleLinesEx(info.head_rect, 1.5f, outline);
        }

        if (model.playback_tick.has_value()) {
            const float y = model.metrics.tick_to_y(*model.playback_tick);
            ui::draw_line_f(content.x, y, content.x + content.width, y, with_alpha(t.accent, 240));
            ui::draw_line_f(content.x, y + 1.0f, content.x + content.width, y + 1.0f, with_alpha(t.text, 170));
        }
    }

    ui::draw_scrollbar(track, model.content_height_pixels, model.scroll_offset_pixels,
                       t.scrollbar_track, t.scrollbar_thumb, 40.0f);
    DrawRectangleLinesEx(model.metrics.panel_rect, 2.0f, t.border);
}
