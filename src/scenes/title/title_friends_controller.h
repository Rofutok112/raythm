#pragma once

#include <future>
#include <memory>
#include <optional>

#include "network/friend_client.h"
#include "raylib.h"
#include "ui_draw.h"

class title_friends_controller {
public:
    struct room_join_request {
        std::string room_id;
        std::string invite_id;
    };

    void reset();
    void open();
    void close();
    void tick(float dt);
    void poll();
    bool handle_input();
    void draw(ui::draw_layer layer = ui::draw_layer::modal);

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] int unread_badge_count() const;
    [[nodiscard]] std::optional<room_join_request> consume_room_join_request();
    [[nodiscard]] std::optional<std::string> consume_profile_request();

private:
    enum class tab {
        friends,
        requests,
        invites,
    };

    struct state {
        bool open = false;
        bool closing = false;
        bool loading = false;
        bool operation_active = false;
        bool loaded_once = false;
        bool suppress_background_close_until_release = false;
        float open_anim = 0.0f;
        tab selected_tab = tab::friends;
        friend_client::friend_listing friends;
        friend_client::request_listing requests;
        friend_client::invite_listing invites;
        std::optional<room_join_request> pending_room_join;
        std::optional<std::string> pending_profile_user_id;
        std::string message;
    };

    void request_reload();
    void start_accept_request(std::string request_id);
    void start_decline_request(std::string request_id);
    void start_remove_friend(std::string user_id);
    void start_block_user(std::string user_id);
    void start_accept_invite(std::string invite_id);
    void start_decline_invite(std::string invite_id);
    void apply_operation_result(const friend_client::operation_result& result);
    void ensure_social_realtime();
    void poll_social_realtime();
    void apply_social_event(const friend_client::social_realtime_event& event);

    state state_;
    std::future<friend_client::friend_listing_result> friends_future_;
    std::future<friend_client::request_listing_result> requests_future_;
    std::future<friend_client::invite_listing_result> invites_future_;
    std::future<friend_client::operation_result> operation_future_;
    std::future<friend_client::invite_operation_result> invite_operation_future_;
    std::unique_ptr<friend_client::social_realtime_client> social_realtime_;
    float social_reconnect_t_ = 0.0f;
    float social_ping_t_ = 0.0f;
};
