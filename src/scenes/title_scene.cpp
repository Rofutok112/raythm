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
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_last_played.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "tween.h"
#include "title/home_menu_view.h"
#include "title/online_download_view.h"
#include "title/play_session_controller.h"
#include "title/title_create_mode_controller.h"
#include "title/title_header_view.h"
#include "title/title_home_input_controller.h"
#include "title/title_layout.h"
#include "title/title_online_mode_controller.h"
#include "title/title_play_mode_controller.h"
#include "title/seamless_song_select_view.h"
#include "theme.h"
#include "ui_notice.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr const char* kTitleIntroPath = "assets/audio/title_intro.mp3";
constexpr const char* kTitleLoopPath = "assets/audio/title_loop.mp3";
constexpr float kHomeAnimSpeed = 6.5f;
constexpr float kAccountChipInteractiveThreshold = 0.2f;
constexpr float kPlayViewAnimSpeed = 6.0f;
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
    return action == title_home_view::action::create
        ? title_scene::transition_target::create_tools
        : action == title_home_view::action::online
            ? title_scene::transition_target::online_download
            : title_scene::transition_target::song_select;
}

bool consume_startup_level_calculation() {
    static bool consumed = false;
    if (consumed) {
        return false;
    }
    consumed = true;
    return true;
}

}  // namespace

title_scene::title_scene(scene_manager& manager,
                         bool start_with_home_open,
                         bool play_intro_fade,
                         std::string preferred_song_id,
                         std::string preferred_chart_id,
                         std::optional<song_select::recent_result_offset> recent_result_offset,
                         bool start_in_play_view,
                         bool start_in_create_view) :
    scene(manager),
    start_with_home_open_(start_with_home_open),
    play_intro_fade_(play_intro_fade),
    preferred_song_id_(std::move(preferred_song_id)),
    preferred_chart_id_(std::move(preferred_chart_id)),
    recent_result_offset_(std::move(recent_result_offset)),
    start_in_play_view_(start_in_play_view),
    start_in_create_view_(start_in_create_view),
    settings_overlay_(g_settings) {
}

void title_scene::enter_title_mode() {
    mode_ = hub_mode::title;
    suppress_home_pointer_until_release_ = false;
    home_status_message_.clear();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_state_), 0.0f);
}

void title_scene::enter_home_mode(bool suppress_pointer) {
    mode_ = hub_mode::home;
    suppress_home_pointer_until_release_ = suppress_pointer;
    home_status_message_.clear();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_state_), 0.0f);
}

void title_scene::enter_play_mode() {
    mode_ = hub_mode::play;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    sync_play_media();
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_state_, online_state_), 0.0f);
}

void title_scene::enter_online_mode() {
    mode_ = hub_mode::online;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    title_online_view::on_enter(online_state_, audio_controller_.preview());
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_state_, online_state_), 0.0f);
}

void title_scene::enter_create_mode() {
    mode_ = hub_mode::create;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    title_play_session::sync_preview(play_state_, audio_controller_.preview());
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_state_, online_state_), 0.0f);
}

void title_scene::enter_settings_mode() {
    settings_return_mode_ = mode_ == hub_mode::settings ? settings_return_mode_ : mode_;
    mode_ = hub_mode::settings;
    home_status_message_.clear();
    play_state_.login_dialog.open = false;
    profile_controller_.close();
    settings_overlay_.open();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_state_), 0.0f);
}

void title_scene::close_settings_mode() {
    settings_overlay_.request_close();
}

void title_scene::request_play_catalog_reload(std::string preferred_song_id,
                                              std::string preferred_chart_id,
                                              bool sync_media_on_apply,
                                              bool calculate_missing_levels) {
    play_data_controller_.request_catalog_reload(play_state_, std::move(preferred_song_id),
                                                 std::move(preferred_chart_id),
                                                 sync_media_on_apply, calculate_missing_levels);
}

void title_scene::poll_play_catalog_reload() {
    const title_play_data_controller::catalog_poll_result result =
        play_data_controller_.poll_catalog_reload(play_state_, mode_ == hub_mode::play, mode_ == hub_mode::create);
    if (result.sync_play_media) {
        sync_play_media();
    } else if (result.sync_create_preview) {
        title_play_session::sync_preview(play_state_, audio_controller_.preview());
    }
}

