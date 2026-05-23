#include "title_scene.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "multiplayer/multiplayer_controller.h"
#include "network/server_environment.h"
#include "services/online_content_availability.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_last_played.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_navigation.h"
#include "tween.h"
#include "title/home_menu_view.h"
#include "title/local_content_index.h"
#include "title/title_home_input_controller.h"
#include "title/title_hub_view.h"
#include "title/title_layout.h"
#include "title/title_startup_controller.h"
#include "theme.h"
#include "ui_notice.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"
#include "platform/window_chrome.h"

namespace {

constexpr const char* kTitleIntroPath = "assets/audio/title_intro.mp3";
constexpr const char* kTitleLoopPath = "assets/audio/title_loop.mp3";
constexpr float kHomeAnimSpeed = 6.5f;
constexpr float kAccountChipInteractiveThreshold = 0.2f;
constexpr float kPlayViewAnimSpeed = 6.0f;
constexpr ui::draw_layer kTitleModalLayer = ui::draw_layer::modal;
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

bool select_local_song(song_select::state& state, const std::string& song_id) {
    if (song_id.empty()) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        if (state.songs[static_cast<size_t>(i)].song.meta.song_id != song_id) {
            continue;
        }

        song_select::apply_song_selection(state, i, 0);
        return true;
    }

    return false;
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
                          bool start_in_multiplayer_view) :
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
    settings_overlay_(g_settings) {
}

struct local_chart_match {
    const song_select::song_entry* song = nullptr;
    const song_select::chart_option* chart = nullptr;
};

local_chart_match find_online_chart_match(const song_select::state& state,
                                          const std::string& server_url,
                                          const std::string& remote_song_id,
                                          const std::string& remote_chart_id,
                                          int remote_chart_version = 0) {
    const local_content_index::snapshot index = local_content_index::load_snapshot();
    const online_content_availability::resolved_song song =
        online_content_availability::resolve_song(
            state.songs,
            index,
            {
                .server_url = server_environment::normalize_url(server_url),
                .remote_song_id = remote_song_id,
            },
            content_status::community);
    const online_content_availability::resolved_chart chart =
        online_content_availability::resolve_chart(
            state.songs,
            index,
            song,
            {
                .server_url = server_environment::normalize_url(server_url),
                .remote_song_id = remote_song_id,
                .remote_chart_id = remote_chart_id,
                .remote_chart_version = remote_chart_version,
            },
            content_status::community);
    return {song.local_song, chart.local_chart};
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
        audio_controller_.preview());
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::enter_multiplayer_mode(bool reset_room_state) {
    mode_ = hub_mode::multiplayer;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    if (reset_room_state) {
        multiplayer::on_enter(multiplayer_state_, preferred_multiplayer_room_id_);
        preferred_multiplayer_room_id_.clear();
    }
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_create_feature_.state()), 0.0f);
}

void title_scene::enter_online_mode() {
    mode_ = hub_mode::online;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    browse_feature_.on_enter(audio_controller_.preview());
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::enter_create_mode() {
    mode_ = hub_mode::create;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    play_create_feature_.on_enter_create(audio_controller_.preview());
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_create_feature_.state(), browse_feature_.state()), 0.0f);
}

