#pragma once

#include <memory>
#include <string>
#include <vector>

#include "multiplayer/multiplayer_state.h"

namespace multiplayer::client {

class realtime_client {
public:
    realtime_client();
    ~realtime_client();

    realtime_client(const realtime_client&) = delete;
    realtime_client& operator=(const realtime_client&) = delete;

    bool connect(const auth::session_summary& session, const std::string& room_id);
    void close();
    bool connected() const;
    const std::string& room_id() const;
    bool send_command(const std::string& command, const std::string& body = "{}");
    std::vector<room_operation_result> poll_room_events();
    std::string last_error() const;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

room_list_result fetch_room_list(const auth::session_summary& session);
room_operation_result create_room(const auth::session_summary& session,
                                  const std::string& name,
                                  const std::string& password,
                                  int max_players,
                                  bool host_only);
room_operation_result join_room(const auth::session_summary& session,
                                const std::string& room_id,
                                const std::string& password);
room_operation_result fetch_room(const auth::session_summary& session, const std::string& room_id);
room_operation_result leave_room(const auth::session_summary& session, const std::string& room_id);
room_operation_result set_ready(const auth::session_summary& session, const std::string& room_id, bool ready);
room_operation_result send_chat(const auth::session_summary& session,
                                const std::string& room_id,
                                const std::string& message);
room_operation_result add_queue_item(const auth::session_summary& session,
                                     const std::string& room_id,
                                     const std::string& chart_id,
                                     int chart_version);
room_operation_result remove_queue_item(const auth::session_summary& session,
                                        const std::string& room_id,
                                        const std::string& item_id);
room_operation_result reorder_queue_item(const auth::session_summary& session,
                                         const std::string& room_id,
                                         const std::string& item_id,
                                         bool move_up);
room_operation_result set_queue_permission(const auth::session_summary& session,
                                           const std::string& room_id,
                                           bool host_only);
room_operation_result start_match(const auth::session_summary& session, const std::string& room_id);
room_operation_result complete_match(const auth::session_summary& session, const std::string& match_id);
room_operation_result update_score(const auth::session_summary& session,
                                   const std::string& room_id,
                                   const std::string& match_id,
                                   int score,
                                   int combo,
                                   bool failed = false);

}  // namespace multiplayer::client
