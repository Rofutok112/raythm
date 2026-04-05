#include "editor_header_view.h"

#include "editor_layout.h"
#include "theme.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;
}

editor_header_view_result editor_header_view::draw(const editor_header_view_model& model, Rectangle snap_menu_rect) {
    const auto& t = *g_theme;
    editor_header_view_result result;

    ui::draw_section(layout::kPlaybackRect);
    ui::draw_label_value(ui::inset(layout::kPlaybackRect, ui::edge_insets::symmetric(0.0f, 12.0f)),
                         "Audio", model.playback_status, 16,
                         t.text, model.audio_loaded ? t.text_secondary : t.text_muted, 56.0f);

    const ui::selector_state chart_offset = ui::draw_value_selector(
        layout::kChartOffsetRect, "Offset", model.offset_label,
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
