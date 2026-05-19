#include "editor/daw/editor_daw_view.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "editor/editor_timeline_view.h"
#include "editor/view/editor_layout.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui/icons/raythm_icons.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;

constexpr float kRailWidth = 54.0f;
constexpr float kPanelInset = 14.0f;

bool accepts_metadata_character(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint <= 126;
}

const char* key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

const char* palette_label(note_type type) {
    switch (type) {
        case note_type::tap:
            return "TAP";
        case note_type::hold:
            return "LONG";
        case note_type::release:
            return "RELEASE";
        case note_type::stay:
            return "STAY";
    }
    return "TAP";
}

const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Meter";
}

const char* scroll_event_type_label(scroll_event_type type) {
    return type == scroll_event_type::speed ? "Speed" : "Stop";
}

Color panel_tint(Color base, Color tone, float amount) {
    return with_alpha(lerp_color(base, tone, amount), base.a);
}

Rectangle inset_rect(Rectangle rect, float value) {
    return ui::inset(rect, ui::edge_insets::uniform(value));
}

Rectangle row(Rectangle rect, float y, float height) {
    return {rect.x, rect.y + y, rect.width, height};
}

Rectangle centered_icon_rect(Rectangle rect, float inset) {
    return {rect.x + inset, rect.y + inset, rect.width - inset * 2.0f, rect.height - inset * 2.0f};
}

ui::button_state draw_icon_button(Rectangle rect,
                                  void (*draw_icon)(Rectangle, Color, float),
                                  bool active,
                                  Color active_color) {
    const auto& t = *g_theme;
    const ui::button_state button = ui::draw_button_colored(
        rect, "", 16,
        active ? panel_tint(t.row_selected, active_color, 0.14f) : t.row,
        active ? panel_tint(t.row_active, active_color, 0.18f) : t.row_hover,
        active ? t.text : t.text_secondary,
        active ? 2.2f : 1.2f);
    draw_icon(centered_icon_rect(rect, 12.0f), active ? active_color : t.text_secondary, 2.8f);
    return button;
}

void draw_micro_label(Rectangle rect, const char* label, Color color) {
    ui::draw_text_in_rect(label, 12, rect, color, ui::text_align::left);
}

void draw_badge(Rectangle rect, const char* label, Color border, Color text) {
    ui::draw_rect_f(rect, with_alpha(border, 28));
    ui::draw_rect_lines(rect, 1.0f, with_alpha(border, 190));
    ui::draw_text_in_rect(label, 13, rect, text);
}

void draw_rail_icon(Rectangle rect,
                    void (*draw_icon)(Rectangle, Color, float),
                    const char* label,
                    bool selected) {
    const auto& t = *g_theme;
    const ui::row_state state = ui::draw_row(
        rect,
        selected ? panel_tint(t.row_selected, t.accent, 0.14f) : with_alpha(t.row, 210),
        selected ? panel_tint(t.row_active, t.accent, 0.18f) : t.row_hover,
        selected ? t.accent : t.border_light,
        selected ? 2.0f : 1.0f);
    draw_icon(centered_icon_rect({state.visual.x + 7.0f, state.visual.y + 4.0f, 28.0f, 28.0f}, 4.0f),
              selected ? t.accent : t.text_secondary, 2.5f);
    ui::draw_text_in_rect(label, 9,
                          {state.visual.x, state.visual.y + state.visual.height - 14.0f,
                           state.visual.width, 12.0f},
                          selected ? t.text : t.text_muted);
}

void draw_palette_pad(Rectangle rect,
                      note_type type,
                      const editor_note_palette_selection& selection,
                      editor_left_panel_view_result& result) {
    const auto& t = *g_theme;
    const bool selected = !selection.is_ray && selection.type == type;
    const Color tone = type == note_type::hold ? t.success :
        (type == note_type::release ? t.slow :
         (type == note_type::stay ? t.fast : t.accent));
    const ui::row_state state = ui::draw_row(
        rect,
        selected ? panel_tint(t.row_selected, tone, 0.18f) : t.row,
        selected ? panel_tint(t.row_active, tone, 0.2f) : t.row_hover,
        selected ? tone : t.border_light,
        selected ? 2.0f : 1.0f);
    ui::draw_rect_f({state.visual.x + 10.0f, state.visual.y + state.visual.height - 7.0f,
                     state.visual.width - 20.0f, 3.0f},
                    selected ? tone : with_alpha(t.text_muted, 95));
    ui::draw_text_in_rect(palette_label(type), 14,
                          {state.visual.x + 10.0f, state.visual.y + 7.0f,
                           state.visual.width - 20.0f, 19.0f},
                          selected ? t.text : t.text_secondary, ui::text_align::left);
    if (state.clicked) {
        result.selected_note_type = type;
    }
}

