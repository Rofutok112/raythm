#pragma once

#include <future>
#include <optional>
#include <string>

#include "network/auth_client.h"
#include "song_select/song_select_state.h"
#include "title/profile_view.h"

class title_profile_controller {
public:
    struct poll_result {
        bool content_changed = false;
    };

    struct input_result {
        bool consumed = false;
        std::optional<std::string> delete_account_password;
    };

    void reset();
    void open();
    void close();
    void close_if_logged_out(bool logged_in);
    void tick(float dt);
    poll_result poll();
    input_result handle_input(bool auth_request_active);
    void draw(const song_select::auth_state& auth_state, bool auth_request_active, ui::draw_layer layer);

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] Rectangle bounds() const;

private:
    void request_reload();
    void start_delete_song(std::string song_id);
    void start_delete_chart(std::string chart_id);

    title_profile_view::state state_;
    std::future<title_profile_view::load_result> load_future_;
    std::future<auth::operation_result> delete_future_;
};
