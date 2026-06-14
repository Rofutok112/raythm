#pragma once

#include <string>
#include <utility>

namespace title {

enum class command_type {
    none,
    enter_home,
    open_update_catalog,
    add_selected_to_multiplayer,
    open_self_profile,
    open_public_profile,
    open_multiplayer_song_select,
};

struct command {
    command_type type = command_type::none;
    std::string song_id;
    std::string chart_id;
    std::string user_id;

    [[nodiscard]] static command enter_home() {
        return {.type = command_type::enter_home};
    }

    [[nodiscard]] static command open_update_catalog(std::string song_id, std::string chart_id) {
        return {
            .type = command_type::open_update_catalog,
            .song_id = std::move(song_id),
            .chart_id = std::move(chart_id),
        };
    }

    [[nodiscard]] static command add_selected_to_multiplayer() {
        return {.type = command_type::add_selected_to_multiplayer};
    }

    [[nodiscard]] static command open_self_profile() {
        return {.type = command_type::open_self_profile};
    }

    [[nodiscard]] static command open_public_profile(std::string user_id) {
        return {
            .type = command_type::open_public_profile,
            .user_id = std::move(user_id),
        };
    }

    [[nodiscard]] static command open_multiplayer_song_select() {
        return {.type = command_type::open_multiplayer_song_select};
    }

    [[nodiscard]] bool has_value() const {
        return type != command_type::none;
    }
};

}  // namespace title