editor_timeline_note make_timeline_note(const note_data& note) {
    editor_timeline_note_type type = editor_timeline_note_type::tap;
    switch (note.type) {
        case note_type::tap:
            type = editor_timeline_note_type::tap;
            break;
        case note_type::hold:
            type = editor_timeline_note_type::hold;
            break;
        case note_type::release:
            type = editor_timeline_note_type::release;
            break;
        case note_type::stay:
            type = editor_timeline_note_type::stay;
            break;
    }
    return {type, note.tick, note.lane, note.end_tick, note.is_ray, note_lane_width(note)};
}

editor_timeline_view_model make_timeline_model(const editor_timeline_presenter_model& model) {
    const editor_timeline_metrics metrics = editor_timeline_viewport::metrics(model.viewport);
    const float visible_tick_span = editor_timeline_viewport::visible_tick_span(model.viewport);
    const int min_tick = static_cast<int>(std::floor(model.viewport.viewport.bottom_tick - visible_tick_span * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(model.viewport.viewport.bottom_tick + visible_tick_span));

    std::vector<editor_timeline_note> notes;
    notes.reserve(model.state.data().notes.size());
    for (const note_data& note : model.state.data().notes) {
        notes.push_back(make_timeline_note(note));
    }

    std::vector<editor_timeline_scroll_event> scroll_events;
    scroll_events.reserve(model.state.data().scroll_events.size());
    for (const scroll_event& event : model.state.data().scroll_events) {
        scroll_events.push_back({event.type, event.tick, event.duration, event.multiplier});
    }

    std::optional<editor_timeline_note> preview_note;
    if (model.preview_note.has_value()) {
        preview_note = make_timeline_note(*model.preview_note);
    }

    return {
        metrics,
        model.meter_map.visible_grid_lines(min_tick, max_tick),
        std::move(scroll_events),
        std::move(notes),
        model.selected_note_index,
        model.selected_note_indices,
        model.selected_scroll_event_index,
        model.audio_loaded ? std::optional<int>(model.playback_tick) : std::nullopt,
        model.loop_enabled,
        model.loop_start_tick,
        model.loop_end_tick,
        model.waveform_summary,
        &model.state.engine(),
        model.waveform_visible,
        model.waveform_offset_ms,
        preview_note,
        model.preview_has_overlap,
        min_tick,
        max_tick,
        editor_timeline_viewport::snap_interval(model.viewport),
        editor_timeline_viewport::content_height_pixels(model.viewport),
        editor_timeline_viewport::scroll_offset_pixels(model.viewport)
    };
}

void draw_waveform(const editor_timeline_view_model& model, Rectangle content) {
    const auto& t = *g_theme;
    if (!model.waveform_visible || model.waveform_summary == nullptr || model.timing_engine == nullptr) {
        return;
    }

    const int row_count = std::max(1, static_cast<int>(std::ceil(content.height)));
    std::vector<float> rows(static_cast<size_t>(row_count), 0.0f);
    for (const audio_waveform_peak& peak : model.waveform_summary->peaks) {
        const double shifted_ms = peak.seconds * 1000.0 + static_cast<double>(model.waveform_offset_ms);
        const int tick = model.timing_engine->ms_to_tick(shifted_ms);
        if (tick < model.min_tick || tick > model.max_tick) {
            continue;
        }
        const int row_index = static_cast<int>(std::floor(model.metrics.tick_to_y(tick) - content.y));
        if (row_index >= 0 && row_index < row_count) {
            rows[static_cast<size_t>(row_index)] =
                std::max(rows[static_cast<size_t>(row_index)], std::clamp(peak.amplitude, 0.0f, 1.0f));
        }
    }

    const float center_x = content.x + content.width * 0.5f;
    for (int i = 0; i < row_count; ++i) {
        const float amplitude = rows[static_cast<size_t>(i)];
        if (amplitude <= 0.001f) {
            continue;
        }
        const float y = content.y + static_cast<float>(i);
        const float half_width = content.width * 0.48f * amplitude;
        ui::draw_line_f(center_x - half_width, y, center_x + half_width, y, with_alpha(t.accent, 58));
    }
}

void draw_note_block(const editor_timeline_note& note,
                     const editor_timeline_note_draw_info& info,
                     bool selected,
                     bool preview,
                     bool overlap) {
    const auto& t = *g_theme;
    const Color tone = overlap ? t.error :
        (note.type == editor_timeline_note_type::hold ? t.success :
         (note.type == editor_timeline_note_type::release ? t.slow :
          (note.type == editor_timeline_note_type::stay ? t.fast : t.accent)));
    const Color fill = selected ? with_alpha(tone, 235) : with_alpha(tone, preview ? 145 : 195);
    const Color outline = selected ? t.text : with_alpha(tone, 245);

    if (info.has_body) {
        ui::draw_rect_f(info.body_rect, with_alpha(tone, selected ? 135 : 80));
        ui::draw_rect_lines(info.body_rect, selected ? 2.0f : 1.0f, with_alpha(outline, 205));
        ui::draw_rect_f(info.tail_rect, fill);
        ui::draw_rect_lines(info.tail_rect, selected ? 2.0f : 1.0f, outline);
    }

    ui::draw_rect_f(info.head_rect, fill);
    ui::draw_rect_lines(info.head_rect, selected ? 2.4f : 1.2f, outline);
    if (note.is_ray) {
        ui::draw_rect_f({info.head_rect.x, info.head_rect.y - 5.0f, info.head_rect.width, 3.0f},
                        selected ? t.text : with_alpha(t.text_secondary, 180));
    }
}

}  // namespace

