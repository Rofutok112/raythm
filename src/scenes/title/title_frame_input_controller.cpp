#include "title/title_frame_input_controller.h"

#include "network/auth_client.h"
#include "raylib.h"
#include "song_select/song_select_login_dialog.h"
#include "title/catalog_reload_policy.h"
#include "title/title_home_input_controller.h"
#include "title/title_layout.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_notice.h"
#include "virtual_screen.h"
#include "platform/window_chrome.h"

namespace {

constexpr float kAccountChipInteractiveThreshold = 0.2f;

bool is_settings_mode(title::common_mode mode) {
    return mode == title::common_mode::settings;
}

bool is_play_or_create_mode(title::common_mode mode) {
    return mode == title::common_mode::play || mode == title::common_mode::create;
}

bool update_home_pointer_suppression(bool& suppress_home_pointer_until_release) {
    if (!suppress_home_pointer_until_release) {
        return false;
    }

    if (ui::is_mouse_button_down() || ui::is_mouse_button_down(MOUSE_BUTTON_RIGHT)) {
        return true;
    }

    suppress_home_pointer_until_release = false;
    return true;
}

bool handle_account_input(title::frame_input_context& context) {
    if (is_settings_mode(context.mode)) {
        return false;
    }
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    if (context.home_menu_anim < kAccountChipInteractiveThreshold || !ui::is_clicked(account_chip_rect)) {
        return false;
    }
    if (context.play_create_feature.state().login_dialog.open) {
        context.play_create_feature.state().login_dialog.open = false;
    } else {
        song_select::open_login_dialog(context.play_create_feature.state().login_dialog, auth::load_session_summary());
        auth_overlay::refresh_auth_state(context.play_create_feature.state().auth);
    }
    return true;
}

bool handle_login_dialog_input(title::frame_input_context& context) {
    song_select::state& play_state = context.play_create_feature.state();
    if (!play_state.login_dialog.open) {
        return false;
    }
    if (ui::is_cancel_pressed() && !context.auth_controller.request_active) {
        play_state.login_dialog.open = false;
        return true;
    }
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    const Rectangle account_dialog_anchor = {
        account_chip_rect.x,
        account_chip_rect.y + 12.0f,
        account_chip_rect.width,
        account_chip_rect.height
    };
    const song_select::login_dialog_layout login_layout = song_select::make_login_dialog_layout(
        play_state.auth,
        play_state.login_dialog,
        account_dialog_anchor,
        title_layout::screen_rect(),
        ui::draw_layer::modal);
    if (ui::is_mouse_button_released_outside(login_layout.dialog_rect, virtual_screen::get_virtual_mouse())) {
        play_state.login_dialog.open = false;
        return true;
    }
    return true;
}

bool handle_refresh_button_input(title::frame_input_context& context) {
    if (is_settings_mode(context.mode) || context.home_menu_anim < kAccountChipInteractiveThreshold) {
        return false;
    }
    if (!ui::is_clicked(title_layout::refresh_chip_rect())) {
        return false;
    }

    context.play_create_feature.capture_current_selection();
    context.browse_feature.request_reload(true);
    context.catalog_reload_coordinator.request_reload(
        context.play_create_feature,
        context.play_create_feature.preferred_song_id(),
        context.play_create_feature.preferred_chart_id(),
        title_catalog::policy_for(title_catalog::reload_mode::user_refresh));
    ui::notify("Refreshing catalog...", ui::notice_tone::info, 1.8f);
    return true;
}

bool handle_friends_button_input(title::frame_input_context& context) {
    if (is_settings_mode(context.mode) || context.home_menu_anim < kAccountChipInteractiveThreshold) {
        return false;
    }
    if (!ui::is_clicked(title_layout::friends_chip_rect())) {
        return false;
    }
    context.play_create_feature.state().login_dialog.open = false;
    context.friends_controller.open();
    context.modals.bring_to_front(title::modal_id::friends);
    return true;
}

bool handle_rating_rankings_button_input(title::frame_input_context& context) {
    if (is_settings_mode(context.mode) || context.home_menu_anim < kAccountChipInteractiveThreshold) {
        return false;
    }
    if (!ui::is_clicked(title_layout::rating_rankings_chip_rect())) {
        return false;
    }
    context.play_create_feature.state().login_dialog.open = false;
    context.rating_rankings_controller.open();
    context.modals.bring_to_front(title::modal_id::rating_rankings);
    return true;
}

bool settings_button_clicked(const title::frame_input_context& context) {
    if (is_settings_mode(context.mode) || context.home_menu_anim < kAccountChipInteractiveThreshold) {
        return false;
    }
    return ui::is_clicked(title_layout::settings_chip_rect());
}

bool left_click_for_title_home(float home_menu_anim) {
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    const Rectangle refresh_chip_rect = title_layout::refresh_chip_rect();
    const Rectangle settings_chip_rect = title_layout::settings_chip_rect();
    const Rectangle rating_rankings_chip_rect = title_layout::rating_rankings_chip_rect();
    const Rectangle friends_chip_rect = title_layout::friends_chip_rect();
    const bool account_hovered =
        home_menu_anim >= kAccountChipInteractiveThreshold && ui::is_hovered(account_chip_rect);
    const bool refresh_hovered =
        home_menu_anim >= kAccountChipInteractiveThreshold && ui::is_hovered(refresh_chip_rect);
    const bool settings_hovered =
        home_menu_anim >= kAccountChipInteractiveThreshold && ui::is_hovered(settings_chip_rect);
    const bool rating_rankings_hovered =
        home_menu_anim >= kAccountChipInteractiveThreshold && ui::is_hovered(rating_rankings_chip_rect);
    const bool friends_hovered =
        home_menu_anim >= kAccountChipInteractiveThreshold && ui::is_hovered(friends_chip_rect);
    return ui::is_mouse_button_pressed() &&
           !window_chrome::is_pointer_over_chrome() &&
           !account_hovered &&
           !refresh_hovered &&
           !settings_hovered &&
           !rating_rankings_hovered &&
           !friends_hovered;
}

}  // namespace

