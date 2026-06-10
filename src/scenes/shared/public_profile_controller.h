#pragma once

#include <future>
#include <string>

#include "network/auth_client.h"
#include "raylib.h"
#include "ui_draw.h"

namespace public_profile {

struct state {
    bool open = false;
    bool closing = false;
    bool loading = false;
    bool loaded_once = false;
    bool suppress_background_close_until_release = false;
    float open_anim = 0.0f;
    std::string requested_user_id;
    auth::public_profile_result result;
};

class controller {
public:
    void reset();
    void open(std::string user_id);
    void close();
    void tick(float dt);
    void poll();
    bool handle_input();
    void draw(ui::draw_layer layer = ui::draw_layer::modal);

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] Rectangle bounds() const;

private:
    void request_load();

    state state_;
    std::future<auth::public_profile_result> load_future_;
};

}  // namespace public_profile
