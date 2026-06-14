#include "title/title_mode_update_controller.h"

#include "song_select/song_select_navigation.h"
#include "title/catalog_reload_policy.h"

namespace {

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

}  // namespace

namespace title {

command update_play_mode(mode_update_context& context, float dt) {
    const title_play_create_feature::play_update_result result =
        context.play_create_feature.update_play(
            context.manager,
            context.audio_controller,
            context.play_view_anim,
            context.play_entry_origin_rect,
            dt);
    return result.title_command;
}

void update_create_mode(mode_update_context& context, float dt) {
    context.play_create_feature.update_create(
        context.manager,
        context.audio_controller,
        context.play_view_anim,
        context.play_entry_origin_rect,
        dt,
        context.cross_callbacks,
        {
            .enter_home = [&context]() {
                if (context.enter_home_mode) {
                    context.enter_home_mode(false);
                }
            },
        });
}

void update_online_mode(mode_update_context& context, float dt) {
    context.browse_feature.update(
        context.play_view_anim,
        context.play_entry_origin_rect,
        dt,
        context.audio_controller,
        {
            .online = {
                .enter_home = [&context]() {
                    if (context.enter_home_mode) {
                        context.enter_home_mode(false);
                    }
                },
                .select_preview_song = [&context]() {
                    context.audio_controller.select_preview_song(context.browse_feature.preview_song());
                },
                .resume_preview = [&context]() {
                    context.audio_controller.resume_preview_song(context.browse_feature.preview_song());
                },
                .pause_preview = [&context]() {
                    context.audio_controller.pause_preview();
                },
                .open_local_selection = [&context]() {
                    context.preferred_song_id = context.browse_feature.selected_song_id();
                    context.preferred_chart_id.clear();
                    if (!select_local_song(context.play_create_feature.state(), context.preferred_song_id)) {
                        context.catalog_reload_coordinator.request_reload(
                            context.play_create_feature,
                            context.preferred_song_id,
                            context.preferred_chart_id,
                            title_catalog::policy_for(title_catalog::reload_mode::selection_sync));
                    }
                    if (context.enter_play_mode) {
                        context.enter_play_mode();
                    }
                },
            },
        });
}

}  // namespace title
