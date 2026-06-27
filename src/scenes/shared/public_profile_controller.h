#pragma once

#include <string>

#include "network/auth_client.h"
#include "network/friend_client.h"
#include "raylib.h"
#include "shared/public_profile_effects.h"
#include "shared/public_profile_service.h"
#include "shared/public_profile_view.h"
#include "ui_draw.h"

namespace public_profile {

struct state {
    bool open = false;
    bool closing = false;
    bool loading = false;
    bool relationship_operation_active = false;
    bool loaded_once = false;
    bool suppress_background_close_until_release = false;
    float open_anim = 0.0f;
    float link_scroll = 0.0f;
    float best_rating_scroll = 0.0f;
    public_profile_view::tab selected_tab = public_profile_view::tab::overview;
    std::string requested_user_id;
    auth::public_profile_result result;
    friend_client::operation_result relationship_result;
};

class controller {
public:
    void reset();
    void open(std::string user_id);
    void close();
    void tick(float dt);
    void poll();
    bool handle_input();
    void draw(ui::draw_layer layer = ui::draw_layer::modal, bool draw_backdrop = true);

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] Rectangle bounds() const;
    [[nodiscard]] effects consume_effects();

private:
    void request_load();
    void start_relationship_action();
    void apply_view_command(const public_profile_view::command& command);
    void apply_service_event(const service::event& event);
    void emit_notice(notice_request notice);

    state state_;
    service service_;
    effects pending_effects_;
};

}  // namespace public_profile
