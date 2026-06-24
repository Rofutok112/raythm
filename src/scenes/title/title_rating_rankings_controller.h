#pragma once

#include <future>
#include <optional>
#include <string>

#include "network/auth_client.h"
#include "title/title_rating_rankings_view.h"
#include "ui_draw.h"
#include "ui_notice.h"

class title_rating_rankings_controller {
public:
    void reset();
    void open();
    void close();
    void request_reload();
    void tick(float dt);
    void poll();
    bool handle_input();
    void draw(ui::draw_layer layer = ui::draw_layer::modal, bool draw_backdrop = true);

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] Rectangle bounds() const;
    [[nodiscard]] std::optional<std::string> consume_profile_user_id();

private:
    void request_page(int page);
    void apply_result(auth::global_rating_rankings_result result);
    void apply_command(const title_rating_rankings_view::command& command);

    title_rating_rankings_view::model state_;
    bool open_ = false;
    bool closing_ = false;
    bool suppress_background_close_until_release_ = false;
    std::future<auth::global_rating_rankings_result> load_future_;
    std::optional<std::string> pending_profile_user_id_;
};
