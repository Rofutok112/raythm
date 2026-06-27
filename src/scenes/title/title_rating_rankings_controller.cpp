#include "title/title_rating_rankings_controller.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

#include "localization/localization.h"
#include "raylib.h"
#include "tween.h"

void title_rating_rankings_controller::reset() {
    state_ = {};
    open_ = false;
    closing_ = false;
    suppress_background_close_until_release_ = false;
    pending_profile_user_id_.reset();
}

void title_rating_rankings_controller::open() {
    if (open_ && !closing_) {
        return;
    }
    open_ = true;
    closing_ = false;
    state_.avatar_base_url = auth::load_session_summary().server_url;
    suppress_background_close_until_release_ = true;
    if (!state_.loaded_once && !state_.loading) {
        request_page(1);
    }
}

void title_rating_rankings_controller::close() {
    if (open_) {
        closing_ = true;
    }
}

void title_rating_rankings_controller::request_reload() {
    request_page(state_.page);
}

void title_rating_rankings_controller::tick(float dt) {
    if (open_ && closing_) {
        state_.open_anim = tween::retreat(state_.open_anim, dt, 8.0f);
        if (state_.open_anim <= 0.0f) {
            open_ = false;
            closing_ = false;
        }
    } else if (open_) {
        state_.open_anim = tween::advance(state_.open_anim, dt, 8.0f);
    } else {
        state_.open_anim = 0.0f;
    }
}

void title_rating_rankings_controller::poll() {
    if (!load_future_.valid()) {
        return;
    }
    if (load_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }
    apply_result(load_future_.get());
}

bool title_rating_rankings_controller::handle_input() {
    if (!open_) {
        return false;
    }
    if (suppress_background_close_until_release_) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            return true;
        }
        suppress_background_close_until_release_ = false;
    }
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && !state_.loading) {
        close();
        return true;
    }
    state_.scroll_offset = title_rating_rankings_view::clamped_scroll_offset(state_);
    const title_rating_rankings_view::input_result input =
        title_rating_rankings_view::handle_input(state_, ui::draw_layer::modal);
    if (input.scroll_offset_changed) {
        state_.scroll_offset = input.scroll_offset;
    }
    if (input.action.type != title_rating_rankings_view::command_type::none) {
        apply_command(input.action);
    }
    return true;
}

void title_rating_rankings_controller::draw(ui::draw_layer layer, bool draw_backdrop) {
    if (!open_) {
        return;
    }
    title_rating_rankings_view::draw(state_, layer, draw_backdrop);
}

bool title_rating_rankings_controller::is_open() const {
    return open_;
}

Rectangle title_rating_rankings_controller::bounds() const {
    return title_rating_rankings_view::modal_bounds(state_.open_anim);
}

std::optional<std::string> title_rating_rankings_controller::consume_profile_user_id() {
    std::optional<std::string> result = std::move(pending_profile_user_id_);
    pending_profile_user_id_.reset();
    return result;
}

void title_rating_rankings_controller::request_page(int page) {
    if (state_.loading) {
        return;
    }
    state_.loading = true;
    state_.message = localization::tr_literal("Loading...");
    state_.page = std::max(1, page);
    state_.scroll_offset = 0.0f;
    std::promise<auth::global_rating_rankings_result> promise;
    load_future_ = promise.get_future();
    std::thread([promise = std::move(promise), page = state_.page, page_size = state_.page_size]() mutable {
        promise.set_value(auth::fetch_global_rating_rankings(page, page_size));
    }).detach();
}

void title_rating_rankings_controller::apply_result(auth::global_rating_rankings_result result) {
    state_.loading = false;
    state_.loaded_once = true;
    if (!result.success) {
        state_.message = result.message.empty() ? localization::tr_literal("Failed to load rating rankings.")
                                                : result.message;
        ui::notify(state_.message, ui::notice_tone::error, 2.8f);
        return;
    }
    state_.page = result.page;
    state_.page_size = result.page_size;
    state_.total = result.total;
    state_.has_next_page = result.has_next_page;
    state_.items = std::move(result.items);
    state_.message = state_.items.empty() ? localization::tr_literal("No rated players.") : "";
    state_.scroll_offset = title_rating_rankings_view::clamped_scroll_offset(state_);
}

void title_rating_rankings_controller::apply_command(const title_rating_rankings_view::command& command) {
    switch (command.type) {
    case title_rating_rankings_view::command_type::none:
        break;
    case title_rating_rankings_view::command_type::close:
        close();
        break;
    case title_rating_rankings_view::command_type::refresh:
        request_reload();
        break;
    case title_rating_rankings_view::command_type::previous_page:
        request_page(state_.page - 1);
        break;
    case title_rating_rankings_view::command_type::next_page:
        request_page(state_.page + 1);
        break;
    case title_rating_rankings_view::command_type::open_profile:
        pending_profile_user_id_ = command.user_id;
        break;
    }
}
