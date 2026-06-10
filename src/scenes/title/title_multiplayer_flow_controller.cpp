#include "title/title_multiplayer_flow_controller.h"

#include "multiplayer/multiplayer_controller.h"
#include "network/server_environment.h"
#include "title/title_multiplayer_content_resolver.h"

namespace title {

multiplayer_flow_result update_multiplayer_flow(multiplayer_flow_context& context, float dt) {
    const std::string room_server_url =
        server_environment::normalize_url(context.multiplayer_state.auth.server_url);
    prepare_multiplayer_queue_state(
        context.multiplayer_state,
        context.play_state,
        context.multiplayer_local_index,
        context.queue_selected_chart_on_multiplayer_return);
    update_multiplayer_audio(
        context.audio_state,
        context.multiplayer_state,
        context.play_state,
        context.multiplayer_local_index,
        context.audio_controller,
        dt);

    const multiplayer::update_result update_result = multiplayer::update(context.multiplayer_state, dt);
    if (!update_result.requested_profile_user_id.empty()) {
        return {
            .title_command = command::open_public_profile(update_result.requested_profile_user_id),
        };
    }
    if (update_result.back_requested) {
        return {
            .enter_home = true,
        };
    }
    if (update_result.open_song_select_requested) {
        return {
            .title_command = command::open_multiplayer_song_select(),
        };
    }

    if (context.multiplayer_state.current_queue_download_requested) {
        context.multiplayer_state.current_queue_download_requested = false;
        if (!context.multiplayer_state.requested_download_song_id.empty() &&
            !context.multiplayer_state.requested_download_chart_id.empty()) {
            context.browse_feature.start_chart_download_by_remote_id(
                context.multiplayer_state.requested_download_song_id,
                context.multiplayer_state.requested_download_chart_id);
            context.multiplayer_state.requested_download_song_id.clear();
            context.multiplayer_state.requested_download_chart_id.clear();
            context.multiplayer_state.status_message = "Downloading queued chart...";
        }
    }

    if (context.multiplayer_state.start_play_requested) {
        context.multiplayer_state.start_play_requested = false;
        const local_chart_match match =
            find_online_chart_match(context.play_state,
                                    context.multiplayer_local_index,
                                    room_server_url,
                                    context.multiplayer_state.requested_start_song_id,
                                    context.multiplayer_state.requested_start_chart_id);
        if (match.song == nullptr || match.chart == nullptr) {
            context.multiplayer_state.status_message =
                "Queued chart is not installed. Download it before readying up.";
            context.multiplayer_state.local_ready = false;
            return {};
        }
        context.audio_controller.stop_preview();
        return {
            .play_request = multiplayer_play_request{
                .song = match.song,
                .chart = match.chart,
                .room_id = context.multiplayer_state.selected_room_id,
                .match_id = context.multiplayer_state.active_match_id,
            },
        };
    }

    return {};
}

}  // namespace title
