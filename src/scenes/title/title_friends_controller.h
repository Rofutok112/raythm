#pragma once

#include "network/friend_client.h"
#include "title/title_friends_effects.h"
#include "title/title_friends_service.h"
#include "title/title_friends_state.h"
#include "title/title_friends_view.h"
#include "ui_draw.h"

class title_friends_controller {
public:
    void reset();
    void open();
    void close();
    void request_reload();
    void tick(float dt);
    void poll();
    bool handle_input();
    void draw(ui::draw_layer layer = ui::draw_layer::modal);

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] int unread_badge_count() const;
    [[nodiscard]] title_friends_effects::feature_effects consume_effects();

private:
    struct state {
        bool open = false;
        bool closing = false;
        bool loading = false;
        bool operation_active = false;
        bool loaded_once = false;
        bool suppress_background_close_until_release = false;
        float open_anim = 0.0f;
        title_friends_view::tab selected_tab = title_friends_view::tab::friends;
        title_friends_state::social_state social;
        title_friends_effects::feature_effects pending_effects;
        std::string message;
    };

    void start_accept_request(std::string request_id);
    void start_decline_request(std::string request_id);
    void start_remove_friend(std::string user_id);
    void start_block_user(std::string user_id);
    void start_accept_invite(std::string invite_id);
    void start_read_invite(std::string invite_id);
    void start_decline_invite(std::string invite_id);
    void emit_notice(title_friends_effects::notice_request notice);
    void apply_view_command(const title_friends_view::command& command);
    void apply_operation_result(const friend_client::operation_result& result);
    void apply_service_event(const title_friends_service::event& event);
    void apply_social_event(const friend_client::social_realtime_event& event);

    state state_;
    title_friends_service service_;
};
