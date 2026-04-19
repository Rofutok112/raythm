#include "title_scene.h"

#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "audio_manager.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_navigation.h"
#include "title/home_menu_view.h"
#include "title/play_session_controller.h"
#include "title/title_header_view.h"
#include "title/title_layout.h"
#include "title/seamless_song_select_view.h"
#include "theme.h"
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

bool preview_audio_active() {
    audio_manager& audio = audio_manager::instance();
    return audio.is_preview_loaded() || audio.is_preview_playing();
}

float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

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

float lerp_value(float from, float to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return from + (to - from) * clamped;
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
    start_in_create_view_(start_in_create_view) {
}

void title_scene::enter_title_mode() {
    mode_ = hub_mode::title;
    suppress_home_pointer_until_release_ = false;
    home_status_message_.clear();
    sync_audio_mode();
}

void title_scene::enter_home_mode(bool suppress_pointer) {
    mode_ = hub_mode::home;
    suppress_home_pointer_until_release_ = suppress_pointer;
    home_status_message_.clear();
    sync_audio_mode();
}

void title_scene::enter_play_mode() {
    mode_ = hub_mode::play;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    sync_audio_mode();
    title_play_session::sync_media(play_state_, preview_controller_);
}

void title_scene::enter_create_mode() {
    mode_ = hub_mode::create;
    home_status_message_.clear();
    play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
    sync_audio_mode();
    title_play_session::sync_media(play_state_, preview_controller_);
}

void title_scene::update_play_mode(float dt) {
    const title_play_view::update_result result =
        title_play_view::update(play_state_, title_play_view::mode::play, play_view_anim_, play_entry_origin_rect_, dt);

    if (result.back_requested) {
        enter_home_mode(false);
        return;
    }
    if (result.play_requested) {
        title_play_session::start_selected_chart(manager_, play_state_, preview_controller_);
        return;
    }
    if (result.song_selection_changed) {
        title_play_session::sync_media(play_state_, preview_controller_);
        return;
    }
    if (result.chart_selection_changed || result.ranking_source_changed) {
        title_play_session::reload_ranking(play_state_);
        return;
    }
}

void title_scene::update_create_mode(float dt) {
    const title_play_view::update_result result =
        title_play_view::update(play_state_, title_play_view::mode::create, play_view_anim_, play_entry_origin_rect_, dt);

    if (result.back_requested) {
        enter_home_mode(false);
        return;
    }
    if (result.song_selection_changed) {
        title_play_session::sync_media(play_state_, preview_controller_);
        return;
    }
    if (result.chart_selection_changed) {
        return;
    }

    const song_select::song_entry* song = song_select::selected_song(play_state_);
    const auto filtered = song_select::filtered_charts_for_selected_song(play_state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(play_state_, filtered);

    if (result.create_song_requested) {
        manager_.change_scene(song_select::make_song_create_scene(manager_));
        return;
    }
    if (result.edit_song_requested && song != nullptr) {
        manager_.change_scene(song_select::make_edit_song_scene(manager_, *song));
        return;
    }
    if (result.create_chart_requested && song != nullptr) {
        manager_.change_scene(song_select::make_new_chart_scene(manager_, *song, play_state_.difficulty_index));
        return;
    }
    if (result.edit_chart_requested && song != nullptr && chart != nullptr) {
        manager_.change_scene(song_select::make_edit_chart_scene(manager_, *song, *chart));
        return;
    }
    if (result.edit_mv_requested && song != nullptr) {
        manager_.change_scene(song_select::make_mv_editor_scene(manager_, *song));
        return;
    }
    if (result.manage_library_requested) {
        manager_.change_scene(song_select::make_legacy_song_select_scene(
            manager_,
            song != nullptr ? song->song.meta.song_id : "",
            chart != nullptr ? chart->meta.chart_id : "",
            std::nullopt,
            false));
        return;
    }
}

void title_scene::update_common_animation(float dt) {
    auth_overlay::poll_restore(auth_controller_, play_state_.auth, play_state_.login_dialog);
    auth_overlay::poll_request(auth_controller_, play_state_.auth, play_state_.login_dialog);

    if (intro_hold_t_ > 0.0f) {
        intro_hold_t_ = std::max(0.0f, intro_hold_t_ - dt);
    } else {
        intro_fade_.update(dt);
    }

    if (play_state_.login_dialog.open) {
        play_state_.login_dialog.open_anim = std::min(1.0f, play_state_.login_dialog.open_anim + dt * 8.0f);
    } else {
        play_state_.login_dialog.open_anim = 0.0f;
    }

    const float target_anim = mode_ == hub_mode::title ? 0.0f : 1.0f;
    home_menu_anim_ = std::clamp(home_menu_anim_ + (target_anim - home_menu_anim_) * std::min(1.0f, dt * kHomeAnimSpeed),
                                 0.0f, 1.0f);
    if (std::fabs(home_menu_anim_ - target_anim) < 0.002f) {
        home_menu_anim_ = target_anim;
    }

    const float target_play_anim = (mode_ == hub_mode::play || mode_ == hub_mode::create) ? 1.0f : 0.0f;
    play_view_anim_ = std::clamp(play_view_anim_ + (target_play_anim - play_view_anim_) * std::min(1.0f, dt * kPlayViewAnimSpeed),
                                 0.0f, 1.0f);
    if (std::fabs(play_view_anim_ - target_play_anim) < 0.002f) {
        play_view_anim_ = target_play_anim;
    }

    if (play_view_anim_ > 0.0f || preview_audio_active()) {
        preview_controller_.update(dt, song_select::selected_song(play_state_));
    }
    if (play_view_anim_ > 0.0f) {
        song_select::tick_animations(play_state_, dt);
    }

    sync_audio_mode();
    bgm_controller_.update();
    spectrum_visualizer_.update((mode_ == hub_mode::play || mode_ == hub_mode::create || preview_audio_active())
                                    ? title_spectrum_visualizer::source::preview
                                    : title_spectrum_visualizer::source::bgm);
}

bool title_scene::handle_account_input() {
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

void title_scene::update_home_pointer_suppression() {
    if (suppress_home_pointer_until_release_ &&
        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        suppress_home_pointer_until_release_ = false;
    }
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

bool title_scene::handle_home_input() {
    if (mode_ == hub_mode::title) {
        return false;
    }
    if (mode_ == hub_mode::play || mode_ == hub_mode::create) {
        return false;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        enter_title_mode();
        return true;
    }

    if (!suppress_home_pointer_until_release_) {
        for (int index = 0; index < static_cast<int>(title_home_view::entry_count()); ++index) {
            const Rectangle rect = title_home_view::button_rect(index, home_menu_anim_);
            if (ui::is_hovered(rect)) {
                home_menu_selected_index_ = index;
            }
            if (ui::is_clicked(rect)) {
                const title_home_view::entry& entry =
                    title_home_view::entry_at(static_cast<std::size_t>(index));
                if (entry.enabled) {
                    start_transition(entry.target == title_home_view::action::create
                                         ? transition_target::create_tools
                                         : transition_target::song_select);
                } else {
                    home_status_message_ = "This route is still warming up.";
                }
                return true;
            }
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        home_menu_selected_index_ = (home_menu_selected_index_ + 1) % static_cast<int>(title_home_view::entry_count());
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        home_menu_selected_index_ = (home_menu_selected_index_ - 1 + static_cast<int>(title_home_view::entry_count())) %
                                    static_cast<int>(title_home_view::entry_count());
    }
    if (IsKeyPressed(KEY_ENTER)) {
        const title_home_view::entry& entry =
            title_home_view::entry_at(static_cast<std::size_t>(home_menu_selected_index_));
        if (entry.enabled) {
            start_transition(entry.target == title_home_view::action::create
                                 ? transition_target::create_tools
                                 : transition_target::song_select);
        } else {
            home_status_message_ = "This route is still warming up.";
        }
        return true;
    }
    return false;
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
    if (target == transition_target::create_tools) {
        enter_create_mode();
        return;
    }
    transition_target_ = target;
    transitioning_to_song_select_ = true;
    transition_fade_.restart(scene_fade::direction::out, 0.3f, 0.65f);
}

void title_scene::sync_audio_mode() {
    if (mode_ == hub_mode::play || mode_ == hub_mode::create || preview_audio_active()) {
        bgm_controller_.suspend();
        return;
    }
    bgm_controller_.resume();
}

void title_scene::on_enter() {
    bgm_controller_.configure(kTitleIntroPath, kTitleLoopPath);
    spectrum_visualizer_.reset();
    bgm_controller_.on_enter();
    song_select::reset_for_enter(play_state_);
    play_state_.ranking_panel.selected_source = ranking_service::source::local;
    auth_overlay::refresh_auth_state(play_state_.auth);
    title_play_session::warm_scoring_ruleset();
    // Preload song/chart catalog once on scene entry so HOME -> PLAY animation
    // does not have to wait for difficulty calculation and local rank scanning.
    title_play_session::reload_catalog(play_state_, preview_controller_,
                                       preferred_song_id_, preferred_chart_id_,
                                       (mode_ == hub_mode::play || mode_ == hub_mode::create));
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
    home_menu_anim_ = mode_ == hub_mode::title ? 0.0f : 1.0f;
    home_menu_selected_index_ = 0;
    home_status_message_.clear();
    play_view_anim_ = (mode_ == hub_mode::play || mode_ == hub_mode::create) ? 1.0f : 0.0f;
    play_entry_origin_rect_ = {};
    preview_controller_.stop();
    play_state_.login_dialog.open = false;
    if (mode_ == hub_mode::play || mode_ == hub_mode::create) {
        play_entry_origin_rect_ = title_home_view::button_rect(home_menu_selected_index_, home_menu_anim_);
        bgm_controller_.suspend();
        title_play_session::sync_media(play_state_, preview_controller_);
    }
    if (play_state_.auth.logged_in) {
        auth_overlay::start_restore(auth_controller_, play_state_.login_dialog);
    }
}

void title_scene::on_exit() {
    bgm_controller_.on_exit();
    spectrum_visualizer_.reset();
    play_state_.login_dialog.open = false;
    preview_controller_.stop();
}

// Title 上で Home 展開、Play/Create への遷移、Account 導線を扱う。
void title_scene::update(float dt) {
    ui::begin_hit_regions();
    update_common_animation(dt);

    if (transitioning_to_song_select_) {
        transition_fade_.update(dt);
        if (transition_fade_.complete()) {
            switch (transition_target_) {
            case transition_target::song_select:
                enter_play_mode();
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
            CloseWindow();
        }
        return;
    }

    if (handle_account_input()) {
        return;
    }

    if (handle_login_dialog_input()) {
        return;
    }

    update_home_pointer_suppression();

    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    const bool account_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(account_chip_rect);
    const bool left_click_for_home =
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !account_hovered;
    const bool right_click_for_home = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (handle_title_input(left_click_for_home, right_click_for_home)) {
        return;
    }

    if (mode_ == hub_mode::play) {
        update_play_mode(dt);
        return;
    }
    if (mode_ == hub_mode::create) {
        update_create_mode(dt);
        return;
    }

    if (handle_home_input()) {
        return;
    }

    update_title_quit(dt);
}

// タイトルと、そこから展開する Home 導線を描画する。
void title_scene::draw() {
    const auto& t = *g_theme;
    const float menu_t = ease_out_cubic(home_menu_anim_);
    const float play_t = ease_out_cubic(play_view_anim_);
    const Rectangle screen_rect = title_layout::screen_rect();
    const Rectangle spectrum_rect = title_layout::spectrum_rect();
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::begin_draw_queue();
    const float spectrum_alpha = lerp_value(1.0f, 0.5f, play_t);
    spectrum_visualizer_.draw(spectrum_rect, spectrum_alpha);
    title_header_view::draw({
        .closed_header_rect = title_layout::closed_header_rect(),
        .open_header_rect = title_layout::open_header_rect(),
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

    title_play_view::draw(play_state_, preview_controller_,
                          mode_ == hub_mode::create ? title_play_view::mode::create : title_play_view::mode::play,
                          play_view_anim_, play_entry_origin_rect_);

    const Rectangle account_dialog_anchor = {
        account_chip_rect.x,
        account_chip_rect.y + 12.0f,
        account_chip_rect.width,
        account_chip_rect.height
    };
    const song_select::login_dialog_command login_command =
        song_select::draw_login_dialog(play_state_.auth, play_state_.login_dialog,
                                       account_dialog_anchor, screen_rect,
                                       auth_controller_.request_active, kTitleModalLayer);
    if (login_command == song_select::login_dialog_command::close) {
        play_state_.login_dialog.open = false;
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