void title_scene::capture_current_play_selection() {
    const song_select::song_entry* song = song_select::selected_song(play_state_);
    if (song == nullptr) {
        return;
    }

    preferred_song_id_ = song->song.meta.song_id;
    const auto filtered = song_select::filtered_charts_for_selected_song(play_state_);
    if (const song_select::chart_option* chart = song_select::selected_chart_for(play_state_, filtered)) {
        preferred_chart_id_ = chart->meta.chart_id;
    } else {
        preferred_chart_id_.clear();
    }
}

void title_scene::sync_play_media() {
    title_play_session::sync_preview(play_state_, audio_controller_.preview());
    play_data_controller_.request_ranking_reload(play_state_);
}

void title_scene::request_play_ranking_reload() {
    play_data_controller_.request_ranking_reload(play_state_);
}

void title_scene::poll_play_ranking_reload() {
    play_data_controller_.poll_ranking_reload(play_state_);
}

void title_scene::request_scoring_ruleset_warm(bool force_refresh) {
    play_data_controller_.request_scoring_ruleset_warm(force_refresh);
}

void title_scene::poll_scoring_ruleset_warm() {
    play_data_controller_.poll_scoring_ruleset_warm();
}

void title_scene::start_song_upload(const song_select::song_entry& song) {
    play_data_controller_.start_song_upload(song);
}

void title_scene::start_chart_upload(const song_select::song_entry& song,
                                     const song_select::chart_option& chart) {
    play_data_controller_.start_chart_upload(song, chart);
}

void title_scene::poll_create_upload() {
    if (play_data_controller_.poll_create_upload(play_state_).refresh_catalog) {
        capture_current_play_selection();
        title_online_view::reload_catalog(online_state_, true);
        request_play_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                    mode_ == hub_mode::play || mode_ == hub_mode::create,
                                    true);
    }
}

bool title_scene::handle_profile_input() {
    const title_profile_controller::input_result result =
        profile_controller_.handle_input(auth_controller_.request_active);
    if (result.delete_account_password.has_value()) {
        play_state_.login_dialog.password_input.value = *result.delete_account_password;
        auth_overlay::start_request(auth_controller_, play_state_.login_dialog,
                                    song_select::login_dialog_command::request_delete_account);
    }
    return result.consumed;
}

title_play_transfer_controller::catalog_callbacks title_scene::play_transfer_callbacks() {
    return {
        .set_preferred_selection = [this](const std::string& song_id, const std::string& chart_id) {
            preferred_song_id_ = song_id;
            preferred_chart_id_ = chart_id;
        },
        .stop_preview = [this]() {
            audio_controller_.preview().stop();
        },
        .mark_online_song_removed = [this](const std::string& song_id) {
            title_online_view::mark_song_removed(online_state_, song_id);
        },
        .reload_online_catalog = [this]() {
            title_online_view::reload_catalog(online_state_);
        },
        .request_play_catalog_reload =
            [this](const std::string& song_id, const std::string& chart_id, bool sync_media_on_apply) {
                request_play_catalog_reload(song_id, chart_id, sync_media_on_apply);
            },
    };
}

void title_scene::update_play_mode(float dt) {
    title_play_mode_controller::update(
        manager_,
        play_state_,
        audio_controller_.preview(),
        play_transfer_controller_,
        play_view_anim_,
        play_entry_origin_rect_,
        dt,
        {
            .enter_home = [this]() { enter_home_mode(false); },
            .sync_media = [this]() { sync_play_media(); },
            .request_ranking_reload = [this]() { request_play_ranking_reload(); },
        });
}

void title_scene::update_create_mode(float dt) {
    title_create_mode_controller::update(
        manager_,
        play_state_,
        play_transfer_controller_,
        play_view_anim_,
        play_entry_origin_rect_,
        dt,
        {
            .enter_home = [this]() { enter_home_mode(false); },
            .sync_preview = [this]() {
                title_play_session::sync_preview(play_state_, audio_controller_.preview());
            },
            .start_song_upload = [this](const song_select::song_entry& song) {
                start_song_upload(song);
            },
            .start_chart_upload = [this](const song_select::song_entry& song,
                                         const song_select::chart_option& chart) {
                start_chart_upload(song, chart);
            },
            .transfer_callbacks = [this]() {
                return play_transfer_callbacks();
            },
            .sync_media_on_transfer = [this]() {
                return mode_ == hub_mode::play || mode_ == hub_mode::create;
            },
            .upload_in_progress = [this]() {
                return play_data_controller_.upload_in_progress();
            },
        });
}

