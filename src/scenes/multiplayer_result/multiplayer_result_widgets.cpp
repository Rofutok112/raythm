#include "multiplayer_result/multiplayer_result_widgets.h"

#include <algorithm>
#include <cctype>

#include "shared/avatar_texture_cache.h"
#include "theme.h"

namespace multiplayer_result::widgets {

std::string avatar_label_for(const std::string& name) {
    std::string result;
    result.reserve(2);
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "?" : result;
}

void draw_profile_image_slot(Rectangle rect,
                             const std::string& avatar_url,
                             const std::string& display_name,
                             const std::string& base_url,
                             int font_size) {
    avatar_texture_cache::draw_avatar(rect,
                                      avatar_url,
                                      avatar_label_for(display_name),
                                      with_alpha(g_theme->section, 235),
                                      g_theme->text_secondary,
                                      font_size,
                                      base_url);
}

std::string format_score(int score) {
    std::string value = std::to_string(std::max(0, score));
    for (int i = static_cast<int>(value.size()) - 3; i > 0; i -= 3) {
        value.insert(static_cast<size_t>(i), ",");
    }
    return value;
}

Color podium_rank_color(int rank_index) {
    if (rank_index == 0) {
        return g_theme->all_perfect;
    }
    if (rank_index == 1) {
        return g_theme->slow;
    }
    if (rank_index == 2) {
        return g_theme->fast;
    }
    return g_theme->text_muted;
}

}  // namespace multiplayer_result::widgets
