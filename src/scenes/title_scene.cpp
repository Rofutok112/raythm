#include "title_scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/file_dialog.h"
#include "raylib.h"
#include "rlgl.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_import_export_service.h"
#include "song_select/song_select_last_played.h"
#include "song_select/song_select_command_controller.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_detail_view.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_navigation.h"
#include "tween.h"
#include "title/home_menu_view.h"
#include "title/online_download_view.h"
#include "title/play_session_controller.h"
#include "title/title_header_view.h"
#include "title/title_layout.h"
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
constexpr float kSettingsViewAnimSpeed = 7.5f;
constexpr float kSettingsViewSlideOffsetY = 36.0f;
constexpr unsigned char kSettingsPanelAlpha = 205;
constexpr unsigned char kSettingsSectionAlpha = 190;
constexpr unsigned char kSettingsRowAlpha = 178;
constexpr unsigned char kSettingsRowHoverAlpha = 212;
constexpr unsigned char kSettingsRowSelectedAlpha = 218;
constexpr unsigned char kSettingsBorderAlpha = 210;
constexpr ui::draw_layer kTitleModalLayer = ui::draw_layer::modal;

Color fade_alpha(Color color, float fade) {
    const float alpha = static_cast<float>(color.a) * std::clamp(fade, 0.0f, 1.0f);
    return with_alpha(color, static_cast<unsigned char>(std::clamp(alpha, 0.0f, 255.0f)));
}

Color cap_alpha(Color color, unsigned char alpha, float fade) {
    const float capped = static_cast<float>(alpha) * std::clamp(fade, 0.0f, 1.0f);
    return with_alpha(color, static_cast<unsigned char>(std::clamp(capped, 0.0f, 255.0f)));
}

