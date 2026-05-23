#include "title/title_hub_view.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "scene_common.h"
#include "song_select/song_select_login_dialog.h"
#include "theme.h"
#include "title/home_menu_view.h"
#include "title/play_session_controller.h"
#include "title/title_header_view.h"
#include "title/title_layout.h"
#include "title/title_multiplayer_view.h"
#include "title/seamless_song_select_view.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr ui::draw_layer kTitleModalLayer = ui::draw_layer::modal;

std::string make_avatar_label(const auth::session_summary& summary) {
    const std::string source = summary.logged_in
        ? (summary.display_name.empty() ? summary.email : summary.display_name)
        : "A";
    if (source.empty()) {
        return "A";
    }

    std::string result;
    result.reserve(2);
    for (char ch : source) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "A" : result;
}

std::string make_avatar_label(const song_select::auth_state& auth_state) {
    const auth::session_summary summary = {
        auth_state.logged_in,
        {},
        auth_state.email,
        auth_state.display_name,
        auth_state.email_verified,
    };
    return make_avatar_label(summary);
}

const char* account_name_for(const song_select::auth_state& auth_state) {
    if (!auth_state.logged_in) {
        return "ACCOUNT";
    }
    return auth_state.display_name.empty() ? auth_state.email.c_str() : auth_state.display_name.c_str();
}

const char* account_status_for(const song_select::auth_state& auth_state) {
    if (!auth_state.logged_in) {
        return "Manage account";
    }
    return auth_state.email_verified ? "Verified profile" : "Manage account";
}

bool is_play_surface(title_hub_view::mode mode) {
    return mode == title_hub_view::mode::play || mode == title_hub_view::mode::create;
}

}  // namespace

namespace title_hub_view {

draw_result draw(draw_context context) {
    draw_result result;
    const auto& t = *g_theme;
    const float menu_t = tween::ease_out_cubic(context.view.home_menu_anim);
    const float play_t = tween::ease_out_cubic(context.view.play_view_anim);
    const Rectangle screen_rect = title_layout::screen_rect();
    const Rectangle spectrum_rect = title_layout::spectrum_rect();
    const Rectangle settings_chip_rect = title_layout::settings_chip_rect();
    const Rectangle refresh_chip_rect = title_layout::refresh_chip_rect();
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    const bool draw_title_header = context.view.current_mode != mode::settings;
    const title_header_view::draw_config header_config = {
        .closed_header_rect = title_layout::closed_header_rect(),
        .open_header_rect = title_layout::open_header_rect(),
        .refresh_chip_rect = refresh_chip_rect,
        .settings_chip_rect = settings_chip_rect,
        .account_chip_rect = account_chip_rect,
        .menu_t = menu_t,
        .play_t = play_t,
        .account_name = account_name_for(context.play_state.auth),
        .account_status = account_status_for(context.play_state.auth),
        .avatar_label = make_avatar_label(context.play_state.auth),
        .logged_in = context.play_state.auth.logged_in,
        .email_verified = context.play_state.auth.email_verified,
        .now = GetTime(),
    };

    virtual_screen::begin_ui();
    draw_scene_background(t);
    ui::begin_draw_queue();
    context.audio_controller.draw_spectrum(spectrum_rect, tween::lerp(1.0f, 0.5f, play_t));
    if (draw_title_header) {
        title_header_view::draw(header_config);
        if (!context.startup.loading) {
            title_home_view::draw(
                context.view.home_menu_anim,
                context.view.play_view_anim,
                context.view.home_menu_selected_index,
                context.view.home_status_message);
        }
    }

    if (context.startup.loading) {
        title_startup_controller::draw_loading(context.startup, GetFrameTime());
    } else if (context.startup.load_failed && context.view.current_mode == mode::title) {
        title_startup_controller::draw_status(context.startup);
    } else if (context.view.current_mode == mode::settings) {
        context.settings_overlay.draw();
    } else if (is_play_surface(context.view.current_mode)) {
        title_play_view::draw(
            context.play_state,
            context.audio_controller.preview(),
            context.view.current_mode == mode::create ? title_play_view::mode::create : title_play_view::mode::play,
            context.view.play_view_anim,
            context.view.play_entry_origin_rect);
    } else if (context.view.current_mode == mode::multiplayer) {
        title_multiplayer_view::draw(
            context.multiplayer_state,
            context.play_state,
            context.view.play_view_anim,
            context.view.play_entry_origin_rect);
    } else if (context.view.current_mode == mode::online) {
        title_online_view::draw(context.online_state, context.view.play_view_anim, context.view.play_entry_origin_rect);
    }

    const Rectangle account_dialog_anchor = {
        account_chip_rect.x,
        account_chip_rect.y + 12.0f,
        account_chip_rect.width,
        account_chip_rect.height
    };
    if (is_play_surface(context.view.current_mode)) {
        context.play_transfer_controller.draw_or_apply_confirmation(
            context.play_state,
            context.audio_controller.preview(),
            context.transfer_callbacks,
            true);
    }
    context.profile_controller.draw(context.play_state.auth, context.auth_controller.request_active, kTitleModalLayer);
    result.login_command = song_select::draw_login_dialog(
        context.play_state.auth,
        context.play_state.login_dialog,
        account_dialog_anchor,
        screen_rect,
        context.auth_controller.request_active,
        kTitleModalLayer);
    result.close_login_dialog = result.login_command == song_select::login_dialog_command::close;
    result.open_profile = result.login_command == song_select::login_dialog_command::request_profile;

    ui::flush_draw_queue();

    if (!context.startup.loading) {
        if (context.view.intro_hold_active) {
            ui::draw_fullscreen_overlay(BLACK);
        } else {
            context.intro_fade.draw();
        }
    }
    if (context.view.transitioning_to_song_select) {
        context.transition_fade.draw();
    }
    if (context.view.quitting) {
        context.quit_fade.draw();
    }
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
    if (draw_title_header) {
        title_header_view::draw_screen_overlay(header_config);
    }
    return result;
}

}  // namespace title_hub_view
