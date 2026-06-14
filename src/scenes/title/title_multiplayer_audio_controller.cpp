#include "title/title_multiplayer_audio_controller.h"

#include <chrono>
#include <thread>

#include "network/server_environment.h"
#include "title/online_download_internal.h"
#include "title/online_download_remote_client.h"
#include "title/title_multiplayer_content_resolver.h"

namespace {

const multiplayer::room_queue_item* queue_preview_item(const multiplayer::state& multiplayer_state) {
    if (!multiplayer_state.current_room.has_value() || multiplayer_state.current_room->queue.empty()) {
        return nullptr;
    }
    return &multiplayer_state.current_room->queue.front();
}

const song_select::song_entry* queue_preview_song(const multiplayer::state& multiplayer_state,
                                                  const song_select::state& play_state,
                                                  const local_content_index::snapshot& multiplayer_local_index) {
    const multiplayer::room_queue_item* item = queue_preview_item(multiplayer_state);
    if (item == nullptr) {
        return nullptr;
    }
    const std::string room_server_url = server_environment::normalize_url(multiplayer_state.auth.server_url);
    const title::local_chart_match match =
        title::find_online_chart_match(play_state, multiplayer_local_index, room_server_url, item->song_id, item->chart_id);
    return match.song;
}

std::optional<song_select::song_entry> fetch_remote_preview_song_entry(std::string song_id,
                                                                       std::string server_url) {
    const title_online_view::remote_song_lookup_result result =
        title_online_view::fetch_remote_song_by_id(song_id, server_url);
    if (!result.success || result.song.audio_url.empty()) {
        return std::nullopt;
    }
    return title_online_view::detail::make_remote_song_entry(result.song, result.server_url);
}

const song_select::song_entry* remote_queue_preview_song(title::multiplayer_audio_state& audio_state,
                                                        const multiplayer::state& multiplayer_state,
                                                        const multiplayer::room_queue_item& item) {
    if (item.song_id.empty()) {
        title::reset_multiplayer_audio(audio_state);
        return nullptr;
    }

    const std::string room_server_url = server_environment::normalize_url(multiplayer_state.auth.server_url);
    const std::string preview_key = room_server_url + "\n" + item.song_id;
    if (preview_key != audio_state.remote_preview_key) {
        title::reset_multiplayer_audio(audio_state);
        audio_state.remote_preview_key = preview_key;
        std::promise<std::optional<song_select::song_entry>> promise;
        audio_state.remote_preview_future = promise.get_future();
        const std::string song_id = item.song_id;
        std::thread([promise = std::move(promise), song_id, room_server_url]() mutable {
            try {
                promise.set_value(fetch_remote_preview_song_entry(song_id, room_server_url));
            } catch (...) {
                promise.set_value(std::nullopt);
            }
        }).detach();
    }

    if (audio_state.remote_preview_future.valid() &&
        audio_state.remote_preview_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try {
            audio_state.remote_preview_song = audio_state.remote_preview_future.get();
        } catch (...) {
            audio_state.remote_preview_song.reset();
        }
    }

    return audio_state.remote_preview_song.has_value() ? &*audio_state.remote_preview_song : nullptr;
}

}  // namespace

namespace title {

void reset_multiplayer_audio(multiplayer_audio_state& audio_state) {
    audio_state.remote_preview_key.clear();
    audio_state.remote_preview_song.reset();
    audio_state.remote_preview_future = {};
}

void update_multiplayer_audio(multiplayer_audio_state& audio_state,
                              multiplayer::state& multiplayer_state,
                              const song_select::state& play_state,
                              const local_content_index::snapshot& multiplayer_local_index,
                              title_audio_controller& audio_controller,
                              float dt) {
    const multiplayer::room_queue_item* preview_item = queue_preview_item(multiplayer_state);
    const song_select::song_entry* preview_song = queue_preview_song(
        multiplayer_state, play_state, multiplayer_local_index);
    if (preview_song != nullptr) {
        reset_multiplayer_audio(audio_state);
    } else if (preview_item != nullptr) {
        preview_song = remote_queue_preview_song(audio_state, multiplayer_state, *preview_item);
    } else {
        reset_multiplayer_audio(audio_state);
    }

    const std::string preview_song_id = preview_song != nullptr ? preview_song->song.meta.song_id : "";
    if (preview_song_id != audio_state.preview_song_id) {
        audio_state.preview_song_id = preview_song_id;
        audio_controller.stop_preview();
        if (preview_song != nullptr) {
            audio_controller.resume_preview_song(preview_song);
        }
    }

    if (multiplayer_state.queue_preview_seek_requested) {
        multiplayer_state.queue_preview_seek_requested = false;
        if (preview_song != nullptr &&
            audio_controller.preview_snapshot(preview_song).audio.status ==
                song_select::preview_audio_loader::load_status::ready) {
            audio_controller.seek_preview(multiplayer_state.queue_preview_seek_seconds);
        }
    }
    if (multiplayer_state.command == multiplayer::ui_command::toggle_queue_preview) {
        multiplayer_state.command = multiplayer::ui_command::none;
        audio_controller.toggle_preview_song(preview_song);
    }

    audio_controller.update_multiplayer_preview(preview_song, dt);
    multiplayer_state.queue_preview_available = preview_song != nullptr;
    const title_preview_snapshot preview = audio_controller.preview_snapshot(preview_song);
    multiplayer_state.queue_preview_playing = preview.playing;
    multiplayer_state.queue_preview_position_seconds = preview_song != nullptr ? preview.position_seconds : 0.0;
    multiplayer_state.queue_preview_duration_seconds = preview_song != nullptr ? preview.length_seconds : 0.0;
}

}  // namespace title