namespace title {

frame_input_result update_frame_input(frame_input_context& context) {
    const bool suppress_home_pointer_this_frame =
        update_home_pointer_suppression(context.suppress_home_pointer_until_release);

    if (!suppress_home_pointer_this_frame && handle_account_input(context)) {
        return {.consumed = true};
    }

    if (handle_login_dialog_input(context)) {
        return {.consumed = true};
    }

    if (!suppress_home_pointer_this_frame && handle_refresh_button_input(context)) {
        return {.consumed = true};
    }

    if (!suppress_home_pointer_this_frame && handle_friends_button_input(context)) {
        return {.consumed = true};
    }

    if (!suppress_home_pointer_this_frame && handle_rating_rankings_button_input(context)) {
        return {.consumed = true};
    }

    if (!suppress_home_pointer_this_frame && settings_button_clicked(context)) {
        return {
            .consumed = true,
            .enter_settings = true,
        };
    }

    if (context.mode == common_mode::title) {
        const bool left_click = left_click_for_title_home(context.home_menu_anim);
        const bool right_click = ui::is_mouse_button_pressed(MOUSE_BUTTON_RIGHT);
        if (ui::is_enter_pressed() || left_click || right_click) {
            return {
                .consumed = true,
                .enter_home = true,
                .enter_home_suppress_pointer = left_click || right_click,
            };
        }
        return {};
    }

    if (is_play_or_create_mode(context.mode) || context.mode != common_mode::home) {
        return {};
    }

    const title_home_input_controller::result home_result =
        title_home_input_controller::update(context.home_menu_selected_index,
                                            context.home_status_message,
                                            context.home_menu_anim,
                                            suppress_home_pointer_this_frame);
    if (home_result.enter_title) {
        return {
            .consumed = true,
            .enter_title = true,
        };
    }
    if (home_result.selected_action.has_value()) {
        return {
            .consumed = true,
            .selected_home_action = home_result.selected_action,
        };
    }
    return {
        .consumed = home_result.consumed,
    };
}

}  // namespace title
