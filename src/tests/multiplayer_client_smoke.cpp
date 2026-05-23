#include <cassert>
#include <string>

#include "network/multiplayer_client.h"

int main() {
    const std::string body =
        R"({"room":{"roomId":"room-1","roomCode":"ABC123","visibility":"public","hostUserId":"user-1","status":"waiting","settings":{"selectedSongId":"song-1","selectedChartId":"chart-1","keyCount":4},"members":[{"userId":"user-1","displayName":"Host","ready":true,"connected":true,"progress":{"positionMs":1200,"score":500000,"combo":42,"accuracy":98.5,"finished":false},"result":null}],"startsAt":null,"realtime":{"transport":"websocket","status":"planned","channel":"/rooms/room-1"}}})";

    const auto room = multiplayer_client::parse_room_response_for_test(body);
    assert(room.has_value());
    assert(room->room_id == "room-1");
    assert(room->room_code == "ABC123");
    assert(room->settings.selected_song_id == "song-1");
    assert(room->settings.selected_chart_id == "chart-1");
    assert(room->settings.key_count == 4);
    assert(room->members.size() == 1);
    assert(room->members[0].ready);
    assert(room->members[0].progress.score == 500000);
    assert(room->realtime_channel == "/rooms/room-1");

    const std::string songs_body =
        R"({"items":[{"id":"song-only","title":"Song Without Chart","artist":"Composer S"},{"id":"song-a","title":"Online A","artist":"Composer A"}]})";
    const auto song_items = multiplayer_client::parse_online_song_response_for_test(songs_body);
    assert(song_items.size() == 2);
    assert(song_items[0].song_id == "song-only");
    assert(song_items[0].charts.empty());

    const std::string charts_body =
        R"({"items":[{"id":"chart-a","difficultyName":"Hard","keyCount":4,"calculatedLevel":12,"chartVersion":3,"song":{"id":"song-a","title":"Online A","artist":"Composer A"}},{"id":"chart-b","difficultyName":"EX","keyCount":6,"calculatedLevel":18,"chartVersion":4,"song":{"id":"song-a","title":"Online A","artist":"Composer A"}}]})";
    const auto songs = multiplayer_client::parse_online_content_response_for_test(charts_body);
    assert(songs.size() == 1);
    assert(songs[0].song_id == "song-a");
    assert(songs[0].title == "Online A");
    assert(songs[0].charts.size() == 2);
    assert(songs[0].charts[1].chart_id == "chart-b");
    assert(songs[0].charts[1].chart_version == 4);
    return 0;
}
