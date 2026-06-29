#include "mv_editor_context_menu_view.h"

#include <algorithm>
#include <array>
#include <span>

#include "ui_draw.h"
#include "ui_hit.h"

namespace {

constexpr float kScreenWidth = 1920.0f;
constexpr float kScreenHeight = 1080.0f;
constexpr float kPadding = 16.0f;
constexpr float kContextMenuWidth = 188.0f;
constexpr float kContextMenuItemHeight = 24.0f;
constexpr float kContextMenuItemSpacing = 3.0f;

Rectangle context_menu_rect_for(Vector2 position, int item_count) {
    const float menu_height = 12.0f +
        static_cast<float>(item_count) * kContextMenuItemHeight +
        static_cast<float>(std::max(0, item_count - 1)) * kContextMenuItemSpacing;
    Rectangle rect = {position.x, position.y, kContextMenuWidth, menu_height};
    rect.x = std::clamp(rect.x, kPadding, kScreenWidth - rect.width - kPadding);
    rect.y = std::clamp(rect.y, kPadding, kScreenHeight - rect.height - kPadding);
    return rect;
}

bool should_close_context_menu(Rectangle menu_rect, Vector2 mouse, bool opened_this_frame = false) {
    if (opened_this_frame) {
        return false;
    }
    return ui::is_mouse_button_pressed_outside(menu_rect, mouse);
}

} // namespace

mv_editor_context_menu_result draw_mv_context_menu(mv_editor_context_menu_target target,
                                                   Vector2 position,
                                                   bool has_layer,
                                                   bool has_effects,
                                                   bool opened_this_frame,
                                                   Vector2 mouse) {
    switch (target) {
    case mv_editor_context_menu_target::hierarchy: {
        std::array<ui::context_menu_item, 8> items = {{
            {"Create Object", false, ui::context_menu_item::kind::header},
            {"Empty", true},
            {"Text", true},
            {"Rectangle", true},
            {"Image", true},
            {"Beat Grid", true},
            {"Waveform", true},
            {"Spectrum", true},
        }};
        const Rectangle menu_rect = context_menu_rect_for(position, static_cast<int>(items.size()));
        const ui::context_menu_state menu =
            ui::context_menu(menu_rect, std::span<const ui::context_menu_item>(items), {
                .layer = ui::draw_layer::overlay,
                .font_size = 11,
                .item_height = kContextMenuItemHeight,
                .item_spacing = kContextMenuItemSpacing,
            });
        if (menu.clicked_index >= 0) {
            switch (menu.clicked_index) {
            case 1: return {.action = mv_editor_context_menu_action::add_empty_layer};
            case 2: return {.action = mv_editor_context_menu_action::add_text_layer};
            case 3: return {.action = mv_editor_context_menu_action::add_rect_layer};
            case 4: return {.action = mv_editor_context_menu_action::add_image_layer};
            case 5: return {.action = mv_editor_context_menu_action::add_beat_grid_layer};
            case 6: return {.action = mv_editor_context_menu_action::add_waveform_layer};
            case 7: return {.action = mv_editor_context_menu_action::add_spectrum_layer};
            default: return {.action = mv_editor_context_menu_action::close};
            }
        }
        if (should_close_context_menu(menu_rect, mouse)) {
            return {.action = mv_editor_context_menu_action::close};
        }
        return {};
    }
    case mv_editor_context_menu_target::project_assets: {
        std::array<ui::context_menu_item, 3> items = {{
            {"Project", false, ui::context_menu_item::kind::header},
            {"Import Image", true},
            {"New Script", true},
        }};
        const Rectangle menu_rect = context_menu_rect_for(position, static_cast<int>(items.size()));
        const ui::context_menu_state menu =
            ui::context_menu(menu_rect, std::span<const ui::context_menu_item>(items), {
                .layer = ui::draw_layer::overlay,
                .font_size = 11,
                .item_height = kContextMenuItemHeight,
                .item_spacing = kContextMenuItemSpacing,
            });
        if (menu.clicked_index >= 0) {
            switch (menu.clicked_index) {
            case 1: return {.action = mv_editor_context_menu_action::import_image_asset};
            case 2: return {.action = mv_editor_context_menu_action::create_script_asset};
            default: return {.action = mv_editor_context_menu_action::close};
            }
        }
        if (should_close_context_menu(menu_rect, mouse)) {
            return {.action = mv_editor_context_menu_action::close};
        }
        return {};
    }
    case mv_editor_context_menu_target::components: {
        std::array<ui::context_menu_item, 16> items = {{
            {"Add Component", false, ui::context_menu_item::kind::header},
            {"Text", has_layer},
            {"Rectangle", has_layer},
            {"Image", has_layer},
            {"Beat Grid", has_layer},
            {"Waveform", has_layer},
            {"Spectrum", has_layer},
            {"", false, ui::context_menu_item::kind::separator},
            {"Fade", has_layer},
            {"Pulse", has_layer},
            {"Flash", has_layer},
            {"Shake", has_layer},
            {"Lua Behaviour", has_layer},
            {"", false, ui::context_menu_item::kind::separator},
            {"Clear Effects", has_effects},
        }};
        const Rectangle menu_rect = context_menu_rect_for(position, static_cast<int>(items.size()));
        const ui::context_menu_state menu =
            ui::context_menu(menu_rect, std::span<const ui::context_menu_item>(items), {
                .layer = ui::draw_layer::overlay,
                .font_size = 11,
                .item_height = kContextMenuItemHeight,
                .item_spacing = kContextMenuItemSpacing,
            });
        if (menu.clicked_index >= 0) {
            switch (menu.clicked_index) {
            case 1:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "TextRenderer"};
            case 2:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "ShapeRenderer"};
            case 3:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "ImageRenderer"};
            case 4:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "BeatGridRenderer"};
            case 5:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "WaveformRenderer"};
            case 6:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "SpectrumRenderer"};
            case 8:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "Fade"};
            case 9:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "Pulse"};
            case 10:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "Flash"};
            case 11:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "Shake"};
            case 12:
                return {.action = mv_editor_context_menu_action::add_component, .component_type = "LuaBehaviour"};
            case 14:
                return {.action = mv_editor_context_menu_action::clear_effects};
            default:
                return {.action = mv_editor_context_menu_action::close};
            }
        }
        if (should_close_context_menu(menu_rect, mouse, opened_this_frame)) {
            return {.action = mv_editor_context_menu_action::close};
        }
        return {};
    }
    case mv_editor_context_menu_target::timeline: {
        std::array<ui::context_menu_item, 2> items = {{
            {"Timeline", false, ui::context_menu_item::kind::header},
            {"Delete Layer", has_layer},
        }};
        const Rectangle menu_rect = context_menu_rect_for(position, static_cast<int>(items.size()));
        const ui::context_menu_state menu =
            ui::context_menu(menu_rect, std::span<const ui::context_menu_item>(items), {
                .layer = ui::draw_layer::overlay,
                .font_size = 11,
                .item_height = kContextMenuItemHeight,
                .item_spacing = kContextMenuItemSpacing,
            });
        if (menu.clicked_index >= 0) {
            switch (menu.clicked_index) {
            case 1: return {.action = mv_editor_context_menu_action::delete_layer};
            default: return {.action = mv_editor_context_menu_action::close};
            }
        }
        if (should_close_context_menu(menu_rect, mouse)) {
            return {.action = mv_editor_context_menu_action::close};
        }
        return {};
    }
    case mv_editor_context_menu_target::none:
        break;
    }
    return {};
}
