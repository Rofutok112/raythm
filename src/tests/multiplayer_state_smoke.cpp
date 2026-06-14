#include <cassert>
#include <string>

#include "multiplayer/multiplayer_state.h"

namespace {

multiplayer::room_detail make_room(std::string status) {
    multiplayer::room_detail room;
    room.id = "room-1";
    room.status = std::move(status);
    return room;
}

}  // namespace

int main() {
    const multiplayer::room_detail open = make_room("OPEN");
    assert(multiplayer::can_invite_friends(open));

    const multiplayer::room_detail in_match = make_room("IN_MATCH");
    assert(!multiplayer::can_invite_friends(in_match));
    assert(std::string(multiplayer::invite_friends_unavailable_message(in_match)) ==
           "Invites are available after the match.");

    const multiplayer::room_detail closed = make_room("CLOSED");
    assert(!multiplayer::can_invite_friends(closed));
    assert(std::string(multiplayer::invite_friends_unavailable_message(closed)) == "This room is closed.");

    const multiplayer::room_detail unknown = make_room("");
    assert(!multiplayer::can_invite_friends(unknown));
    assert(std::string(multiplayer::invite_friends_unavailable_message(unknown)) ==
           "Invites are available in open rooms.");

    return 0;
}