void title_scene::enter_settings_mode() {
    settings_return_mode_ = mode_ == hub_mode::settings ? settings_return_mode_ : mode_;
    mode_ = hub_mode::settings;
    home_status_message_.clear();
    play_create_feature_.state().login_dialog.open = false;
    profile_controller_.close();
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
        mode_ == hub_mode::play || mode_ == hub_mode::create,
        home_status_message_,
        [this](std::string song_id, std::string chart_id, bool sync_media, bool calculate_missing_levels) {
            play_create_feature_.request_catalog_reload(
                std::move(song_id), std::move(chart_id), sync_media, calculate_missing_levels);
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

title_play_create_feature::cross_callbacks title_scene::play_cross_callbacks() {
    return {
        .stop_preview = [this]() {
            audio_controller_.preview().stop();
        },
        .mark_online_song_removed = [this](const std::string& song_id) {
            browse_feature_.mark_song_removed(song_id);
        },
        .reload_online_catalog = [this]() {
            browse_feature_.request_reload();
        },
    };
}

void title_scene::update_play_mode(float dt) {
    play_create_feature_.update_play(
        manager_,
        audio_controller_.preview(),
        play_view_anim_,
        play_entry_origin_rect_,
        dt,
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
        });
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

void title_scene::update_create_mode(float dt) {
    play_create_feature_.update_create(
        manager_,
        audio_controller_.preview(),
        play_view_anim_,
        play_entry_origin_rect_,
        dt,
        play_cross_callbacks(),
        {
            .enter_home = [this]() { enter_home_mode(false); },
        });
}

void title_scene::update_multiplayer_mode(float dt) {
    const song_select::song_entry* song = song_select::selected_song(play_create_feature_.state());
    const auto filtered = song_select::filtered_charts_for_selected_song(play_create_feature_.state());
    const song_select::chart_option* chart = song_select::selected_chart_for(play_create_feature_.state(), filtered);
    const std::string room_server_url = server_environment::normalize_url(multiplayer_state_.auth.server_url);
    multiplayer_state_.queue_candidate_available = chart != nullptr &&
        online_content::is_queueable(chart->online_identity) &&
        server_environment::normalize_url(chart->online_identity->server_url) == room_server_url;
    if (song != nullptr && chart != nullptr) {
        multiplayer_state_.queue_candidate_song_title = song->song.meta.title;
        multiplayer_state_.queue_candidate_chart_name = chart->meta.difficulty;
        if (chart->online_identity.has_value()) {
            multiplayer_state_.queue_candidate_remote_song_id = chart->online_identity->remote_song_id;
            multiplayer_state_.queue_candidate_remote_chart_id = chart->online_identity->remote_chart_id;
            multiplayer_state_.queue_candidate_remote_chart_version = chart->online_identity->remote_chart_version;
            multiplayer_state_.queue_candidate_message = multiplayer_state_.queue_candidate_available
                ? "Selected chart can be queued."
                : (server_environment::normalize_url(chart->online_identity->server_url) == room_server_url
                    ? "Selected chart is missing online identity."
                    : "Selected chart belongs to another server.");
        } else {
            multiplayer_state_.queue_candidate_remote_song_id.clear();
            multiplayer_state_.queue_candidate_remote_chart_id.clear();
            multiplayer_state_.queue_candidate_remote_chart_version = 0;
            multiplayer_state_.queue_candidate_message = "Selected chart is local-only.";
        }
    } else {
        multiplayer_state_.queue_candidate_song_title.clear();
        multiplayer_state_.queue_candidate_chart_name.clear();
        multiplayer_state_.queue_candidate_remote_song_id.clear();
        multiplayer_state_.queue_candidate_remote_chart_id.clear();
        multiplayer_state_.queue_candidate_remote_chart_version = 0;
        multiplayer_state_.queue_candidate_message = "Select an online chart from Play.";
    }
    multiplayer_state_.current_queue_chart_installed = false;
    multiplayer_state_.installed_queue_item_ids.clear();
    if (multiplayer_state_.current_room.has_value()) {
        for (const multiplayer::room_queue_item& item : multiplayer_state_.current_room->queue) {
            const local_chart_match match =
                find_online_chart_match(play_create_feature_.state(), room_server_url, item.song_id, item.chart_id);
            const bool installed = match.song != nullptr && match.chart != nullptr;
            if (installed) {
                multiplayer_state_.installed_queue_item_ids.push_back(item.id);
            }
            if (&item == &multiplayer_state_.current_room->queue.front()) {
                multiplayer_state_.current_queue_chart_installed = installed;
            }
        }
    }
    if (queue_selected_chart_on_multiplayer_return_ && multiplayer_state_.current_room.has_value()) {
        queue_selected_chart_on_multiplayer_return_ = false;
        if (multiplayer_state_.queue_candidate_available) {
            multiplayer_state_.status_message = "Adding selected chart...";
            multiplayer_state_.command = multiplayer::ui_command::add_selected_chart;
        } else {
            multiplayer_state_.status_message = multiplayer_state_.queue_candidate_message;
        }
    }
    const multiplayer::update_result result = multiplayer::update(multiplayer_state_, dt);
    if (result.back_requested) {
        enter_home_mode(false);
        return;
    }
    if (result.open_song_select_requested) {
        if (multiplayer_state_.current_room.has_value()) {
            preferred_multiplayer_room_id_ = multiplayer_state_.current_room->id;
        }
        multiplayer_chart_pick_active_ = true;
        queue_selected_chart_on_multiplayer_return_ = false;
        enter_play_mode();
        return;
    }
    if (multiplayer_state_.current_queue_download_requested) {
        multiplayer_state_.current_queue_download_requested = false;
        if (!multiplayer_state_.requested_download_song_id.empty() &&
            !multiplayer_state_.requested_download_chart_id.empty()) {
            browse_feature_.start_chart_download_by_remote_id(
                multiplayer_state_.requested_download_song_id,
                multiplayer_state_.requested_download_chart_id);
            multiplayer_state_.requested_download_song_id.clear();
            multiplayer_state_.requested_download_chart_id.clear();
            multiplayer_state_.status_message = "Downloading queued chart...";
        }
    }
    if (multiplayer_state_.start_play_requested) {
        multiplayer_state_.start_play_requested = false;
        const local_chart_match match =
            find_online_chart_match(play_create_feature_.state(),
                                    room_server_url,
                                    multiplayer_state_.requested_start_song_id,
                                    multiplayer_state_.requested_start_chart_id);
        if (match.song == nullptr || match.chart == nullptr) {
            multiplayer_state_.status_message = "Queued chart is not installed. Download it before readying up.";
            multiplayer_state_.local_ready = false;
            return;
        }
        audio_controller_.preview().stop();
        manager_.change_scene(song_select::make_multiplayer_play_scene(
            manager_,
            *match.song,
            *match.chart,
            multiplayer_state_.selected_room_id,
            multiplayer_state_.active_match_id));
    }
}

void title_scene::update_online_mode(float dt) {
    browse_feature_.update(
        play_view_anim_,
        play_entry_origin_rect_,
        dt,
        {
            .online = {
                .enter_home = [this]() { enter_home_mode(false); },
                .select_preview_song = [this]() {
                    audio_controller_.preview().select_song(browse_feature_.preview_song());
                },
                .resume_preview = [this]() {
                    audio_controller_.preview().resume(browse_feature_.preview_song());
                },
                .pause_preview = [this]() {
                    audio_controller_.preview().pause();
                },
                .open_local_selection = [this]() {
                    preferred_song_id_ = browse_feature_.selected_song_id();
                    preferred_chart_id_.clear();
                    if (!select_local_song(play_create_feature_.state(), preferred_song_id_)) {
                        play_create_feature_.request_catalog_reload(preferred_song_id_, preferred_chart_id_, true);
                    }
                    enter_play_mode();
                },
            },
        });
}

void title_scene::update_common_animation(float dt) {
    const hub_mode content_mode = content_mode_for_settings(mode_, settings_return_mode_);
    const bool content_mode_is_play_or_create = content_mode == hub_mode::play || content_mode == hub_mode::create;

    auth_overlay::poll_restore(auth_controller_,
                               play_create_feature_.state().auth,
                               play_create_feature_.state().login_dialog);
    auth_overlay::poll_request(auth_controller_,
                               play_create_feature_.state().auth,
                               play_create_feature_.state().login_dialog);
    play_create_feature_.poll_catalog_reload(
        audio_controller_.preview(), mode_ == hub_mode::play, mode_ == hub_mode::create);
    play_create_feature_.poll_transfer(play_cross_callbacks(), content_mode_is_play_or_create);
    play_create_feature_.poll_ranking_reload();
    play_create_feature_.poll_scoring_ruleset_warm();
    if (play_create_feature_.poll_create_upload(content_mode_is_play_or_create)) {
        browse_feature_.request_reload(true);
    }

    if (profile_controller_.poll().content_changed) {
        auth_overlay::refresh_auth_state(play_create_feature_.state().auth);
        browse_feature_.request_reload();
        play_create_feature_.request_catalog_reload("", "", content_mode_is_play_or_create);
    }

    profile_controller_.close_if_logged_out(play_create_feature_.state().auth.logged_in);

    const title_browse_feature::poll_result browse_poll =
        browse_feature_.poll(content_mode == hub_mode::online);
    if (browse_poll.downloaded_content) {
        preferred_song_id_ = browse_poll.downloaded_song_id;
        preferred_chart_id_.clear();
        play_create_feature_.request_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                                    content_mode_is_play_or_create,
                                                    true);
    }
    if (browse_poll.select_preview_song) {
        audio_controller_.preview().select_song(browse_feature_.preview_song());
    }

    if (intro_hold_t_ > 0.0f) {
        intro_hold_t_ = std::max(0.0f, intro_hold_t_ - dt);
    } else {
        intro_fade_.update(dt);
    }

    if (play_create_feature_.state().login_dialog.open) {
        play_create_feature_.state().login_dialog.open_anim = tween::advance(play_create_feature_.state().login_dialog.open_anim, dt, 8.0f);
    } else {
        play_create_feature_.state().login_dialog.open_anim = 0.0f;
    }

    profile_controller_.tick(dt);

    settings_overlay_.update_animation(mode_ == hub_mode::settings, dt);

    const float target_anim = content_mode == hub_mode::title ? 0.0f : 1.0f;
    home_menu_anim_ = tween::damp(home_menu_anim_, target_anim, dt, kHomeAnimSpeed, 0.002f);

    const float target_play_anim =
        (content_mode == hub_mode::play || content_mode == hub_mode::multiplayer ||
         content_mode == hub_mode::online || content_mode == hub_mode::create)
            ? 1.0f
            : 0.0f;
    play_view_anim_ = tween::damp(play_view_anim_, target_play_anim, dt, kPlayViewAnimSpeed, 0.002f);

    if (play_view_anim_ > 0.0f && (content_mode == hub_mode::play || content_mode == hub_mode::create)) {
        song_select::tick_animations(play_create_feature_.state(), dt);
    }
    audio_controller_.update(current_audio_mode(), selected_audio_song(content_mode, play_create_feature_.state(), browse_feature_.state()), dt);
}

bool title_scene::handle_account_input() {
    if (mode_ == hub_mode::settings) {
        return false;
    }
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    if (home_menu_anim_ < kAccountChipInteractiveThreshold || !ui::is_clicked(account_chip_rect)) {
        return false;
    }
    if (play_create_feature_.state().login_dialog.open) {
        play_create_feature_.state().login_dialog.open = false;
    } else {
        song_select::open_login_dialog(play_create_feature_.state().login_dialog, auth::load_session_summary());
        auth_overlay::refresh_auth_state(play_create_feature_.state().auth);
    }
    return true;
}

bool title_scene::handle_settings_button_input() {
    if (mode_ == hub_mode::settings || home_menu_anim_ < kAccountChipInteractiveThreshold) {
        return false;
    }
    if (!ui::is_clicked(title_layout::settings_chip_rect())) {
        return false;
    }
    enter_settings_mode();
    return true;
}

bool title_scene::handle_refresh_button_input() {
    if (mode_ == hub_mode::settings || home_menu_anim_ < kAccountChipInteractiveThreshold) {
        return false;
    }
    if (!ui::is_clicked(title_layout::refresh_chip_rect())) {
        return false;
    }

    play_create_feature_.capture_current_selection();
    browse_feature_.request_reload(true);
    play_create_feature_.request_catalog_reload(play_create_feature_.preferred_song_id(),
                                                play_create_feature_.preferred_chart_id(),
                                                mode_ == hub_mode::play || mode_ == hub_mode::create,
                                                true);
    ui::notify("Refreshing catalog...", ui::notice_tone::info, 1.8f);
    return true;
}

bool title_scene::handle_login_dialog_input() {
    if (!play_create_feature_.state().login_dialog.open) {
        return false;
    }
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
        !auth_controller_.request_active) {
        play_create_feature_.state().login_dialog.open = false;
    }
    return true;
}

