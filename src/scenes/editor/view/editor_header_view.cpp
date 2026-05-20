#include "editor/view/editor_header_view.h"

#include "editor/view/editor_layout.h"
#include "theme.h"
#include "ui/icons/raythm_icons.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;

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
        active ? t.row_selected : t.row,
        active ? t.row_active : t.row_hover,
        active ? t.text : t.text_secondary,
        1.5f);
    draw_icon(centered_icon_rect(rect, 13.0f), active ? active_color : t.text_secondary, 2.8f);
    return button;
}
}

editor_header_view_result editor_header_view::draw(const editor_header_view_model& model, Rectangle snap_menu_rect) {
    const auto& t = *g_theme;
    editor_header_view_result result;

    ui::draw_section(layout::kEditorTitleRect);
    ui::draw_label_value(ui::inset(layout::kEditorTitleRect, ui::edge_insets::symmetric(0.0f, 12.0f)),
                         "raythm", "Chart Editor", 16,
                         t.text, t.accent, 72.0f);

    ui::draw_section(layout::kTransportBarRect);
    const Rectangle transport_content = ui::inset(layout::kTransportBarRect, ui::edge_insets::symmetric(0.0f, 10.0f));
    const Rectangle play_rect = {transport_content.x, transport_content.y + 5.0f, 41.0f, 41.0f};
    const ui::button_state play_button = model.audio_playing
        ? draw_icon_button(play_rect, raythm_icons::draw_pause, true, t.accent)
        : draw_icon_button(play_rect, raythm_icons::draw_play, false, t.text);
    result.playback_toggled = play_button.clicked;
    ui::draw_label_value({play_rect.x + play_rect.width + 12.0f, transport_content.y, 196.0f, transport_content.height},
                         "Transport", model.playback_status, 16,
                         t.text, model.audio_loaded ? t.text_secondary : t.text_muted, 86.0f);

    const Rectangle loop_rect = {transport_content.x + 266.0f, transport_content.y + 5.0f, 41.0f, 41.0f};
    const ui::button_state loop_button = draw_icon_button(loop_rect, raythm_icons::draw_repeat_2,
                                                          model.loop_enabled, t.success);
    result.loop_toggled = loop_button.clicked;
    ui::draw_label_value({loop_rect.x + loop_rect.width + 10.0f, transport_content.y, 156.0f, transport_content.height},
                         "Loop", model.loop_label, 16,
                         model.loop_enabled ? t.success : t.text_secondary,
                         model.loop_enabled ? t.text : t.text_muted, 46.0f);

    ui::draw_section(layout::kLoopStatusRect);
    ui::draw_label_value(ui::inset(layout::kLoopStatusRect, ui::edge_insets::symmetric(0.0f, 12.0f)),
                         "Region", model.loop_label, 16,
                         model.loop_enabled ? t.success : t.text_secondary,
                         model.loop_enabled ? t.text : t.text_muted, 70.0f);

    const ui::selector_state chart_offset = ui::draw_value_selector(
        layout::kChartOffsetRect, "Song Offset", model.offset_label,
        16, 24.0f, 68.0f, 10.0f);
    result.offset_left_clicked = chart_offset.left.clicked;
    result.offset_right_clicked = chart_offset.right.clicked;

    const ui::button_state waveform_toggle = ui::draw_button_colored(
        layout::kWaveformToggleRect, model.waveform_visible ? "WAVE ON" : "WAVE OFF", 16,
        model.waveform_visible ? t.row_selected : t.row,
        model.waveform_visible ? t.row_active : t.row_hover,
        model.waveform_visible ? t.text : t.text_secondary);
    result.waveform_toggled = waveform_toggle.clicked;

    const ui::dropdown_state dropdown = ui::enqueue_dropdown(
        layout::kSnapDropdownRect, snap_menu_rect,
        "Snap", model.snap_labels[model.snap_index],
        model.snap_labels,
        model.snap_index, model.snap_dropdown_open,
        ui::draw_layer::base, ui::draw_layer::overlay,
        16, 64.0f);
    result.snap_dropdown_toggled = dropdown.trigger.clicked;
    result.snap_index_clicked = dropdown.clicked_index;
    result.snap_dropdown_close_requested =
        model.snap_dropdown_open && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !ui::is_hovered(layout::kSnapDropdownRect, ui::draw_layer::base) &&
        !ui::is_hovered(snap_menu_rect, ui::draw_layer::overlay);
    return result;
}
