#include "title/home_menu_view.h"

#include <algorithm>
#include <array>

#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"

namespace {

constexpr float kHomeButtonWidth = 232.0f;
constexpr float kHomeButtonHeight = 78.0f;
constexpr float kHomeButtonGap = 18.0f;
constexpr float kHomeButtonRowY = 376.0f;
constexpr float kHomeButtonIntroOffsetY = 24.0f;

constexpr std::array<title_home_view::entry, 4> kHomeEntries = {{
    {"PLAY", "Solo song select.", true, title_home_view::action::play},
    {"MULTIPLAY", "Room battles soon.", false, title_home_view::action::multiplayer},
    {"ONLINE", "Browse and download.", true, title_home_view::action::online},
    {"CREATE", "Create, import, export.", true, title_home_view::action::create},
}};

float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

Rectangle translate_rect(Rectangle rect, float dx, float dy) {
    rect.x += dx;
    rect.y += dy;
    return rect;
}

}  // namespace

namespace title_home_view {

std::size_t entry_count() {
    return kHomeEntries.size();
}

const entry& entry_at(std::size_t index) {
    return kHomeEntries.at(index);
}

Rectangle button_rect(int index, float anim_t) {
    const float eased = ease_out_cubic(anim_t);
    const float total_width =
        static_cast<float>(kHomeEntries.size()) * kHomeButtonWidth +
        static_cast<float>(kHomeEntries.size() - 1) * kHomeButtonGap;
    return {
        (static_cast<float>(kScreenWidth) - total_width) * 0.5f +
            static_cast<float>(index) * (kHomeButtonWidth + kHomeButtonGap),
        kHomeButtonRowY + (1.0f - eased) * kHomeButtonIntroOffsetY,
        kHomeButtonWidth,
        kHomeButtonHeight
    };
}

void draw(float menu_anim_t, float play_anim_t, int selected_index, std::string_view status_message) {
    const auto& t = *g_theme;
    const float menu_t = ease_out_cubic(menu_anim_t);
    if (menu_t <= 0.01f) {
        return;
    }

    for (int index = 0; index < static_cast<int>(kHomeEntries.size()); ++index) {
        const entry& current = kHomeEntries[static_cast<std::size_t>(index)];
        const Rectangle raw_rect = button_rect(index, menu_anim_t);
        const Rectangle rect = translate_rect(raw_rect, 0.0f, -34.0f * play_anim_t);
        const bool selected = index == selected_index;
        const float button_fade = (1.0f - play_anim_t) * menu_t;
        const Color button_base = t.row_soft;
        const Color button_selected = t.row_soft_selected;
        const unsigned char base_alpha =
            static_cast<unsigned char>(static_cast<float>(t.row_soft_alpha) * button_fade);
        const unsigned char selected_alpha =
            static_cast<unsigned char>(static_cast<float>(t.row_soft_selected_alpha) * button_fade);
        const Color bg = !current.enabled
            ? with_alpha(button_base, static_cast<unsigned char>(static_cast<float>(t.row_soft_alpha) * 0.85f * button_fade))
            : (selected ? with_alpha(button_selected, selected_alpha)
                        : with_alpha(button_base, base_alpha));
        const Color border = !current.enabled
            ? with_alpha(t.border_light, static_cast<unsigned char>(180.0f * button_fade))
            : (selected ? with_alpha(t.border_active, static_cast<unsigned char>(255.0f * button_fade))
                        : with_alpha(t.border, static_cast<unsigned char>(230.0f * button_fade)));
        if (button_fade <= 0.01f) {
            continue;
        }
        DrawRectangleRec(rect, bg);
        DrawRectangleLinesEx(rect, 1.8f, border);
        ui::draw_text_in_rect(current.label, 24,
                              {rect.x + 14.0f, rect.y + 12.0f, rect.width - 28.0f, 24.0f},
                              with_alpha(current.enabled ? t.text : t.text_muted, static_cast<unsigned char>(255.0f * button_fade)),
                              ui::text_align::center);
        ui::draw_text_in_rect(current.detail, 13,
                              {rect.x + 16.0f, rect.y + 42.0f, rect.width - 32.0f, 18.0f},
                              with_alpha(current.enabled ? t.text_muted : t.text_hint, static_cast<unsigned char>(220.0f * button_fade)),
                              ui::text_align::center);
    }

    if (!status_message.empty() && (1.0f - play_anim_t) > 0.01f) {
        ui::draw_text_in_rect(status_message.data(), 16,
                              {0.0f, kHomeButtonRowY + kHomeButtonHeight + 22.0f,
                               static_cast<float>(kScreenWidth), 18.0f},
                              with_alpha(t.text_muted, static_cast<unsigned char>(230.0f * menu_t * (1.0f - play_anim_t))),
                              ui::text_align::center);
    }
}

}  // namespace title_home_view