void title_scene::update_online_mode(float dt) {
    title_online_mode_controller::update(
        online_state_,
        play_view_anim_,
        play_entry_origin_rect_,
        dt,
        {
            .enter_home = [this]() { enter_home_mode(false); },
            .select_preview_song = [this]() {
                audio_controller_.preview().select_song(title_online_view::preview_song(online_state_));
            },
            .resume_preview = [this]() {
                audio_controller_.preview().resume(title_online_view::preview_song(online_state_));
            },
            .pause_preview = [this]() {
                audio_controller_.preview().pause();
            },
            .open_local_selection = [this]() {
                preferred_song_id_ = title_online_view::selected_song_id(online_state_);
                preferred_chart_id_.clear();
                if (!select_local_song(play_state_, preferred_song_id_)) {
                    request_play_catalog_reload(preferred_song_id_, preferred_chart_id_, true);
                }
                enter_play_mode();
            },
        });
}

void title_scene::update_common_animation(float dt) {
    auth_overlay::poll_restore(auth_controller_, play_state_.auth, play_state_.login_dialog);
    auth_overlay::poll_request(auth_controller_, play_state_.auth, play_state_.login_dialog);
    poll_play_catalog_reload();
    play_transfer_controller_.poll(play_state_, play_transfer_callbacks(),
                                   mode_ == hub_mode::play || mode_ == hub_mode::create);
    poll_play_ranking_reload();
    poll_scoring_ruleset_warm();
    poll_create_upload();
    if (profile_controller_.poll().content_changed) {
        title_online_view::reload_catalog(online_state_);
        request_play_catalog_reload("", "", mode_ == hub_mode::play || mode_ == hub_mode::create);
    }
    title_online_view::poll_song_page(online_state_);
    title_online_view::poll_chart_page(online_state_);
    title_online_view::poll_owned(online_state_);
    profile_controller_.close_if_logged_out(play_state_.auth.logged_in);
    const hub_mode content_mode = content_mode_for_settings(mode_, settings_return_mode_);

    if (title_online_view::poll_download(online_state_)) {
        preferred_song_id_ = title_online_view::selected_song_id(online_state_);
        preferred_chart_id_.clear();
        request_play_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                    content_mode == hub_mode::play || content_mode == hub_mode::create,
                                    true);
    }
    if (title_online_view::poll_catalog(online_state_) && content_mode == hub_mode::online) {
        audio_controller_.preview().select_song(title_online_view::preview_song(online_state_));
    }

    if (intro_hold_t_ > 0.0f) {
        intro_hold_t_ = std::max(0.0f, intro_hold_t_ - dt);
    } else {
        intro_fade_.update(dt);
    }

    if (play_state_.login_dialog.open) {
        play_state_.login_dialog.open_anim = tween::advance(play_state_.login_dialog.open_anim, dt, 8.0f);
    } else {
        play_state_.login_dialog.open_anim = 0.0f;
    }

    profile_controller_.tick(dt);

    settings_overlay_.update_animation(mode_ == hub_mode::settings, dt);

    const float target_anim = content_mode == hub_mode::title ? 0.0f : 1.0f;
    home_menu_anim_ = tween::damp(home_menu_anim_, target_anim, dt, kHomeAnimSpeed, 0.002f);

    const float target_play_anim =
        (content_mode == hub_mode::play || content_mode == hub_mode::online || content_mode == hub_mode::create)
            ? 1.0f
            : 0.0f;
    play_view_anim_ = tween::damp(play_view_anim_, target_play_anim, dt, kPlayViewAnimSpeed, 0.002f);

    if (play_view_anim_ > 0.0f && (content_mode == hub_mode::play || content_mode == hub_mode::create)) {
        song_select::tick_animations(play_state_, dt);
    }
    audio_controller_.update(current_audio_mode(), selected_audio_song(content_mode, play_state_, online_state_), dt);
}

