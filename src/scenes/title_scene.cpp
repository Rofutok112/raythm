#include "title_scene.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "multiplayer/multiplayer_controller.h"
#include "network/server_environment.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_last_played.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_navigation.h"
#include "title/home_menu_view.h"
#include "title/catalog_reload_policy.h"
#include "title/local_content_index.h"
#include "title/title_common_update_controller.h"
#include "title/title_frame_input_controller.h"
#include "title/title_command_dispatcher.h"
#include "title/title_hub_view.h"
#include "title/title_mode_update_controller.h"
#include "title/title_multiplayer_flow_controller.h"
#include "title/title_settings_flow_controller.h"
#include "title/title_startup_controller.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr const char* kTitleIntroPath = "assets/audio/title_intro.mp3";
constexpr const char* kTitleLoopPath = "assets/audio/title_loop.mp3";
constexpr ui::draw_layer kTitleModalLayer = ui::draw_layer::modal;

title::common_mode to_title_common_mode(title_scene::hub_mode mode) {
    switch (mode) {
    case title_scene::hub_mode::title:
        return title::common_mode::title;
    case title_scene::hub_mode::home:
        return title::common_mode::home;
    case title_scene::hub_mode::play:
        return title::common_mode::play;
    case title_scene::hub_mode::multiplayer:
        return title::common_mode::multiplayer;
    case title_scene::hub_mode::online:
        return title::common_mode::online;
    case title_scene::hub_mode::create:
        return title::common_mode::create;
    case title_scene::hub_mode::settings:
        return title::common_mode::settings;
    }
    return title::common_mode::title;
}

title_scene::hub_mode content_mode_for_settings(title_scene::hub_mode mode, title_scene::hub_mode return_mode) {
    return mode == title_scene::hub_mode::settings ? return_mode : mode;
}

const song_select::song_entry* selected_audio_song(title_scene::hub_mode mode,
                                                   const song_select::state& play_state,
                                                   const title_online_view::state& online_state) {
    if (mode == title_scene::hub_mode::online) {
        return title_online_view::preview_song(online_state);
    }
    return song_select::selected_song(play_state);
}

title_scene::transition_target transition_target_for_home_action(title_home_view::action action) {
    return action == title_home_view::action::multiplayer
        ? title_scene::transition_target::multiplayer
        : action == title_home_view::action::create
        ? title_scene::transition_target::create_tools
        : action == title_home_view::action::online
            ? title_scene::transition_target::online_download
            : title_scene::transition_target::song_select;
}

}  // namespace

title_scene::title_scene(scene_manager& manager,
                         bool start_with_home_open,
                         bool play_intro_fade,
                         std::string preferred_song_id,
                          std::string preferred_chart_id,
                          std::optional<song_select::recent_result_offset> recent_result_offset,
                          bool start_in_play_view,
                          bool start_in_create_view,
                          std::string preferred_multiplayer_room_id,
                          bool start_in_multiplayer_view,
                          bool start_in_settings_view) :
    scene(manager),
    start_with_home_open_(start_with_home_open),
    play_intro_fade_(play_intro_fade),
    preferred_song_id_(std::move(preferred_song_id)),
    preferred_chart_id_(std::move(preferred_chart_id)),
    recent_result_offset_(std::move(recent_result_offset)),
    start_in_play_view_(start_in_play_view),
    start_in_create_view_(start_in_create_view),
    preferred_multiplayer_room_id_(std::move(preferred_multiplayer_room_id)),
    start_in_multiplayer_view_(start_in_multiplayer_view),
    start_in_settings_view_(start_in_settings_view),
    settings_overlay_(g_settings) {
}

void title_scene::enter_title_mode() {
    mode_ = hub_mode::title;
    suppress_home_pointer_until_release_ = false;
    home_status_message_.clear();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_create_feature_.state()), 0.0f);
}

void title_scene::enter_home_mode(bool suppress_pointer) {
    mode_ = hub_mode::home;
    suppress_home_pointer_until_release_ = suppress_pointer;
    home_status_message_.clear();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_create_feature_.state()), 0.0f);
}

