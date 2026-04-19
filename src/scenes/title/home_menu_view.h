#pragma once

#include <cstddef>
#include <string_view>

#include "raylib.h"

namespace title_home_view {

enum class action {
    play,
    multiplayer,
    online,
    create,
};

struct entry {
    const char* label;
    const char* detail;
    bool enabled;
    action target;
};

std::size_t entry_count();
const entry& entry_at(std::size_t index);
Rectangle button_rect(int index, float anim_t);
void draw(float menu_anim_t, float play_anim_t, int selected_index, std::string_view status_message);

}  // namespace title_home_view