bool title_scene::update_home_pointer_suppression() {
    if (!suppress_home_pointer_until_release_) {
        return false;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        return true;
    }

    suppress_home_pointer_until_release_ = false;
    return true;
}

bool title_scene::handle_title_input(bool left_click_for_home, bool right_click_for_home) {
    if (mode_ != hub_mode::title) {
        return false;
    }
    if (IsKeyPressed(KEY_ENTER) || left_click_for_home || right_click_for_home) {
        enter_home_mode(left_click_for_home || right_click_for_home);
        return true;
    }
    return false;
}

bool title_scene::handle_home_input(bool suppress_pointer_this_frame) {
    if (mode_ == hub_mode::title) {
        return false;
    }
    if (mode_ == hub_mode::play || mode_ == hub_mode::create) {
        return false;
    }

    const title_home_input_controller::result result =
        title_home_input_controller::update(home_menu_selected_index_,
                                            home_status_message_,
                                            home_menu_anim_,
                                            suppress_pointer_this_frame);
    if (result.enter_title) {
        enter_title_mode();
        return true;
    }
    if (result.selected_action.has_value()) {
        start_transition(transition_target_for_home_action(*result.selected_action));
        return true;
    }
    return result.consumed;
}

void title_scene::update_settings_mode(float dt) {
    if (settings_overlay_.closing()) {
        if (settings_overlay_.closed()) {
            auth_overlay::refresh_auth_state(play_create_feature_.state().auth);
            const hub_mode return_mode = settings_return_mode_;
            switch (return_mode) {
                case hub_mode::title:
                    enter_title_mode();
                    break;
                case hub_mode::play:
                    enter_play_mode();
                    break;
                case hub_mode::multiplayer:
                    enter_multiplayer_mode(false);
                    break;
                case hub_mode::online:
                    enter_online_mode();
                    break;
                case hub_mode::create:
                    enter_create_mode();
                    break;
                case hub_mode::home:
                case hub_mode::settings:
                    enter_home_mode();
                    break;
            }
        }
        return;
    }

    settings_overlay_.update(dt);
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
    auth_overlay::refresh_auth_state(play_create_feature_.state().auth);
    profile_controller_.reset();
    play_create_feature_.state().recent_result_offset = recent_result_offset_;
    if (play_intro_fade_) {
        intro_fade_.restart(scene_fade::direction::in, 1.0f, 1.0f);
        intro_hold_t_ = 0.5f;
    } else {
        intro_fade_.restart(scene_fade::direction::in, 0.0f, 0.0f);
        intro_hold_t_ = 0.0f;
    }
    mode_ = start_in_multiplayer_view_ ? hub_mode::multiplayer
        : (start_in_create_view_ ? hub_mode::create
        : (start_in_play_view_ ? hub_mode::play : (start_with_home_open_ ? hub_mode::home : hub_mode::title)));
    if (mode_ == hub_mode::play) {
        play_create_feature_.on_enter_play(
            multiplayer_chart_pick_active_,
            server_environment::normalize_url(multiplayer_state_.auth.server_url),
            audio_controller_.preview());
    } else if (mode_ == hub_mode::create) {
        play_create_feature_.on_enter_create(audio_controller_.preview());
    } else if (mode_ == hub_mode::online) {
        browse_feature_.on_enter(audio_controller_.preview());
    }
    if (mode_ == hub_mode::multiplayer) {
        multiplayer::on_enter(multiplayer_state_, preferred_multiplayer_room_id_);
        preferred_multiplayer_room_id_.clear();
    }
    suppress_home_pointer_until_release_ = false;
    settings_return_mode_ = hub_mode::home;
    home_menu_anim_ = mode_ == hub_mode::title ? 0.0f : 1.0f;
    home_menu_selected_index_ = 0;
    home_status_message_.clear();
    play_view_anim_ = (mode_ == hub_mode::play || mode_ == hub_mode::multiplayer ||
                       mode_ == hub_mode::online || mode_ == hub_mode::create) ? 1.0f : 0.0f;
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

    if (handle_profile_input()) {
        return;
    }

    const bool suppress_home_pointer_this_frame = update_home_pointer_suppression();

    if (!suppress_home_pointer_this_frame && handle_account_input()) {
        return;
    }

    if (handle_login_dialog_input()) {
        return;
    }

    if (!suppress_home_pointer_this_frame && handle_refresh_button_input()) {
        return;
    }

    if (!suppress_home_pointer_this_frame && handle_settings_button_input()) {
        return;
    }

    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    const Rectangle refresh_chip_rect = title_layout::refresh_chip_rect();
    const Rectangle settings_chip_rect = title_layout::settings_chip_rect();
    const bool account_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(account_chip_rect);
    const bool refresh_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(refresh_chip_rect);
    const bool settings_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(settings_chip_rect);
    const bool left_click_for_home =
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !window_chrome::is_pointer_over_chrome() &&
        !account_hovered &&
        !refresh_hovered &&
        !settings_hovered;
    const bool right_click_for_home = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (handle_title_input(left_click_for_home, right_click_for_home)) {
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

    if (handle_home_input(suppress_home_pointer_this_frame)) {
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
        profile_controller_,
        auth_controller_,
        cross_callbacks,
        mode_ == hub_mode::play || mode_ == hub_mode::create,
        intro_fade_,
        transition_fade_,
        quit_fade_,
    });
    if (result.close_login_dialog) {
        play_create_feature_.state().login_dialog.open = false;
    } else if (result.open_profile) {
        play_create_feature_.state().login_dialog.open = false;
        profile_controller_.open();
    } else if (result.login_command != song_select::login_dialog_command::none) {
        auth_overlay::start_request(auth_controller_, play_create_feature_.state().login_dialog, result.login_command);
    }
}

