#pragma once

struct play_mods {
    bool auto_play = false;
    bool no_fail = false;

    [[nodiscard]] bool any_enabled() const {
        return auto_play || no_fail;
    }
};