namespace editor::daw {

editor_left_panel_view_result draw_left_panel(const editor_left_panel_view_model& model) {
    const auto& t = *g_theme;
    editor_left_panel_view_result result;
    metadata_panel_state& metadata_panel = *model.metadata_panel;
    const Rectangle panel = layout::kLeftPanelRect;
    const Rectangle rail = {panel.x, panel.y, kRailWidth, panel.height};
    const Rectangle content = {panel.x + kRailWidth + 10.0f, panel.y + 14.0f,
                               panel.width - kRailWidth - 24.0f, panel.height - 28.0f};
    const char* status_label = model.is_dirty ? "MODIFIED" : (model.has_file ? "SAVED" : "UNSAVED");

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.18f));
    ui::draw_rect_lines(panel, 1.5f, t.border);
    ui::draw_rect_f(rail, panel_tint(t.section, t.accent, 0.08f));
    ui::draw_rect_f({rail.x + rail.width - 2.0f, rail.y, 2.0f, rail.height}, t.border_light);

    draw_rail_icon({rail.x + 8.0f, rail.y + 16.0f, 38.0f, 50.0f},
                   raythm_icons::draw_mouse_pointer, "SEL", true);
    draw_rail_icon({rail.x + 8.0f, rail.y + 74.0f, 38.0f, 50.0f},
                   raythm_icons::draw_scan, "RNG", false);
    draw_rail_icon({rail.x + 8.0f, rail.y + 132.0f, 38.0f, 50.0f},
                   raythm_icons::draw_layers, "NT", false);
    draw_rail_icon({rail.x + 8.0f, rail.y + 190.0f, 38.0f, 50.0f},
                   raythm_icons::draw_repeat_2, "LP", false);

    ui::draw_text_in_rect("TRACK", 15, row(content, 0.0f, 20.0f), t.text_muted, ui::text_align::left);
    draw_marquee_text(model.song_title, content.x, content.y + 26.0f, 24, t.text, content.width, model.now);
    draw_badge({content.x, content.y + 62.0f, 95.0f, 24.0f}, status_label,
               model.is_dirty ? t.slow : t.success, model.is_dirty ? t.slow : t.success);

    const Rectangle meta = {content.x, content.y + 104.0f, content.width, 210.0f};
    ui::draw_section(meta);
    ui::draw_text_in_rect("Chart Setup", 20, {meta.x + 12.0f, meta.y + 10.0f, meta.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    result.difficulty_result = ui::draw_text_input(
        {meta.x + 12.0f, meta.y + 48.0f, meta.width - 24.0f, 34.0f},
        metadata_panel.difficulty_input, "Diff", "Difficulty", "New",
        ui::draw_layer::base, 16, 24, accepts_metadata_character, 58.0f);
    result.author_result = ui::draw_text_input(
        {meta.x + 12.0f, meta.y + 90.0f, meta.width - 24.0f, 34.0f},
        metadata_panel.chart_author_input, "Author", "Chart author", "Unknown",
        ui::draw_layer::base, 16, 32, accepts_metadata_character, 58.0f);
    const ui::selector_state key_count_selector = ui::draw_value_selector(
        {meta.x + 12.0f, meta.y + 132.0f, meta.width - 24.0f, 34.0f},
        "Lanes", key_count_label(metadata_panel.key_count), 16, 26.0f, 58.0f, 12.0f);
    result.key_count_left_clicked = key_count_selector.left.clicked;
    result.key_count_right_clicked = key_count_selector.right.clicked;
    if (!metadata_panel.error.empty()) {
        ui::draw_text_in_rect(metadata_panel.error.c_str(), 13,
                              {meta.x + 12.0f, meta.y + 174.0f, meta.width - 24.0f, 22.0f},
                              t.error, ui::text_align::left);
    }

    const Rectangle palette = {content.x, meta.y + meta.height + 14.0f, content.width, 196.0f};
    ui::draw_section(palette);
    ui::draw_text_in_rect("Note Palette", 20,
                          {palette.x + 12.0f, palette.y + 10.0f, palette.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    const float gap = 8.0f;
    const float pad_width = (palette.width - 32.0f) * 0.5f;
    const float pad_height = 48.0f;
    draw_palette_pad({palette.x + 12.0f, palette.y + 48.0f, pad_width, pad_height},
                     note_type::tap, model.note_palette, result);
    draw_palette_pad({palette.x + 20.0f + pad_width, palette.y + 48.0f, pad_width, pad_height},
                     note_type::hold, model.note_palette, result);
    draw_palette_pad({palette.x + 12.0f, palette.y + 48.0f + pad_height + gap, pad_width, pad_height},
                     note_type::release, model.note_palette, result);
    draw_palette_pad({palette.x + 20.0f + pad_width, palette.y + 48.0f + pad_height + gap, pad_width, pad_height},
                     note_type::stay, model.note_palette, result);

    const ui::button_state ray_button = ui::draw_button_colored(
        {palette.x + 12.0f, palette.y + 48.0f + (pad_height + gap) * 2.0f,
         palette.width - 24.0f, 32.0f},
        model.note_palette.is_ray ? "RAY LANE ARMED" : "RAY LANE",
        14,
        model.note_palette.is_ray ? panel_tint(t.row_selected, t.fast, 0.18f) : t.row,
        model.note_palette.is_ray ? panel_tint(t.row_active, t.fast, 0.18f) : t.row_hover,
        model.note_palette.is_ray ? t.text : t.text_secondary,
        model.note_palette.is_ray ? 2.0f : 1.0f);
    result.ray_toggled = ray_button.clicked;

    const Rectangle ops = {content.x, palette.y + palette.height + 14.0f, content.width, 144.0f};
    ui::draw_section(ops);
    ui::draw_text_in_rect("Session Ops", 20, {ops.x + 12.0f, ops.y + 10.0f, ops.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    ui::draw_label_value({ops.x + 12.0f, ops.y + 50.0f, ops.width - 24.0f, 22.0f},
                         "Edit Target", model.note_palette.is_ray ? "Ray lane" : palette_label(model.note_palette.type),
                         14, t.text_muted, t.text_secondary, 78.0f);
    ui::draw_label_value({ops.x + 12.0f, ops.y + 82.0f, ops.width - 24.0f, 22.0f},
                         "Snap", "Header control", 14, t.text_muted, t.text_secondary, 78.0f);
    ui::draw_label_value({ops.x + 12.0f, ops.y + 114.0f, ops.width - 24.0f, 22.0f},
                         "Loop", "[ ] then L", 14, t.text_muted, t.text_secondary, 78.0f);

    if (model.load_error != nullptr) {
        ui::draw_text_in_rect(model.load_error->c_str(), 16,
                              {content.x, content.y + content.height - 58.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }

    return result;
}

editor_right_panel_view_result draw_right_panel(const editor_right_panel_view_model& model,
                                                editor_timing_panel_state& timing_state) {
    const auto& t = *g_theme;
    editor_right_panel_view_result result;
    const Rectangle panel = layout::kRightPanelRect;
    const Rectangle content = inset_rect(panel, kPanelInset);

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.14f));
    ui::draw_rect_lines(panel, 1.5f, t.border);
    ui::draw_text_in_rect("INSPECTOR", 15, {content.x, content.y, content.width, 20.0f},
                          t.text_muted, ui::text_align::left);
    draw_badge({content.x + content.width - 122.0f, content.y - 2.0f, 122.0f, 24.0f},
               model.selected_note_count > 0 ? "NOTES" : "TIMING",
               model.selected_note_count > 0 ? t.accent : t.text_muted,
               model.selected_note_count > 0 ? t.accent : t.text_secondary);

    std::vector<editor_timing_panel_item> items;
    items.reserve(model.timing_events->size());
    for (size_t index = 0; index < model.timing_events->size(); ++index) {
        const timing_event& event = (*model.timing_events)[index];
        items.push_back({
            index,
            std::string(timing_event_type_label(event.type)) + " " + model.meter_map->bar_beat_label(event.tick),
            event.type == timing_event_type::bpm
                ? TextFormat("%.1f", event.bpm)
                : TextFormat("%d/%d", event.numerator, event.denominator),
            model.selected_event_index.has_value() && *model.selected_event_index == index
        });
    }

    std::vector<editor_timing_panel_item> scroll_items;
    scroll_items.reserve(model.scroll_events->size());
    for (size_t index = 0; index < model.scroll_events->size(); ++index) {
        const scroll_event& event = (*model.scroll_events)[index];
        scroll_items.push_back({
            index,
            std::string("Scroll ") + model.meter_map->bar_beat_label(event.tick),
            event.type == scroll_event_type::speed
                ? TextFormat("%s %.2fx / %dt", scroll_event_type_label(event.type), event.multiplier, event.duration)
                : TextFormat("%s / %dt", scroll_event_type_label(event.type), event.duration),
            model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index == index
        });
    }

    std::optional<timing_event> selected_event;
    if (model.selected_event_index.has_value() && *model.selected_event_index < model.timing_events->size()) {
        selected_event = (*model.timing_events)[*model.selected_event_index];
    }
    std::optional<scroll_event> selected_scroll_event;
    if (model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index < model.scroll_events->size()) {
        selected_scroll_event = (*model.scroll_events)[*model.selected_scroll_event_index];
    }

    const Rectangle timing_content = {content.x, content.y + 38.0f, content.width, content.height - 38.0f};
    result.panel_result = editor_timing_panel::draw(
        {timing_content, model.mouse, std::move(items), std::move(scroll_items),
         selected_event, selected_scroll_event, model.selected_note_count, model.selected_note_summary,
         model.delete_enabled, model.scroll_delete_enabled},
        timing_state);

    const Rectangle editor_box = {timing_content.x, timing_content.y + 660.0f,
                                  timing_content.width, timing_content.height - 660.0f};
    result.clicked_outside_editor = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                    !CheckCollisionPointRec(model.mouse, editor_box);
    return result;
}

editor_header_view_result draw_header(const editor_header_view_model& model, Rectangle snap_menu_rect) {
    const auto& t = *g_theme;
    editor_header_view_result result;
    const Rectangle bar = layout::kHeaderRect;
    const Rectangle content = inset_rect(bar, 10.0f);

    ui::draw_rect_f(bar, panel_tint(t.panel, t.bg_alt, 0.18f));
    ui::draw_rect_lines(bar, 1.5f, t.border);
    ui::draw_button_colored(layout::kBackButtonRect, "BACK", 18, t.row, t.row_hover, t.text);
    ui::draw_button_colored(layout::kSettingsButtonRect, "SETTINGS", 16, t.row, t.row_hover, t.text);

    ui::draw_text_in_rect("raythm", 20, {content.x + 420.0f, content.y + 3.0f, 100.0f, 22.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("DAW Chart Editor", 13, {content.x + 420.0f, content.y + 28.0f, 150.0f, 18.0f},
                          t.accent, ui::text_align::left);

    const Rectangle transport = {content.x + 608.0f, content.y + 1.0f, 430.0f, 50.0f};
    ui::draw_section(transport);
    const Rectangle play_rect = {transport.x + 10.0f, transport.y + 5.0f, 40.0f, 40.0f};
    const ui::button_state play_button = model.audio_playing
        ? draw_icon_button(play_rect, raythm_icons::draw_pause, true, t.accent)
        : draw_icon_button(play_rect, raythm_icons::draw_play, false, t.text);
    result.playback_toggled = play_button.clicked;
    ui::draw_label_value({transport.x + 62.0f, transport.y + 5.0f, 168.0f, 40.0f},
                         "Now", model.playback_status, 14, t.text_muted,
                         model.audio_loaded ? t.text : t.text_muted, 46.0f);
    const Rectangle loop_button_rect = {transport.x + 246.0f, transport.y + 5.0f, 40.0f, 40.0f};
    const ui::button_state loop_button = draw_icon_button(loop_button_rect, raythm_icons::draw_repeat_2,
                                                          model.loop_enabled, t.success);
    result.loop_toggled = loop_button.clicked;
    ui::draw_label_value({transport.x + 296.0f, transport.y + 5.0f, 124.0f, 40.0f},
                         "Loop", model.loop_label, 14,
                         model.loop_enabled ? t.success : t.text_muted,
                         model.loop_enabled ? t.text : t.text_secondary, 42.0f);

    const ui::selector_state chart_offset = ui::draw_value_selector(
        layout::kChartOffsetRect, "Offset", model.offset_label,
        14, 24.0f, 50.0f, 10.0f);
    result.offset_left_clicked = chart_offset.left.clicked;
    result.offset_right_clicked = chart_offset.right.clicked;

    const ui::button_state waveform_toggle = ui::draw_button_colored(
        layout::kWaveformToggleRect, model.waveform_visible ? "WAVE ON" : "WAVE OFF", 14,
        model.waveform_visible ? panel_tint(t.row_selected, t.fast, 0.15f) : t.row,
        model.waveform_visible ? panel_tint(t.row_active, t.fast, 0.15f) : t.row_hover,
        model.waveform_visible ? t.text : t.text_secondary,
        model.waveform_visible ? 2.0f : 1.0f);
    result.waveform_toggled = waveform_toggle.clicked;

    const ui::dropdown_state dropdown = ui::enqueue_dropdown(
        layout::kSnapDropdownRect, snap_menu_rect,
        "Snap", model.snap_labels[model.snap_index],
        model.snap_labels,
        model.snap_index, model.snap_dropdown_open,
        ui::draw_layer::base, ui::draw_layer::overlay,
        14, 58.0f);
    result.snap_dropdown_toggled = dropdown.trigger.clicked;
    result.snap_index_clicked = dropdown.clicked_index;
    result.snap_dropdown_close_requested =
        model.snap_dropdown_open && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !ui::is_hovered(layout::kSnapDropdownRect, ui::draw_layer::base) &&
        !ui::is_hovered(snap_menu_rect, ui::draw_layer::overlay);
    return result;
}

void draw_timeline(const editor_timeline_presenter_model& presenter_model) {
    const auto& t = *g_theme;
    const editor_timeline_view_model model = make_timeline_model(presenter_model);
    const Rectangle panel = model.metrics.panel_rect;
    const Rectangle content = model.metrics.content_rect();
    const Rectangle track = model.metrics.scrollbar_track_rect();
    const std::set<size_t> selected_indices(model.selected_note_indices.begin(), model.selected_note_indices.end());

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.12f));
    ui::draw_rect_lines(panel, 1.5f, t.border);

    const Rectangle title = {panel.x + 14.0f, panel.y + 12.0f, panel.width - 28.0f, 54.0f};
    ui::draw_rect_f(title, panel_tint(t.section, t.accent, 0.06f));
    ui::draw_rect_lines(title, 1.0f, t.border_light);
    ui::draw_text_in_rect("Arrangement", 21, {title.x + 14.0f, title.y + 6.0f, 150.0f, 24.0f},
                          t.text, ui::text_align::left);
    draw_micro_label({title.x + 14.0f, title.y + 32.0f, 220.0f, 16.0f},
                     "measure ruler / notes / automation", t.text_muted);
    const char* selection_label = model.selected_note_indices.empty()
        ? (model.selected_note_index.has_value() ? "1 NOTE" : "NO SELECTION")
        : TextFormat("%d NOTES", static_cast<int>(model.selected_note_indices.size()));
    draw_badge({title.x + title.width - 330.0f, title.y + 12.0f, 120.0f, 30.0f},
               selection_label,
               model.selected_note_indices.empty() && !model.selected_note_index.has_value() ? t.text_muted : t.accent,
               model.selected_note_indices.empty() && !model.selected_note_index.has_value() ? t.text_secondary : t.text);
    draw_badge({title.x + title.width - 200.0f, title.y + 12.0f, 86.0f, 30.0f},
               TextFormat("%dt", model.snap_interval), t.fast, t.text);
    draw_badge({title.x + title.width - 104.0f, title.y + 12.0f, 90.0f, 30.0f},
               model.loop_enabled ? "LOOP ON" : "LOOP OFF",
               model.loop_enabled ? t.success : t.text_muted,
               model.loop_enabled ? t.success : t.text_secondary);

    const Rectangle arrange = {content.x, content.y + 62.0f, content.width, content.height - 62.0f};
    {
        ui::scoped_clip_rect clip_scope(arrange);
        draw_waveform(model, arrange);

        for (int lane = 0; lane < std::max(1, model.metrics.key_count); ++lane) {
            Rectangle lane_rect = model.metrics.lane_rect(lane);
            lane_rect.y = arrange.y;
            lane_rect.height = arrange.height;
            ui::draw_rect_f(lane_rect, lane % 2 == 0 ? with_alpha(t.row, 28) : with_alpha(t.section, 36));
            ui::draw_rect_lines(lane_rect, 1.0f, with_alpha(t.border_light, 150));
            ui::draw_text_in_rect(TextFormat("%d", lane + 1), 15,
                                  {lane_rect.x + 8.0f, arrange.y + 8.0f, lane_rect.width - 16.0f, 18.0f},
                                  t.text_hint, ui::text_align::left);
        }

        if (model.loop_end_tick > model.loop_start_tick &&
            model.loop_end_tick >= model.min_tick && model.loop_start_tick <= model.max_tick) {
            const float start_y = model.metrics.tick_to_y(model.loop_start_tick);
            const float end_y = model.metrics.tick_to_y(model.loop_end_tick);
            const Rectangle loop = {arrange.x, std::min(start_y, end_y), arrange.width,
                                    std::max(8.0f, std::fabs(end_y - start_y))};
            ui::draw_rect_f(loop, with_alpha(t.success, model.loop_enabled ? 48 : 20));
            ui::draw_rect_lines(loop, model.loop_enabled ? 2.0f : 1.0f,
                                with_alpha(t.success, model.loop_enabled ? 220 : 130));
        }

        const int snap_interval = std::max(1, model.snap_interval);
        const int first_snap_tick = std::max(0, (model.min_tick / snap_interval) * snap_interval);
        for (int tick = first_snap_tick; tick <= model.max_tick; tick += snap_interval) {
            const float y = model.metrics.tick_to_y(tick);
            if (y >= arrange.y && y <= arrange.y + arrange.height) {
                ui::draw_line_f(arrange.x, y, arrange.x + arrange.width, y, with_alpha(t.editor_grid_snap, 165));
            }
        }

        for (const editor_meter_map::grid_line& line : model.grid_lines) {
            const float y = model.metrics.tick_to_y(line.tick);
            if (y < arrange.y || y > arrange.y + arrange.height) {
                continue;
            }
            ui::draw_line_f(arrange.x, y, arrange.x + arrange.width, y,
                            line.major ? t.editor_grid_major : t.editor_grid_minor);
            if (line.major) {
                ui::draw_rect_f({arrange.x, y - 12.0f, 64.0f, 24.0f}, with_alpha(t.panel, 220));
                ui::draw_text_in_rect(TextFormat("%d:%d", line.measure, line.beat), 13,
                                      {arrange.x + 7.0f, y - 10.0f, 50.0f, 20.0f},
                                      t.text_secondary, ui::text_align::left);
            }
        }

        for (size_t index = 0; index < model.scroll_events.size(); ++index) {
            const editor_timeline_scroll_event& event = model.scroll_events[index];
            if (event.duration <= 0 || event.tick > model.max_tick || event.tick + event.duration < model.min_tick) {
                continue;
            }
            const float start_y = model.metrics.tick_to_y(event.tick);
            const float end_y = model.metrics.tick_to_y(event.tick + event.duration);
            const bool selected = model.selected_scroll_event_index.has_value() &&
                                  *model.selected_scroll_event_index == index;
            const Color tone = event.type == scroll_event_type::speed ? t.fast : t.error;
            const Rectangle band = {arrange.x, std::min(start_y, end_y), arrange.width,
                                    std::max(8.0f, std::fabs(end_y - start_y))};
            ui::draw_rect_f(band, with_alpha(tone, selected ? 92 : 42));
            ui::draw_rect_lines(band, selected ? 2.0f : 1.0f, with_alpha(tone, selected ? 230 : 140));
            ui::draw_text_in_rect(event.type == scroll_event_type::speed
                                      ? TextFormat("Speed %.2fx", event.multiplier)
                                      : "Stop",
                                  13,
                                  {band.x + band.width - 130.0f, band.y + 4.0f, 118.0f, 18.0f},
                                  selected ? t.text : t.text_secondary, ui::text_align::right);
        }

        for (size_t index = 0; index < model.notes.size(); ++index) {
            const editor_timeline_note& note = model.notes[index];
            if (note.lane < 0 || note.lane >= model.metrics.key_count) {
                continue;
            }
            const editor_timeline_note_draw_info info = model.metrics.note_rects(note);
            const bool selected = selected_indices.find(index) != selected_indices.end() ||
                                  (model.selected_note_index.has_value() && *model.selected_note_index == index);
            draw_note_block(note, info, selected, false, false);
        }

        if (model.preview_note.has_value()) {
            const editor_timeline_note_draw_info info = model.metrics.note_rects(*model.preview_note);
            draw_note_block(*model.preview_note, info, true, true, model.preview_has_overlap);
        }

        if (model.playback_tick.has_value()) {
            const float y = model.metrics.tick_to_y(*model.playback_tick);
            DrawLineEx({arrange.x, y}, {arrange.x + arrange.width, y}, 3.0f, t.accent);
            ui::draw_rect_f({arrange.x, y - 5.0f, 64.0f, 10.0f}, t.accent);
        }
    }

    const Rectangle ruler = {content.x, content.y, content.width, 50.0f};
    ui::draw_rect_f(ruler, with_alpha(t.section, 235));
    ui::draw_rect_lines(ruler, 1.0f, t.border_light);
    ui::draw_text_in_rect("RULER", 12, {ruler.x + 10.0f, ruler.y + 6.0f, 60.0f, 16.0f},
                          t.text_muted, ui::text_align::left);
    for (const editor_meter_map::grid_line& line : model.grid_lines) {
        if (!line.major) {
            continue;
        }
        const float y = model.metrics.tick_to_y(line.tick);
        if (y < arrange.y || y > arrange.y + arrange.height) {
            continue;
        }
        const float x = ruler.x + 80.0f + std::fmod(static_cast<float>(line.measure) * 74.0f, ruler.width - 120.0f);
        ui::draw_text_in_rect(TextFormat("%d", line.measure), 13, {x, ruler.y + 24.0f, 34.0f, 18.0f}, t.text_secondary);
    }

    ui::draw_rect_f(track, t.scrollbar_track);
    if (model.content_height_pixels > 1.0f) {
        const float thumb_ratio = std::clamp(content.height / model.content_height_pixels, 0.06f, 1.0f);
        const float thumb_height = std::max(24.0f, track.height * thumb_ratio);
        const float max_scroll = std::max(1.0f, model.content_height_pixels - content.height);
        const float thumb_y = track.y + (track.height - thumb_height) *
            std::clamp(model.scroll_offset_pixels / max_scroll, 0.0f, 1.0f);
        ui::draw_rect_f({track.x, thumb_y, track.width, thumb_height}, t.scrollbar_thumb);
    }
}

}  // namespace editor::daw
