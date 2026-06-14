#include "title/title_common_update_controller.h"

#include <algorithm>

#include "song_select/song_select_navigation.h"
#include "title/catalog_reload_policy.h"
#include "tween.h"

namespace {

constexpr float kHomeAnimSpeed = 6.5f;
constexpr float kPlayViewAnimSpeed = 6.0f;

title::common_mode content_mode_for_settings(title::common_mode mode, title::common_mode return_mode) {
    return mode == title::common_mode::settings ? return_mode : mode;
}

bool is_play_or_create(title::common_mode mode) {
    return mode == title::common_mode::play || mode == title::common_mode::create;
}

bool uses_play_view(title::common_mode mode) {
    return mode == title::common_mode::play ||
           mode == title::common_mode::multiplayer ||
           mode == title::common_mode::online ||
           mode == title::common_mode::create;
}

const song_select::song_entry* selected_audio_song(title::common_mode mode,
                                                   const song_select::state& play_state,
                                                   const title_online_view::state& online_state) {
    if (mode == title::common_mode::online) {
        return title_online_view::preview_song(online_state);
    }
    return song_select::selected_song(play_state);
}

}  // namespace

namespace title {

void update_common_frame(common_update_context& context, float dt) {
    const common_mode content_mode = content_mode_for_settings(context.mode, context.settings_return_mode);
    const bool content_mode_is_play_or_create = is_play_or_create(content_mode);

    auth_overlay::poll_restore(context.auth_controller,
                               context.play_create_feature.state().auth,
                               context.play_create_feature.state().login_dialog);
    auth_overlay::poll_request(context.auth_controller,
                               context.play_create_feature.state().auth,
                               context.play_create_feature.state().login_dialog);
    context.play_create_feature.poll_catalog_reload(
        context.audio_controller, context.mode == common_mode::play, context.mode == common_mode::create);
    context.play_create_feature.poll_transfer(context.cross_callbacks);
    if (content_mode == common_mode::play) {
        context.play_create_feature.poll_ranking_reload();
    }
    if (context.play_create_feature.poll_scoring_ruleset_warm()) {
        if (!content_mode_is_play_or_create) {
            context.play_create_feature.capture_current_selection();
            context.catalog_reload_coordinator.request_reload(
                context.play_create_feature,
                context.play_create_feature.preferred_song_id(),
                context.play_create_feature.preferred_chart_id(),
                title_catalog::policy_for(title_catalog::reload_mode::scoring_ruleset_warmed));
        }
    }
    if (context.play_create_feature.poll_create_upload()) {
        context.catalog_reload_coordinator.mark_level_refresh_covered();
        context.browse_feature.request_reload(true);
    }

    if (context.profile_controller.poll().content_changed) {
        auth_overlay::refresh_auth_state(context.play_create_feature.state().auth);
        context.browse_feature.request_reload();
        context.catalog_reload_coordinator.request_reload(
            context.play_create_feature,
            "",
            "",
            title_catalog::policy_for(title_catalog::reload_mode::content_changed));
    }

    context.profile_controller.close_if_logged_out(context.play_create_feature.state().auth.logged_in);
    context.public_profile_controller.poll();

    const title_browse_feature::poll_result browse_poll =
        context.browse_feature.poll(content_mode == common_mode::online);
    if (browse_poll.downloaded_content) {
        context.preferred_song_id = browse_poll.downloaded_song_id;
        context.preferred_chart_id.clear();
        if (content_mode == common_mode::multiplayer && context.refresh_multiplayer_local_index) {
            context.refresh_multiplayer_local_index();
        }
        context.catalog_reload_coordinator.request_reload(
            context.play_create_feature,
            context.preferred_song_id,
            context.preferred_chart_id,
            title_catalog::policy_for(title_catalog::reload_mode::content_changed));
    }
    if (browse_poll.select_preview_song) {
        context.audio_controller.select_preview_song(context.browse_feature.preview_song());
    }

    context.catalog_reload_coordinator.request_background_rebuild_if_due(
        context.play_create_feature,
        context.startup.loading,
        content_mode_is_play_or_create);

    if (context.intro_hold_t > 0.0f) {
        context.intro_hold_t = std::max(0.0f, context.intro_hold_t - dt);
    } else {
        context.intro_fade.update(dt);
    }

    if (context.play_create_feature.state().login_dialog.open) {
        context.play_create_feature.state().login_dialog.open_anim =
            tween::advance(context.play_create_feature.state().login_dialog.open_anim, dt, 8.0f);
    } else {
        context.play_create_feature.state().login_dialog.open_anim = 0.0f;
    }

    context.profile_controller.tick(dt);
    context.public_profile_controller.tick(dt);

    context.settings_overlay.update_animation(context.mode == common_mode::settings, dt);

    const float target_anim = content_mode == common_mode::title ? 0.0f : 1.0f;
    context.home_menu_anim = tween::damp(context.home_menu_anim, target_anim, dt, kHomeAnimSpeed, 0.002f);

    const float target_play_anim = uses_play_view(content_mode) ? 1.0f : 0.0f;
    context.play_view_anim = tween::damp(context.play_view_anim, target_play_anim, dt, kPlayViewAnimSpeed, 0.002f);

    if (context.play_view_anim > 0.0f && is_play_or_create(content_mode)) {
        song_select::tick_animations(context.play_create_feature.state(), dt);
    }
    if (content_mode != common_mode::multiplayer && context.current_audio_mode) {
        context.audio_controller.update(context.current_audio_mode(),
                                        selected_audio_song(content_mode,
                                                            context.play_create_feature.state(),
                                                            context.browse_feature.state()),
                                        dt);
    }
}

}  // namespace title