void title_scene::enter_play_mode() {
    mode_ = hub_mode::play;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    play_create_feature_.on_enter_play(
        multiplayer_chart_pick_active_,
        server_environment::normalize_url(multiplayer_state_.auth.server_url),
        audio_controller_);
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::enter_multiplayer_mode(bool reset_room_state) {
    mode_ = hub_mode::multiplayer;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    title::reset_multiplayer_audio(multiplayer_audio_state_);
    audio_controller_.stop_preview();
    refresh_multiplayer_local_index();
    if (reset_room_state) {
        multiplayer::on_enter(multiplayer_state_, preferred_multiplayer_room_id_, preferred_multiplayer_invite_id_);
        preferred_multiplayer_room_id_.clear();
        preferred_multiplayer_invite_id_.clear();
    }
    audio_controller_.update_multiplayer_preview(nullptr, 0.0f);
}

void title_scene::enter_multiplayer_room_invite(std::string room_id, std::string invite_id) {
    preferred_multiplayer_room_id_ = std::move(room_id);
    preferred_multiplayer_invite_id_ = std::move(invite_id);
    friends_controller_.close();
    enter_multiplayer_mode(true);
}

void title_scene::refresh_multiplayer_local_index() {
    multiplayer_local_index_ = local_content_index::load_snapshot();
}

void title_scene::enter_online_mode() {
    mode_ = hub_mode::online;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    browse_feature_.on_enter(audio_controller_);
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::enter_create_mode() {
    mode_ = hub_mode::create;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    play_create_feature_.on_enter_create(audio_controller_);
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::enter_settings_mode() {
    settings_return_mode_ = mode_ == hub_mode::settings ? settings_return_mode_ : mode_;
    mode_ = hub_mode::settings;
    home_status_message_.clear();
    play_create_feature_.state().login_dialog.open = false;
    friends_controller_.close();
    profile_controller_.close();
    public_profile_controller_.close();
    settings_overlay_.open();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_create_feature_.state()), 0.0f);
}

void title_scene::close_settings_mode() {
    settings_overlay_.request_close();
}

void title_scene::update_startup_loading() {
    title_startup_controller::update(startup_, {
        play_create_feature_.state(),
        preferred_song_id_,
        preferred_chart_id_,
        home_status_message_,
        [this](std::string song_id, std::string chart_id, title_catalog::reload_policy policy) {
            play_create_feature_.request_catalog_reload(std::move(song_id), std::move(chart_id), policy);
        },
        [this]() {
            return play_create_feature_.catalog_loading();
        },
        [this]() {
            browse_feature_.request_reload();
        },
        [this]() {
            if (!start_in_multiplayer_view_) {
                auth_overlay::start_restore(auth_controller_, play_create_feature_.state().login_dialog);
            }
        },
        [this](bool force_refresh) {
            play_create_feature_.request_scoring_ruleset_warm(force_refresh);
        },
        [this]() {
            return play_create_feature_.scoring_ruleset_loading();
        },
        [this]() {
            return play_create_feature_.catalog_progress();
        },
    });
}

title_hub_view::mode to_title_hub_view_mode(title_scene::hub_mode mode) {
    switch (mode) {
        case title_scene::hub_mode::title:
            return title_hub_view::mode::title;
        case title_scene::hub_mode::home:
            return title_hub_view::mode::home;
        case title_scene::hub_mode::play:
            return title_hub_view::mode::play;
        case title_scene::hub_mode::multiplayer:
            return title_hub_view::mode::multiplayer;
        case title_scene::hub_mode::online:
            return title_hub_view::mode::online;
        case title_scene::hub_mode::create:
            return title_hub_view::mode::create;
        case title_scene::hub_mode::settings:
            return title_hub_view::mode::settings;
    }
    return title_hub_view::mode::title;
}

bool title_scene::handle_profile_input() {
    const title_profile_controller::input_result result =
        profile_controller_.handle_input(auth_controller_.request_active);
    if (result.delete_account_password.has_value()) {
        play_create_feature_.state().login_dialog.password_input.value = *result.delete_account_password;
        auth_overlay::start_request(auth_controller_, play_create_feature_.state().login_dialog,
                                    song_select::login_dialog_command::request_delete_account);
    }
    return result.consumed;
}

bool title_scene::handle_public_profile_input() {
    return public_profile_controller_.handle_input();
}

bool title_scene::handle_friends_input() {
    return friends_controller_.handle_input();
}

title_play_create_feature::cross_callbacks title_scene::play_cross_callbacks() {
    return {
        .stop_preview = [this]() {
            audio_controller_.stop_preview();
        },
        .mark_online_song_removed = [this](const std::string& song_id) {
            browse_feature_.mark_song_removed(song_id);
        },
        .reload_online_catalog = [this]() {
            browse_feature_.request_reload();
        },
    };
}

title::mode_update_context title_scene::make_mode_update_context() {
    return {
        .manager = manager_,
        .play_create_feature = play_create_feature_,
        .audio_controller = audio_controller_,
        .browse_feature = browse_feature_,
        .catalog_reload_coordinator = catalog_reload_coordinator_,
        .play_view_anim = play_view_anim_,
        .play_entry_origin_rect = play_entry_origin_rect_,
        .preferred_song_id = preferred_song_id_,
        .preferred_chart_id = preferred_chart_id_,
        .cross_callbacks = play_cross_callbacks(),
        .enter_home_mode = [this](bool suppress_pointer) { enter_home_mode(suppress_pointer); },
        .enter_play_mode = [this]() { enter_play_mode(); },
    };
}

title::common_update_context title_scene::make_common_update_context() {
    return {
        .mode = to_title_common_mode(mode_),
        .settings_return_mode = to_title_common_mode(settings_return_mode_),
        .auth_controller = auth_controller_,
        .play_create_feature = play_create_feature_,
        .audio_controller = audio_controller_,
        .browse_feature = browse_feature_,
        .catalog_reload_coordinator = catalog_reload_coordinator_,
        .profile_controller = profile_controller_,
        .public_profile_controller = public_profile_controller_,
        .settings_overlay = settings_overlay_,
        .startup = startup_,
        .intro_fade = intro_fade_,
        .intro_hold_t = intro_hold_t_,
        .home_menu_anim = home_menu_anim_,
        .play_view_anim = play_view_anim_,
        .preferred_song_id = preferred_song_id_,
        .preferred_chart_id = preferred_chart_id_,
        .cross_callbacks = play_cross_callbacks(),
        .refresh_multiplayer_local_index = [this]() { refresh_multiplayer_local_index(); },
        .current_audio_mode = [this]() { return current_audio_mode(); },
    };
}

void title_scene::update_play_mode(float dt) {
    title::mode_update_context context = make_mode_update_context();
    (void)apply_title_command(title::update_play_mode(context, dt));
}

bool title_scene::return_to_multiplayer_room(bool queue_selected_chart) {
    if (!multiplayer_state_.current_room.has_value() && preferred_multiplayer_room_id_.empty()) {
        return false;
    }
    if (multiplayer_state_.current_room.has_value()) {
        preferred_multiplayer_room_id_ = multiplayer_state_.current_room->id;
    }
    multiplayer_chart_pick_active_ = false;
    queue_selected_chart_on_multiplayer_return_ = queue_selected_chart;
    enter_multiplayer_mode();
    return true;
}

bool title_scene::add_selected_chart_to_multiplayer_room() {
    if (!multiplayer_chart_pick_active_) {
        return false;
    }
    return return_to_multiplayer_room(true);
}

bool title_scene::apply_title_command(const title::command& command) {
    return title::dispatch_command(
        {
            .enter_home = [this]() {
            if (!return_to_multiplayer_room(false)) {
                enter_home_mode(false);
            }
            },
            .open_update_catalog = [this](const std::string& song_id, const std::string& chart_id) {
                browse_feature_.select_local_update_target(song_id, chart_id, true);
                enter_online_mode();
            },
            .add_selected_to_multiplayer = [this]() {
                return add_selected_chart_to_multiplayer_room();
            },
            .open_self_profile = [this]() {
                play_create_feature_.state().login_dialog.open = false;
                profile_controller_.open();
            },
            .open_public_profile = [this](const std::string& user_id) {
                public_profile_controller_.open(user_id);
            },
            .open_multiplayer_song_select = [this]() {
                if (multiplayer_state_.current_room.has_value()) {
                    preferred_multiplayer_room_id_ = multiplayer_state_.current_room->id;
                }
                multiplayer_chart_pick_active_ = true;
                queue_selected_chart_on_multiplayer_return_ = false;
                enter_play_mode();
            },
        },
        command);
}

void title_scene::update_create_mode(float dt) {
    title::mode_update_context context = make_mode_update_context();
    title::update_create_mode(context, dt);
}

void title_scene::update_multiplayer_mode(float dt) {
    title::multiplayer_flow_context context{
        .multiplayer_state = multiplayer_state_,
        .play_state = play_create_feature_.state(),
        .multiplayer_local_index = multiplayer_local_index_,
        .queue_selected_chart_on_multiplayer_return = queue_selected_chart_on_multiplayer_return_,
        .audio_state = multiplayer_audio_state_,
        .audio_controller = audio_controller_,
        .browse_feature = browse_feature_,
    };
    const title::multiplayer_flow_result result = title::update_multiplayer_flow(context, dt);
    if (apply_title_command(result.title_command)) {
        return;
    }
    if (result.enter_home) {
        enter_home_mode(false);
        return;
    }
    if (result.play_request.has_value() &&
        result.play_request->song != nullptr &&
        result.play_request->chart != nullptr) {
        manager_.change_scene(song_select::make_multiplayer_play_scene(
            manager_,
            *result.play_request->song,
            *result.play_request->chart,
            result.play_request->room_id,
            result.play_request->match_id));
    }
}

void title_scene::update_online_mode(float dt) {
    title::mode_update_context context = make_mode_update_context();
    title::update_online_mode(context, dt);
}

void title_scene::update_common_animation(float dt) {
    title::common_update_context context = make_common_update_context();
    title::update_common_frame(context, dt);
}

void title_scene::update_settings_mode(float dt) {
    const title::settings_flow_result result =
        title::update_settings_flow(settings_overlay_, to_title_common_mode(settings_return_mode_), dt);
    if (!result.return_mode.has_value()) {
        return;
    }

    if (result.refresh_auth_state) {
        auth_overlay::refresh_auth_state(play_create_feature_.state().auth);
    }
    switch (*result.return_mode) {
        case title::common_mode::title:
            enter_title_mode();
            break;
        case title::common_mode::play:
            enter_play_mode();
            break;
        case title::common_mode::multiplayer:
            enter_multiplayer_mode(false);
            break;
        case title::common_mode::online:
            enter_online_mode();
            break;
        case title::common_mode::create:
            enter_create_mode();
            break;
        case title::common_mode::home:
        case title::common_mode::settings:
            enter_home_mode();
            break;
    }
}

void title_scene::update_title_quit(float dt) {
    if (mode_ == hub_mode::title && IsKeyDown(KEY_ESCAPE)) {
        esc_hold_t_ += dt;
        if (esc_hold_t_ >= 0.3f) {
            esc_hold_t_ = 0.0f;
            quitting_ = true;
            quit_fade_.restart(scene_fade::direction::out, 1.5f, 1.0f);
        }
    } else {
        esc_hold_t_ = 0.0f;
    }
}

void title_scene::start_transition(transition_target target) {
    if (transitioning_to_song_select_) {
        return;
    }
    if (target == transition_target::song_select) {
        enter_play_mode();
        return;
    }
    if (target == transition_target::multiplayer) {
        enter_multiplayer_mode();
        return;
    }
    if (target == transition_target::online_download) {
        enter_online_mode();
        return;
    }
    if (target == transition_target::create_tools) {
        enter_create_mode();
        return;
    }
    transition_target_ = target;
    transitioning_to_song_select_ = true;
    transition_fade_.restart(scene_fade::direction::out, 0.3f, 0.65f);
}

title_audio_policy::hub_mode title_scene::current_audio_mode() const {
    const hub_mode mode = content_mode_for_settings(mode_, settings_return_mode_);
    return mode == hub_mode::title ? title_audio_policy::hub_mode::title :
           mode == hub_mode::home ? title_audio_policy::hub_mode::home :
           mode == hub_mode::play ? title_audio_policy::hub_mode::play :
           mode == hub_mode::multiplayer ? title_audio_policy::hub_mode::home :
           mode == hub_mode::online ? title_audio_policy::hub_mode::online :
           mode == hub_mode::create ? title_audio_policy::hub_mode::create :
                                  title_audio_policy::hub_mode::home;
}

void title_scene::on_enter() {
    audio_controller_.configure(kTitleIntroPath, kTitleLoopPath);
    audio_controller_.on_enter();
    play_create_feature_.reset();
    catalog_reload_coordinator_.reset();
    auth_overlay::refresh_auth_state(play_create_feature_.state().auth);
    profile_controller_.reset();
    public_profile_controller_.reset();
    play_create_feature_.state().recent_result_offset = recent_result_offset_;
    if (play_intro_fade_) {
        intro_fade_.restart(scene_fade::direction::in, 1.0f, 1.0f);
        intro_hold_t_ = 0.5f;
    } else {
        intro_fade_.restart(scene_fade::direction::in, 0.0f, 0.0f);
        intro_hold_t_ = 0.0f;
    }
    const hub_mode requested_mode = start_in_multiplayer_view_ ? hub_mode::multiplayer
        : (start_in_create_view_ ? hub_mode::create
        : (start_in_play_view_ ? hub_mode::play : (start_with_home_open_ ? hub_mode::home : hub_mode::title)));
    mode_ = start_in_settings_view_ ? hub_mode::settings : requested_mode;
    if (requested_mode == hub_mode::play) {
        play_create_feature_.on_enter_play(
            multiplayer_chart_pick_active_,
            server_environment::normalize_url(multiplayer_state_.auth.server_url),
            audio_controller_);
    } else if (requested_mode == hub_mode::create) {
        play_create_feature_.on_enter_create(audio_controller_);
    } else if (requested_mode == hub_mode::online) {
        browse_feature_.on_enter(audio_controller_);
    }
    if (requested_mode == hub_mode::multiplayer) {
        multiplayer::on_enter(multiplayer_state_, preferred_multiplayer_room_id_);
        preferred_multiplayer_room_id_.clear();
    }
    suppress_home_pointer_until_release_ = false;
    settings_return_mode_ = start_in_settings_view_ ? requested_mode : hub_mode::home;
    home_menu_anim_ = requested_mode == hub_mode::title ? 0.0f : 1.0f;
    home_menu_selected_index_ = 0;
    home_status_message_.clear();
    play_view_anim_ = (requested_mode == hub_mode::play || requested_mode == hub_mode::multiplayer ||
                       requested_mode == hub_mode::online || requested_mode == hub_mode::create) ? 1.0f : 0.0f;
    play_entry_origin_rect_ = {};
    settings_overlay_.open();
    play_create_feature_.state().login_dialog.open = false;
    title_startup_controller::reset(startup_);
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::on_exit() {
    if (mode_ == hub_mode::settings) {
        settings_overlay_.save();
    }
    play_create_feature_.state().login_dialog.open = false;
    profile_controller_.close();
    public_profile_controller_.close();
    play_create_feature_.on_exit();
    browse_feature_.on_exit();
    audio_controller_.on_exit();
}

void title_scene::on_app_exit() {
    if (mode_ == hub_mode::settings) {
        settings_overlay_.save();
    }
    multiplayer::leave_current_room_best_effort(multiplayer_state_);
}

// Title 上で Home 展開、Play/Create への遷移、Account 導線を扱う。
void title_scene::update(float dt) {
    ui::begin_hit_regions();
    if (play_create_feature_.state().context_menu.open) {
        ui::register_hit_region(play_create_feature_.state().context_menu.rect, song_select::layout::kContextMenuLayer);
    }
    if (play_create_feature_.state().confirmation_dialog.open) {
        ui::register_hit_region(song_select::layout::kConfirmDialogRect, song_select::layout::kModalLayer);
    }
    if (profile_controller_.is_open()) {
        ui::register_hit_region(profile_controller_.bounds(), kTitleModalLayer);
    }
    if (public_profile_controller_.is_open()) {
        ui::register_hit_region(public_profile_controller_.bounds(), kTitleModalLayer);
    }
    update_common_animation(dt);
    update_startup_loading();

    if (transitioning_to_song_select_) {
        transition_fade_.update(dt);
        if (transition_fade_.complete()) {
            switch (transition_target_) {
            case transition_target::song_select:
                enter_play_mode();
                break;
            case transition_target::multiplayer:
                enter_multiplayer_mode();
                break;
            case transition_target::online_download:
                enter_online_mode();
                break;
            case transition_target::create_tools:
                enter_create_mode();
                break;
            }
        }
        return;
    }

    if (quitting_) {
        quit_fade_.update(dt);
        if (quit_fade_.complete()) {
            manager_.request_exit();
        }
        return;
    }

    if (play_create_feature_.state().confirmation_dialog.open && IsKeyPressed(KEY_ESCAPE)) {
        play_create_feature_.cancel_confirmation();
        return;
    }

    if (play_create_feature_.busy()) {
        return;
    }

    if (startup_.loading) {
        update_title_quit(dt);
        return;
    }

    friends_controller_.tick(dt);
    friends_controller_.poll();
    if (const auto room_join = friends_controller_.consume_room_join_request(); room_join.has_value()) {
        enter_multiplayer_room_invite(room_join->room_id, room_join->invite_id);
        return;
    }

    if (handle_friends_input()) {
        return;
    }

    if (handle_profile_input()) {
        return;
    }

    if (handle_public_profile_input()) {
        return;
    }

    title::frame_input_context input_context{
        .mode = to_title_common_mode(mode_),
        .home_menu_anim = home_menu_anim_,
        .home_menu_selected_index = home_menu_selected_index_,
        .home_status_message = home_status_message_,
        .suppress_home_pointer_until_release = suppress_home_pointer_until_release_,
        .play_create_feature = play_create_feature_,
        .browse_feature = browse_feature_,
        .catalog_reload_coordinator = catalog_reload_coordinator_,
        .friends_controller = friends_controller_,
        .auth_controller = auth_controller_,
    };
    const title::frame_input_result input_result = title::update_frame_input(input_context);
    if (input_result.enter_settings) {
        enter_settings_mode();
        return;
    }
    if (input_result.enter_home) {
        enter_home_mode(input_result.enter_home_suppress_pointer);
        return;
    }
    if (input_result.enter_title) {
        enter_title_mode();
        return;
    }
    if (input_result.selected_home_action.has_value()) {
        start_transition(transition_target_for_home_action(*input_result.selected_home_action));
        return;
    }
    if (input_result.consumed) {
        return;
    }

    if (mode_ == hub_mode::play) {
        update_play_mode(dt);
        return;
    }
    if (mode_ == hub_mode::multiplayer) {
        update_multiplayer_mode(dt);
        return;
    }
    if (mode_ == hub_mode::online) {
        update_online_mode(dt);
        return;
    }
    if (mode_ == hub_mode::create) {
        update_create_mode(dt);
        return;
    }
    if (mode_ == hub_mode::settings) {
        update_settings_mode(dt);
        return;
    }

    update_title_quit(dt);
}

// タイトルと、そこから展開する Home 導線を描画する。
void title_scene::draw() {
    const title_play_create_feature::cross_callbacks cross_callbacks = play_cross_callbacks();
    const title_hub_view::draw_result result = title_hub_view::draw({
        {
            to_title_hub_view_mode(mode_),
            transitioning_to_song_select_,
            quitting_,
            intro_hold_t_ > 0.0f,
            home_menu_anim_,
            play_view_anim_,
            home_menu_selected_index_,
            home_status_message_,
            play_entry_origin_rect_,
        },
        play_create_feature_,
        multiplayer_state_,
        browse_feature_,
        startup_,
        audio_controller_,
        settings_overlay_,
        friends_controller_,
        profile_controller_,
        public_profile_controller_,
        auth_controller_,
        cross_callbacks,
        intro_fade_,
        transition_fade_,
        quit_fade_,
    });
    if (result.close_login_dialog) {
        play_create_feature_.state().login_dialog.open = false;
    } else if (apply_title_command(result.title_command)) {
        return;
    } else if (result.login_command != song_select::login_dialog_command::none) {
        auth_overlay::start_request(auth_controller_, play_create_feature_.state().login_dialog, result.login_command);
    }
}