ui_theme make_settings_overlay_theme(const ui_theme& source, float fade) {
    ui_theme theme = source;
    theme.panel = cap_alpha(source.panel, kSettingsPanelAlpha, fade);
    theme.section = cap_alpha(source.section, kSettingsSectionAlpha, fade);
    theme.row = cap_alpha(source.row, kSettingsRowAlpha, fade);
    theme.row_hover = cap_alpha(source.row_hover, kSettingsRowHoverAlpha, fade);
    theme.row_selected = cap_alpha(source.row_selected, kSettingsRowSelectedAlpha, fade);
    theme.row_selected_hover = cap_alpha(source.row_selected_hover, kSettingsRowSelectedAlpha, fade);
    theme.row_active = cap_alpha(source.row_active, kSettingsRowSelectedAlpha, fade);
    theme.row_list_hover = cap_alpha(source.row_list_hover, kSettingsRowHoverAlpha, fade);
    theme.row_soft = cap_alpha(source.row_soft, source.row_soft_alpha, fade);
    theme.row_soft_hover = cap_alpha(source.row_soft_hover, source.row_soft_hover_alpha, fade);
    theme.row_soft_selected = cap_alpha(source.row_soft_selected, source.row_soft_selected_alpha, fade);
    theme.row_soft_selected_hover = cap_alpha(source.row_soft_selected_hover, source.row_soft_selected_hover_alpha, fade);
    theme.border = cap_alpha(source.border, kSettingsBorderAlpha, fade);
    theme.border_light = cap_alpha(source.border_light, kSettingsBorderAlpha, fade);
    theme.border_active = fade_alpha(source.border_active, fade);
    theme.text = fade_alpha(source.text, fade);
    theme.text_secondary = fade_alpha(source.text_secondary, fade);
    theme.text_muted = fade_alpha(source.text_muted, fade);
    theme.text_dim = fade_alpha(source.text_dim, fade);
    theme.text_hint = fade_alpha(source.text_hint, fade);
    theme.slider_track = cap_alpha(source.slider_track, kSettingsRowAlpha, fade);
    theme.slider_fill = fade_alpha(source.slider_fill, fade);
    theme.slider_knob = fade_alpha(source.slider_knob, fade);
    theme.scrollbar_track = cap_alpha(source.scrollbar_track, kSettingsRowAlpha, fade);
    theme.scrollbar_thumb = fade_alpha(source.scrollbar_thumb, fade);
    return theme;
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

bool can_upload_content(content_status status) {
    return status == content_status::local;
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
    settings_gameplay_page_(g_settings),
    settings_audio_page_(g_settings, settings_runtime_applier_),
    settings_video_page_(g_settings, settings_runtime_applier_),
    settings_key_config_page_(g_settings) {
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
    settings_view_anim_ = 0.0f;
    settings_view_closing_ = false;
    home_status_message_.clear();
    play_state_.login_dialog.open = false;
    title_profile_view::close(profile_state_);
    current_settings_page_ = settings::page_id::gameplay;
    settings_gameplay_page_.reset_interaction();
    settings_audio_page_.reset_interaction();
    settings_video_page_.reset_interaction();
    settings_key_config_page_.reset();
    audio_controller_.update(current_audio_mode(), song_select::selected_song(play_state_), 0.0f);
}

void title_scene::close_settings_mode() {
    if (settings_view_closing_) {
        return;
    }
    save_settings(g_settings);
    settings_key_config_page_.clear_selection();
    settings_view_closing_ = true;
}

void title_scene::request_play_catalog_reload(std::string preferred_song_id,
                                              std::string preferred_chart_id,
                                              bool sync_media_on_apply) {
    if (play_catalog_loading_) {
        play_catalog_reload_pending_ = true;
        queued_play_catalog_song_id_ = std::move(preferred_song_id);
        queued_play_catalog_chart_id_ = std::move(preferred_chart_id);
        queued_play_catalog_sync_media_on_apply_ =
            queued_play_catalog_sync_media_on_apply_ || sync_media_on_apply;
        play_state_.catalog_loading = true;
        return;
    }

    play_catalog_song_id_ = std::move(preferred_song_id);
    play_catalog_chart_id_ = std::move(preferred_chart_id);
    play_catalog_sync_media_on_apply_ = sync_media_on_apply;
    play_catalog_loading_ = true;
    play_state_.catalog_loading = true;
    std::promise<song_select::catalog_data> promise;
    play_catalog_future_ = promise.get_future();
    std::thread([promise = std::move(promise)]() mutable {
        try {
            promise.set_value(song_select::load_catalog());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::poll_play_catalog_reload() {
    if (!play_catalog_loading_) {
        return;
    }
    if (play_catalog_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    try {
        song_select::apply_catalog(play_state_, play_catalog_future_.get(),
                                   play_catalog_song_id_, play_catalog_chart_id_);
    } catch (const std::exception& ex) {
        song_select::apply_catalog(play_state_, {}, play_catalog_song_id_, play_catalog_chart_id_);
        play_state_.load_errors = {ex.what()};
    }
    play_catalog_loading_ = false;
    const bool should_sync_media = play_catalog_sync_media_on_apply_ || mode_ == hub_mode::play;
    if (should_sync_media) {
        sync_play_media();
    } else if (mode_ == hub_mode::create) {
        title_play_session::sync_preview(play_state_, audio_controller_.preview());
    }
    play_catalog_sync_media_on_apply_ = false;

    if (!play_catalog_reload_pending_) {
        return;
    }

    play_catalog_reload_pending_ = false;
    request_play_catalog_reload(queued_play_catalog_song_id_,
                                queued_play_catalog_chart_id_,
                                queued_play_catalog_sync_media_on_apply_);
    queued_play_catalog_song_id_.clear();
    queued_play_catalog_chart_id_.clear();
    queued_play_catalog_sync_media_on_apply_ = false;
}

void title_scene::sync_play_media() {
    title_play_session::sync_preview(play_state_, audio_controller_.preview());
    request_play_ranking_reload();
}

void title_scene::request_play_ranking_reload() {
    if (play_ranking_loading_) {
        ++play_ranking_generation_;
        play_ranking_reload_pending_ = true;
        if (play_state_.ranking_panel.selected_source == ranking_service::source::online) {
            play_state_.ranking_panel.listing = {};
            play_state_.ranking_panel.listing.ranking_source = play_state_.ranking_panel.selected_source;
            play_state_.ranking_panel.listing.available = false;
            play_state_.ranking_panel.listing.message = "Loading online rankings...";
        }
        return;
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(play_state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(play_state_, filtered);
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    const ranking_service::source source = play_state_.ranking_panel.selected_source;

    ++play_ranking_generation_;
    play_ranking_pending_generation_ = play_ranking_generation_;
    play_state_.ranking_panel.source_dropdown_open = false;
    play_state_.ranking_panel.scroll_y = 0.0f;
    play_state_.ranking_panel.scroll_y_target = 0.0f;
    play_state_.ranking_panel.scrollbar_dragging = false;
    play_state_.ranking_panel.scrollbar_drag_offset = 0.0f;

    if (source == ranking_service::source::online) {
        play_state_.ranking_panel.listing = {};
        play_state_.ranking_panel.listing.ranking_source = source;
        play_state_.ranking_panel.listing.available = false;
        play_state_.ranking_panel.listing.message = "Loading online rankings...";
    }

    play_ranking_loading_ = true;
    std::promise<ranking_service::listing> promise;
    play_ranking_future_ = promise.get_future();
    std::thread([promise = std::move(promise), chart_id, source]() mutable {
        try {
            promise.set_value(ranking_service::load_chart_ranking(chart_id, source, 50));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::poll_play_ranking_reload() {
    if (!play_ranking_loading_) {
        return;
    }
    if (play_ranking_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    ranking_service::listing listing;
    try {
        listing = play_ranking_future_.get();
    } catch (const std::exception& ex) {
        listing.available = false;
        listing.message = ex.what();
        listing.ranking_source = play_state_.ranking_panel.selected_source;
    }
    play_ranking_loading_ = false;
    const bool stale = play_ranking_pending_generation_ != play_ranking_generation_;
    if (!stale) {
        play_state_.ranking_panel.listing = std::move(listing);
        play_state_.ranking_panel.reveal_anim = 0.0f;
    }

    if (play_ranking_reload_pending_) {
        play_ranking_reload_pending_ = false;
        request_play_ranking_reload();
        return;
    }
}

void title_scene::request_scoring_ruleset_warm(bool force_refresh) {
    if (scoring_ruleset_loading_) {
        return;
    }

    scoring_ruleset_loading_ = true;
    std::promise<bool> promise;
    scoring_ruleset_future_ = promise.get_future();
    std::thread([promise = std::move(promise), force_refresh]() mutable {
        try {
            promise.set_value(ranking_service::warm_scoring_ruleset_cache(force_refresh));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::poll_scoring_ruleset_warm() {
    if (!scoring_ruleset_loading_) {
        return;
    }
    if (scoring_ruleset_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    try {
        scoring_ruleset_future_.get();
    } catch (...) {
    }
    scoring_ruleset_loading_ = false;
}

void title_scene::start_song_upload(const song_select::song_entry& song) {
    if (create_upload_in_progress_) {
        ui::notify("Upload already in progress.", ui::notice_tone::info, 1.8f);
        return;
    }

    create_upload_in_progress_ = true;
    ui::notify("Uploading song...", ui::notice_tone::info, 1.8f);
    std::promise<title_create_upload::upload_result> promise;
    create_upload_future_ = promise.get_future();
    std::thread([promise = std::move(promise), song]() mutable {
        try {
            promise.set_value(title_create_upload::upload_song(song));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::start_chart_upload(const song_select::song_entry& song,
                                     const song_select::chart_option& chart) {
    if (create_upload_in_progress_) {
        ui::notify("Upload already in progress.", ui::notice_tone::info, 1.8f);
        return;
    }

    create_upload_in_progress_ = true;
    ui::notify("Uploading chart...", ui::notice_tone::info, 1.8f);
    std::promise<title_create_upload::upload_result> promise;
    create_upload_future_ = promise.get_future();
    std::thread([promise = std::move(promise), song, chart]() mutable {
        try {
            promise.set_value(title_create_upload::upload_chart(song, chart));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::poll_create_upload() {
    if (!create_upload_in_progress_) {
        return;
    }
    if (create_upload_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    title_create_upload::upload_result result;
    try {
        result = create_upload_future_.get();
    } catch (const std::exception& ex) {
        result.success = false;
        result.message = ex.what();
    } catch (...) {
        result.success = false;
        result.message = "Upload failed.";
    }
    create_upload_in_progress_ = false;
    song_select::queue_status_message(play_state_, result.message, !result.success);
    if (result.success && result.should_refresh_online_catalog) {
        title_online_view::reload_catalog(online_state_);
    }
}

void title_scene::open_profile() {
    title_profile_view::open(profile_state_);
    request_profile_reload();
}

void title_scene::request_profile_reload() {
    if (profile_state_.loading) {
        return;
    }

    profile_state_.loading = true;
    std::promise<title_profile_view::load_result> promise;
    profile_load_future_ = promise.get_future();
    std::thread([promise = std::move(promise)]() mutable {
        try {
            title_profile_view::load_result loaded;
            loaded.uploads = auth::fetch_my_community_uploads();
            loaded.rankings = auth::fetch_my_profile_rankings();

            auto to_activity_item = [](const auth::profile_ranking_record& record) {
                title_profile_view::activity_item item;
                item.song_title = record.song_title;
                item.artist = record.artist;
                item.difficulty_name = record.difficulty_name;
                item.local_summary = "Score " + std::to_string(record.score);
                item.online_summary = "Online #" + std::to_string(record.placement) +
                                      " / " + std::to_string(record.score);
                return item;
            };

            if (loaded.rankings.success) {
                for (const auth::profile_ranking_record& record : loaded.rankings.recent_records) {
                    loaded.activity.push_back(to_activity_item(record));
                }
                for (const auth::profile_ranking_record& record : loaded.rankings.first_place_records) {
                    loaded.first_place_records.push_back(to_activity_item(record));
                }
            }

            promise.set_value(std::move(loaded));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::poll_profile() {
    if (profile_state_.loading &&
        profile_load_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        title_profile_view::load_result loaded;
        try {
            loaded = profile_load_future_.get();
        } catch (const std::exception& ex) {
            loaded.uploads = {
                .success = false,
                .message = ex.what(),
            };
        } catch (...) {
            loaded.uploads = {
                .success = false,
                .message = "Failed to load profile.",
            };
        }
        profile_state_.uploads = std::move(loaded.uploads);
        profile_state_.rankings = std::move(loaded.rankings);
        profile_state_.activity = std::move(loaded.activity);
        profile_state_.first_place_records = std::move(loaded.first_place_records);
        profile_state_.loading = false;
        profile_state_.loaded_once = true;
        title_profile_view::clamp_scroll(profile_state_);
        if (!profile_state_.uploads.success) {
            ui::notify(profile_state_.uploads.message, ui::notice_tone::error, 3.0f);
        } else if (!profile_state_.rankings.success) {
            ui::notify(profile_state_.rankings.message, ui::notice_tone::error, 3.0f);
        }
    }

    if (!profile_state_.deleting ||
        profile_delete_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    auth::operation_result result;
    try {
        result = profile_delete_future_.get();
    } catch (const std::exception& ex) {
        result.success = false;
        result.message = ex.what();
    } catch (...) {
        result.success = false;
        result.message = "Delete failed.";
    }
    profile_state_.deleting = false;
    profile_state_.pending_delete = title_profile_view::delete_target::none;
    profile_state_.pending_id.clear();
    profile_state_.pending_label.clear();
    ui::notify(result.message, result.success ? ui::notice_tone::success : ui::notice_tone::error, 3.0f);
    if (result.success) {
        title_online_view::reload_catalog(online_state_);
        request_play_catalog_reload("", "", mode_ == hub_mode::play || mode_ == hub_mode::create);
        request_profile_reload();
    }
}

void title_scene::start_profile_delete_song(std::string song_id) {
    if (profile_state_.deleting) {
        return;
    }

    profile_state_.deleting = true;
    std::promise<auth::operation_result> promise;
    profile_delete_future_ = promise.get_future();
    std::thread([promise = std::move(promise), song_id = std::move(song_id)]() mutable {
        try {
            promise.set_value(auth::delete_community_song_upload(song_id));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_scene::start_profile_delete_chart(std::string chart_id) {
    if (profile_state_.deleting) {
        return;
    }

    profile_state_.deleting = true;
    std::promise<auth::operation_result> promise;
    profile_delete_future_ = promise.get_future();
    std::thread([promise = std::move(promise), chart_id = std::move(chart_id)]() mutable {
        try {
            promise.set_value(auth::delete_community_chart_upload(chart_id));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool title_scene::handle_profile_input() {
    const title_profile_view::command command =
        title_profile_view::update(profile_state_, auth_controller_.request_active);
    switch (command.type) {
    case title_profile_view::command_type::delete_account:
        if (command.password.empty()) {
            ui::notify("Password is required to delete the account.", ui::notice_tone::error, 2.8f);
        } else {
            play_state_.login_dialog.password_input.value = command.password;
            auth_overlay::start_request(auth_controller_, play_state_.login_dialog,
                                        song_select::login_dialog_command::request_delete_account);
            title_profile_view::close(profile_state_);
        }
        return true;
    case title_profile_view::command_type::delete_song:
        start_profile_delete_song(command.id);
        return true;
    case title_profile_view::command_type::delete_chart:
        start_profile_delete_chart(command.id);
        return true;
    case title_profile_view::command_type::close:
        return true;
    case title_profile_view::command_type::none:
        return profile_state_.open;
    }
    return profile_state_.open;
}

void title_scene::draw_profile_modal() {
    title_profile_view::draw(profile_state_, play_state_.auth, auth_controller_.request_active, kTitleModalLayer);
}

void title_scene::apply_play_delete_result(const song_select::delete_result& result) {
    play_state_.confirmation_dialog = {};
    if (!result.success) {
        song_select::queue_status_message(play_state_, result.message, true);
        return;
    }

    const song_select::song_entry* selected_song = song_select::selected_song(play_state_);
    const std::string deleted_song_id = selected_song != nullptr ? selected_song->song.meta.song_id : "";
    preferred_song_id_ = result.preferred_song_id;
    preferred_chart_id_ = result.preferred_chart_id;
    audio_controller_.preview().stop();
    title_online_view::mark_song_removed(online_state_, deleted_song_id);
    title_online_view::reload_catalog(online_state_);
    request_play_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                mode_ == hub_mode::play || mode_ == hub_mode::create);
    song_select::queue_status_message(play_state_, result.message, false);
}

void title_scene::apply_play_transfer_result(const song_select::transfer_result& result) {
    if (result.cancelled) {
        return;
    }
    if (!result.success) {
        song_select::queue_status_message(play_state_, result.message, true);
        return;
    }

    if (result.reload_catalog) {
        preferred_song_id_ = result.preferred_song_id;
        preferred_chart_id_ = result.preferred_chart_id;
        title_online_view::reload_catalog(online_state_);
        request_play_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                    mode_ == hub_mode::play || mode_ == hub_mode::create);
    }
    song_select::queue_status_message(play_state_, result.message, false);
}

void title_scene::open_overwrite_song_confirmation(std::vector<song_select::song_import_request> requests) {
    const size_t overwrite_count = requests.size();
    transfer_controller_.set_pending_song_import_requests(std::move(requests));
    song_select::open_confirmation_dialog(
        play_state_, song_select::pending_confirmation_action::overwrite_song_import,
        overwrite_count <= 1 ? "Overwrite Song" : "Overwrite Songs",
        overwrite_count <= 1 ? "A user song with the same song ID already exists. Overwrite it?"
                             : "Some selected songs already exist. Overwrite them?",
        "",
        "OVERWRITE");
}

void title_scene::open_overwrite_chart_confirmation(std::vector<song_select::chart_import_request> requests) {
    const size_t overwrite_count = requests.size();
    transfer_controller_.set_pending_chart_import_requests(std::move(requests));
    song_select::open_confirmation_dialog(
        play_state_, song_select::pending_confirmation_action::overwrite_chart_import,
        overwrite_count <= 1 ? "Overwrite Chart" : "Overwrite Charts",
        overwrite_count <= 1 ? "A user chart with the same chart ID already exists. Overwrite it?"
                             : "Some selected charts already exist. Overwrite them?",
        "",
        "OVERWRITE");
}

void title_scene::poll_play_transfer() {
    if (const auto prepared = transfer_controller_.poll_song_import_prepare(); prepared.has_value()) {
        if (prepared->requests.empty()) {
            apply_play_transfer_result(prepared->transfer);
        } else if (prepared->overwrite_count > 0) {
            open_overwrite_song_confirmation(prepared->requests);
        } else {
            transfer_controller_.start_song_imports(prepared->requests);
            song_select::queue_status_message(play_state_, transfer_controller_.busy_label(), false);
        }
    }
    if (const auto result = transfer_controller_.poll_background_transfer(); result.has_value()) {
        apply_play_transfer_result(*result);
    }
}

void title_scene::start_song_import() {
    const std::vector<std::string> source_paths = file_dialog::open_song_package_files();
    if (source_paths.empty()) {
        return;
    }
    transfer_controller_.start_song_import_prepare(play_state_, source_paths);
    song_select::queue_status_message(play_state_, transfer_controller_.busy_label(), false);
}

void title_scene::start_chart_import() {
    song_select::transfer_result result;
    if (const auto batch = song_select::prepare_chart_imports(play_state_, result); batch.has_value()) {
        if (batch->overwrite_count > 0) {
            open_overwrite_chart_confirmation(batch->requests);
        } else {
            apply_play_transfer_result(song_select::import_chart_packages(batch->requests));
        }
    } else {
        apply_play_transfer_result(result);
    }
}

void title_scene::start_song_export() {
    const int song_index = play_state_.selected_song_index;
    if (const auto request = song_select::prepare_song_export(play_state_, song_index); request.has_value()) {
        transfer_controller_.start_song_export(*request);
        song_select::queue_status_message(play_state_, transfer_controller_.busy_label(), false);
    }
}

void title_scene::start_chart_export() {
    apply_play_transfer_result(
        song_select::export_chart_package(play_state_, play_state_.selected_song_index, play_state_.difficulty_index));
}

void title_scene::update_play_mode(float dt) {
    if (transfer_controller_.busy()) {
        return;
    }

    const title_play_view::update_result result =
        title_play_view::update(play_state_, title_play_view::mode::play, play_view_anim_, play_entry_origin_rect_, dt);

    if (result.back_requested) {
        enter_home_mode(false);
        return;
    }
    if (result.delete_song_requested) {
        return;
    }
    if (result.delete_chart_requested) {
        return;
    }
    if (result.play_requested) {
        title_play_session::start_selected_chart(manager_, play_state_, audio_controller_.preview());
        return;
    }
    if (result.song_selection_changed) {
        sync_play_media();
        return;
    }
    if (result.chart_selection_changed || result.ranking_source_changed) {
        request_play_ranking_reload();
        return;
    }
}

void title_scene::update_create_mode(float dt) {
    const title_play_view::update_result result =
        title_play_view::update(play_state_, title_play_view::mode::create, play_view_anim_, play_entry_origin_rect_, dt);
    const bool create_action_requested =
        result.create_song_requested ||
        result.edit_song_requested ||
        result.upload_song_requested ||
        result.import_song_requested ||
        result.export_song_requested ||
        result.create_chart_requested ||
        result.edit_chart_requested ||
        result.upload_chart_requested ||
        result.import_chart_requested ||
        result.export_chart_requested ||
        result.edit_mv_requested ||
        result.manage_library_requested;

    if (result.back_requested) {
        enter_home_mode(false);
        return;
    }
    if (result.song_selection_changed) {
        title_play_session::sync_preview(play_state_, audio_controller_.preview());
        return;
    }
    if (result.chart_selection_changed) {
        return;
    }
    if (create_upload_in_progress_ && create_action_requested) {
        ui::notify("Wait for the current upload to finish.", ui::notice_tone::info, 1.8f);
        return;
    }
    if (transfer_controller_.busy() && create_action_requested) {
        ui::notify("Wait for the current transfer to finish.", ui::notice_tone::info, 1.8f);
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
    if (result.import_song_requested) {
        start_song_import();
        return;
    }
    if (result.export_song_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(play_state_, "Select a song to export.", true);
        } else {
            start_song_export();
        }
        return;
    }
    if (result.upload_song_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(play_state_, "Select a song to upload.", true);
        } else if (!can_upload_content(song->status)) {
            ui::notify("Only Local songs can be uploaded.", ui::notice_tone::error, 2.8f);
        } else {
            start_song_upload(*song);
        }
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
    if (result.import_chart_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(play_state_, "Select a song before importing a chart.", true);
        } else {
            start_chart_import();
        }
        return;
    }
    if (result.export_chart_requested) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(play_state_, "Select a chart to export.", true);
        } else {
            start_chart_export();
        }
        return;
    }
    if (result.upload_chart_requested) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(play_state_, "Select a chart to upload.", true);
        } else if (!can_upload_content(song->status) || !can_upload_content(chart->status)) {
            ui::notify("Only Local charts from Local songs can be uploaded.", ui::notice_tone::error, 2.8f);
        } else {
            start_chart_upload(*song, *chart);
        }
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

void title_scene::update_online_mode(float dt) {
    const title_online_view::update_result result =
        title_online_view::update(online_state_, play_view_anim_, play_entry_origin_rect_, dt);

    if (result.back_requested) {
        enter_home_mode(false);
        return;
    }
    if (result.song_selection_changed) {
        audio_controller_.preview().select_song(title_online_view::preview_song(online_state_));
        return;
    }
    if (result.action == title_online_view::requested_action::primary) {
        title_online_view::start_download(online_state_);
        return;
    }
    if (result.action == title_online_view::requested_action::download_chart) {
        title_online_view::start_chart_download(online_state_);
        return;
    }
    if (result.action == title_online_view::requested_action::restart_preview) {
        audio_controller_.preview().resume(title_online_view::preview_song(online_state_));
        return;
    }
    if (result.action == title_online_view::requested_action::stop_preview) {
        audio_controller_.preview().pause();
        return;
    }
    if (result.action == title_online_view::requested_action::open_local) {
        preferred_song_id_ = title_online_view::selected_song_id(online_state_);
        preferred_chart_id_.clear();
        if (!select_local_song(play_state_, preferred_song_id_)) {
            request_play_catalog_reload(preferred_song_id_, preferred_chart_id_, true);
        }
        enter_play_mode();
    }
}

void title_scene::update_common_animation(float dt) {
    auth_overlay::poll_restore(auth_controller_, play_state_.auth, play_state_.login_dialog);
    auth_overlay::poll_request(auth_controller_, play_state_.auth, play_state_.login_dialog);
    poll_play_catalog_reload();
    poll_play_transfer();
    poll_play_ranking_reload();
    poll_scoring_ruleset_warm();
    poll_create_upload();
    poll_profile();
    title_online_view::poll_song_page(online_state_);
    title_online_view::poll_chart_page(online_state_);
    title_online_view::poll_owned(online_state_);
    if (profile_state_.open && !play_state_.auth.logged_in) {
        title_profile_view::close(profile_state_);
    }
    const hub_mode content_mode = content_mode_for_settings(mode_, settings_return_mode_);

    if (title_online_view::poll_download(online_state_)) {
        preferred_song_id_ = title_online_view::selected_song_id(online_state_);
        preferred_chart_id_.clear();
        request_play_catalog_reload(preferred_song_id_, preferred_chart_id_,
                                    content_mode == hub_mode::play || content_mode == hub_mode::create);
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

    if (profile_state_.open && profile_state_.closing) {
        profile_state_.open_anim = tween::retreat(profile_state_.open_anim, dt, 8.0f);
        if (profile_state_.open_anim <= 0.0f) {
            profile_state_.open = false;
            profile_state_.closing = false;
        }
    } else if (profile_state_.open) {
        profile_state_.open_anim = tween::advance(profile_state_.open_anim, dt, 8.0f);
    } else {
        profile_state_.open_anim = 0.0f;
    }

    update_settings_view_animation(dt);

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

void title_scene::update_settings_view_animation(float dt) {
    if (mode_ != hub_mode::settings) {
        settings_view_anim_ = 0.0f;
        settings_view_closing_ = false;
        return;
    }

    if (settings_view_closing_) {
        settings_view_anim_ = tween::retreat(settings_view_anim_, dt, kSettingsViewAnimSpeed);
    } else {
        settings_view_anim_ = tween::advance(settings_view_anim_, dt, kSettingsViewAnimSpeed);
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

bool title_scene::handle_home_input(bool suppress_pointer_this_frame) {
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

    if (!suppress_pointer_this_frame) {
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
                                         : entry.target == title_home_view::action::online
                                             ? transition_target::online_download
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
                                 : entry.target == title_home_view::action::online
                                     ? transition_target::online_download
                                     : transition_target::song_select);
        } else {
            home_status_message_ = "This route is still warming up.";
        }
        return true;
    }
    return false;
}

void title_scene::update_settings_mode(float dt) {
    if (settings_view_closing_) {
        if (settings_view_anim_ <= 0.0f) {
            const hub_mode return_mode = settings_return_mode_;
            settings_view_closing_ = false;
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

    settings_key_config_page_.tick(dt);

    if (settings_view_anim_ < 0.95f) {
        return;
    }

    if (current_settings_page_blocks_navigation()) {
        update_current_settings_page();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
        ui::is_clicked(settings::kBackRect, settings::kLayer)) {
        close_settings_mode();
        return;
    }

    Rectangle tabs[settings::kPageCount];
    settings::build_tab_rects(tabs);
    for (int i = 0; i < settings::kPageCount; ++i) {
        if (ui::is_clicked(tabs[i], settings::kLayer)) {
            change_settings_page(static_cast<settings::page_id>(i));
            break;
        }
    }

    update_current_settings_page();
}

void title_scene::update_current_settings_page() {
    switch (current_settings_page_) {
        case settings::page_id::gameplay:
            settings_gameplay_page_.update();
            break;
        case settings::page_id::audio:
            settings_audio_page_.update();
            break;
        case settings::page_id::video:
            settings_video_page_.update();
            break;
        case settings::page_id::key_config:
            settings_key_config_page_.update();
            break;
    }
}

void title_scene::change_settings_page(settings::page_id next_page) {
    settings_gameplay_page_.reset_interaction();
    settings_audio_page_.reset_interaction();
    settings_video_page_.reset_interaction();
    if (current_settings_page_ == settings::page_id::key_config && next_page != settings::page_id::key_config) {
        settings_key_config_page_.clear_selection();
    }
    current_settings_page_ = next_page;
}

bool title_scene::current_settings_page_blocks_navigation() const {
    return current_settings_page_ == settings::page_id::key_config && settings_key_config_page_.blocks_navigation();
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
    play_catalog_loading_ = false;
    play_ranking_loading_ = false;
    play_ranking_reload_pending_ = false;
    play_ranking_generation_ = 0;
    play_ranking_pending_generation_ = 0;
    play_catalog_reload_pending_ = false;
    play_catalog_sync_media_on_apply_ = false;
    queued_play_catalog_sync_media_on_apply_ = false;
    play_catalog_song_id_.clear();
    play_catalog_chart_id_.clear();
    queued_play_catalog_song_id_.clear();
    queued_play_catalog_chart_id_.clear();
    play_state_.ranking_panel.selected_source = ranking_service::source::local;
    auth_overlay::refresh_auth_state(play_state_.auth);
    scoring_ruleset_loading_ = false;
    profile_state_ = {};
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
    current_settings_page_ = settings::page_id::gameplay;
    settings_gameplay_page_.reset_interaction();
    settings_audio_page_.reset_interaction();
    settings_video_page_.reset_interaction();
    settings_key_config_page_.reset();
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
        save_settings(g_settings);
    }
    play_state_.login_dialog.open = false;
    title_profile_view::close(profile_state_);
    transfer_controller_.on_exit();
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
    if (profile_state_.open) {
        ui::register_hit_region(title_profile_view::bounds(), kTitleModalLayer);
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
            CloseWindow();
        }
        return;
    }

    if (play_state_.confirmation_dialog.open && IsKeyPressed(KEY_ESCAPE)) {
        transfer_controller_.clear_pending_song_import_request(true);
        transfer_controller_.clear_pending_chart_import_request();
        play_state_.confirmation_dialog = {};
        return;
    }

    if (transfer_controller_.busy()) {
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

    if (!suppress_home_pointer_this_frame && handle_settings_button_input()) {
        return;
    }

    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    const Rectangle settings_chip_rect = title_layout::settings_chip_rect();
    const bool account_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(account_chip_rect);
    const bool settings_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(settings_chip_rect);
    const bool left_click_for_home =
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !account_hovered &&
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

void title_scene::draw_settings_view() const {
    const float eased = tween::ease_out_cubic(settings_view_anim_);
    const float slide_y = (1.0f - eased) * kSettingsViewSlideOffsetY;
    const ui_theme* previous_theme = g_theme;
    ui_theme overlay_theme = make_settings_overlay_theme(*previous_theme, eased);
    g_theme = &overlay_theme;
    const auto& t = *g_theme;

    rlPushMatrix();
    rlTranslatef(0.0f, slide_y, 0.0f);

    ui::draw_panel(settings::kSidebarRect);
    ui::draw_panel(settings::kContentRect);

    ui::draw_header_block(settings::kSidebarHeaderRect, "SETTINGS", "Saved on back");

    Rectangle tabs[settings::kPageCount];
    settings::build_tab_rects(tabs);
    for (int i = 0; i < settings::kPageCount; ++i) {
        const settings::page_descriptor& descriptor =
            settings::page_descriptor_for(static_cast<settings::page_id>(i));
        if (static_cast<int>(current_settings_page_) == i) {
            ui::draw_button_colored(tabs[i], descriptor.navigation_label, 22,
                                    t.row_selected, t.row_active, t.text);
        } else {
            ui::draw_button_colored(tabs[i], descriptor.navigation_label, 22,
                                    t.row, t.row_hover, t.text_secondary);
        }
    }

    draw_marquee_text("ESC or right click goes back", settings::kSidebarHintRect.x,
                      settings::kSidebarHintRect.y, 20, t.text_muted,
                      settings::kSidebarHintRect.width, GetTime());
    ui::draw_button(settings::kBackRect, "BACK", 22);

    const settings::page_descriptor& descriptor = settings::page_descriptor_for(current_settings_page_);
    ui::draw_header_block(settings::kContentHeaderRect, descriptor.title, descriptor.subtitle);

    draw_current_settings_page();

    rlPopMatrix();
    g_theme = previous_theme;
}

void title_scene::draw_current_settings_page() const {
    switch (current_settings_page_) {
        case settings::page_id::gameplay:
            settings_gameplay_page_.draw();
            break;
        case settings::page_id::audio:
            settings_audio_page_.draw();
            break;
        case settings::page_id::video:
            settings_video_page_.draw();
            break;
        case settings::page_id::key_config:
            settings_key_config_page_.draw();
            break;
    }
}

// タイトルと、そこから展開する Home 導線を描画する。
void title_scene::draw() {
    const auto& t = *g_theme;
    const float menu_t = tween::ease_out_cubic(home_menu_anim_);
    const float play_t = tween::ease_out_cubic(play_view_anim_);
    const Rectangle screen_rect = title_layout::screen_rect();
    const Rectangle spectrum_rect = title_layout::spectrum_rect();
    const Rectangle settings_chip_rect = title_layout::settings_chip_rect();
    const Rectangle account_chip_rect = title_layout::account_chip_rect();
    virtual_screen::begin_ui();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::begin_draw_queue();
    const float spectrum_alpha = tween::lerp(1.0f, 0.5f, play_t);
    audio_controller_.draw_spectrum(spectrum_rect, spectrum_alpha);
    if (mode_ != hub_mode::settings) {
        title_header_view::draw({
            .closed_header_rect = title_layout::closed_header_rect(),
            .open_header_rect = title_layout::open_header_rect(),
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
        draw_settings_view();
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
        if (transfer_controller_.busy()) {
            song_select::draw_busy_overlay(transfer_controller_.busy_label());
        } else {
            const song_select::confirmation_command command = song_select::draw_confirmation_dialog(play_state_);
            song_select::commands::apply_confirmation_command(
                play_state_, audio_controller_.preview(), transfer_controller_, command,
                [this](const song_select::delete_result& result) { apply_play_delete_result(result); },
                [this](const song_select::transfer_result& result) { apply_play_transfer_result(result); });
        }
    }
    draw_profile_modal();
    const song_select::login_dialog_command login_command =
        song_select::draw_login_dialog(play_state_.auth, play_state_.login_dialog,
                                       account_dialog_anchor, screen_rect,
                                       auth_controller_.request_active, kTitleModalLayer);
    if (login_command == song_select::login_dialog_command::close) {
        play_state_.login_dialog.open = false;
    } else if (login_command == song_select::login_dialog_command::request_profile) {
        play_state_.login_dialog.open = false;
        open_profile();
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