bool title_scene::handle_account_input() {
    if (mode_ == hub_mode::settings) {
        return false;
    }
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    if (home_menu_anim_ < kAccountChipInteractiveThreshold || !ui::is_clicked(account_chip_rect)) {
        return false;
    }
    if (play_state_.login_dialog.open) {
        play_state_.login_dialog.open = false;
    } else {
        song_select::open_login_dialog(play_state_.login_dialog, auth::load_session_summary());
        auth_overlay::refresh_auth_state(play_state_.auth);
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

    capture_current_play_selection();
    title_online_view::reload_catalog(online_state_, true);
    request_play_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                mode_ == hub_mode::play || mode_ == hub_mode::create,
                                true);
    ui::notify("Refreshing catalog...", ui::notice_tone::info, 1.8f);
    return true;
}

bool title_scene::handle_login_dialog_input() {
    if (!play_state_.login_dialog.open) {
        return false;
    }
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
        !auth_controller_.request_active) {
        play_state_.login_dialog.open = false;
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
            auth_overlay::refresh_auth_state(play_state_.auth);
            const hub_mode return_mode = settings_return_mode_;
            switch (return_mode) {
                case hub_mode::title:
                    enter_title_mode();
                    break;
                case hub_mode::play:
                    enter_play_mode();
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
           mode == hub_mode::online ? title_audio_policy::hub_mode::online :
           mode == hub_mode::create ? title_audio_policy::hub_mode::create :
                                  title_audio_policy::hub_mode::home;
}

void title_scene::on_enter() {
    const bool calculate_startup_levels = consume_startup_level_calculation();
    song_select::catalog_data startup_catalog;
    try {
        startup_catalog = song_select::load_catalog(calculate_startup_levels);
    } catch (const std::exception& ex) {
        startup_catalog.load_errors = {ex.what()};
    } catch (...) {
        startup_catalog.load_errors = {"Failed to load song catalog."};
    }

    audio_controller_.configure(kTitleIntroPath, kTitleLoopPath);
    audio_controller_.on_enter();
    song_select::reset_for_enter(play_state_);
    play_data_controller_.reset(play_state_);
    auth_overlay::refresh_auth_state(play_state_.auth);
    profile_controller_.reset();
    request_scoring_ruleset_warm(true);
    play_state_.recent_result_offset = recent_result_offset_;
    if (play_intro_fade_) {
        intro_fade_.restart(scene_fade::direction::in, 1.0f, 1.0f);
        intro_hold_t_ = 0.5f;
    } else {
        intro_fade_.restart(scene_fade::direction::in, 0.0f, 0.0f);
        intro_hold_t_ = 0.0f;
    }
    mode_ = start_in_create_view_ ? hub_mode::create
        : (start_in_play_view_ ? hub_mode::play : (start_with_home_open_ ? hub_mode::home : hub_mode::title));
    suppress_home_pointer_until_release_ = false;
    settings_return_mode_ = hub_mode::home;
    home_menu_anim_ = mode_ == hub_mode::title ? 0.0f : 1.0f;
    home_menu_selected_index_ = 0;
    home_status_message_.clear();
    play_view_anim_ = (mode_ == hub_mode::play || mode_ == hub_mode::online || mode_ == hub_mode::create) ? 1.0f : 0.0f;
    play_entry_origin_rect_ = {};
    settings_overlay_.open();
    play_state_.login_dialog.open = false;
    title_online_view::reload_catalog(online_state_);
    song_select::apply_catalog(play_state_, std::move(startup_catalog), preferred_song_id_, preferred_chart_id_);
    if (mode_ == hub_mode::play || mode_ == hub_mode::create) {
        play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
        sync_play_media();
    }
    if (play_state_.auth.logged_in) {
        auth_overlay::start_restore(auth_controller_, play_state_.login_dialog);
    }
    audio_controller_.update(current_audio_mode(), selected_audio_song(mode_, play_state_, online_state_), 0.0f);
}

void title_scene::on_exit() {
    if (mode_ == hub_mode::settings) {
        settings_overlay_.save();
    }
    play_state_.login_dialog.open = false;
    profile_controller_.close();
    play_transfer_controller_.on_exit();
    title_online_view::on_exit(online_state_);
    audio_controller_.on_exit();
}

// Title 上で Home 展開、Play/Create への遷移、Account 導線を扱う。
void title_scene::update(float dt) {
    ui::begin_hit_regions();
    if (play_state_.context_menu.open) {
        ui::register_hit_region(play_state_.context_menu.rect, song_select::layout::kContextMenuLayer);
    }
    if (play_state_.confirmation_dialog.open) {
        ui::register_hit_region(song_select::layout::kConfirmDialogRect, song_select::layout::kModalLayer);
    }
    if (profile_controller_.is_open()) {
        ui::register_hit_region(profile_controller_.bounds(), kTitleModalLayer);
    }
    update_common_animation(dt);

    if (transitioning_to_song_select_) {
        transition_fade_.update(dt);
        if (transition_fade_.complete()) {
            switch (transition_target_) {
            case transition_target::song_select:
                enter_play_mode();
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

    if (play_state_.confirmation_dialog.open && IsKeyPressed(KEY_ESCAPE)) {
        play_transfer_controller_.cancel_confirmation(play_state_);
        return;
    }

    if (play_transfer_controller_.busy()) {
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
    const auto& t = *g_theme;
    const float menu_t = tween::ease_out_cubic(home_menu_anim_);
    const float play_t = tween::ease_out_cubic(play_view_anim_);
    const Rectangle screen_rect = title_layout::screen_rect();
    const Rectangle spectrum_rect = title_layout::spectrum_rect();
    const Rectangle settings_chip_rect = title_layout::settings_chip_rect();
    const Rectangle refresh_chip_rect = title_layout::refresh_chip_rect();
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    virtual_screen::begin_ui();
    draw_scene_background(t);
    ui::begin_draw_queue();
    const float spectrum_alpha = tween::lerp(1.0f, 0.5f, play_t);
    audio_controller_.draw_spectrum(spectrum_rect, spectrum_alpha);
    if (mode_ != hub_mode::settings) {
        title_header_view::draw({
            .closed_header_rect = title_layout::closed_header_rect(),
            .open_header_rect = title_layout::open_header_rect(),
            .refresh_chip_rect = refresh_chip_rect,
            .settings_chip_rect = settings_chip_rect,
            .account_chip_rect = account_chip_rect,
            .menu_t = menu_t,
            .play_t = play_t,
            .account_name = account_name_for(play_state_.auth),
            .account_status = account_status_for(play_state_.auth),
            .avatar_label = make_avatar_label(play_state_.auth),
            .logged_in = play_state_.auth.logged_in,
            .email_verified = play_state_.auth.email_verified,
            .now = GetTime(),
        });

        title_home_view::draw(home_menu_anim_, play_view_anim_, home_menu_selected_index_, home_status_message_);
    }

    if (mode_ == hub_mode::settings) {
        settings_overlay_.draw();
    } else if (mode_ == hub_mode::play || mode_ == hub_mode::create) {
        title_play_view::draw(play_state_, audio_controller_.preview(),
                              mode_ == hub_mode::create ? title_play_view::mode::create : title_play_view::mode::play,
                              play_view_anim_, play_entry_origin_rect_);
    } else if (mode_ == hub_mode::online) {
        title_online_view::draw(online_state_, play_view_anim_, play_entry_origin_rect_);
    }

    const Rectangle account_dialog_anchor = {
        account_chip_rect.x,
        account_chip_rect.y + 12.0f,
        account_chip_rect.width,
        account_chip_rect.height
    };
    if (mode_ == hub_mode::play || mode_ == hub_mode::create) {
        play_transfer_controller_.draw_or_apply_confirmation(
            play_state_, audio_controller_.preview(), play_transfer_callbacks(),
            mode_ == hub_mode::play || mode_ == hub_mode::create);
    }
    profile_controller_.draw(play_state_.auth, auth_controller_.request_active, kTitleModalLayer);
    const song_select::login_dialog_command login_command =
        song_select::draw_login_dialog(play_state_.auth, play_state_.login_dialog,
                                       account_dialog_anchor, screen_rect,
                                       auth_controller_.request_active, kTitleModalLayer);
    if (login_command == song_select::login_dialog_command::close) {
        play_state_.login_dialog.open = false;
    } else if (login_command == song_select::login_dialog_command::request_profile) {
        play_state_.login_dialog.open = false;
        profile_controller_.open();
    } else if (login_command != song_select::login_dialog_command::none) {
        auth_overlay::start_request(auth_controller_, play_state_.login_dialog, login_command);
    }

    ui::flush_draw_queue();

    if (intro_hold_t_ > 0.0f) {
        ui::draw_fullscreen_overlay(BLACK);
    } else {
        intro_fade_.draw();
    }
    if (transitioning_to_song_select_) {
        transition_fade_.draw();
    }
    if (quitting_) {
        quit_fade_.draw();
    }
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
