#include "title/home_menu_view.h"

#include <algorithm>
#include <array>

#include "scene_common.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace {

constexpr float kHomeButtonWidth = 348.0f;
constexpr float kHomeButtonHeight = 117.0f;
constexpr float kHomeButtonGap = 27.0f;
constexpr float kHomeButtonRowY = 564.0f;
constexpr float kHomeButtonIntroOffsetY = 36.0f;
constexpr float kPlayTransitionOffsetY = 51.0f;
constexpr float kButtonBorderWidth = 2.7f;
constexpr float kTitlePaddingX = 21.0f;
constexpr float kTitleOffsetY = 18.0f;
constexpr float kTitleHeight = 36.0f;
constexpr float kDetailPaddingX = 24.0f;
constexpr float kDetailOffsetY = 63.0f;
constexpr float kDetailHeight = 27.0f;
constexpr float kStatusOffsetY = 33.0f;
constexpr float kStatusHeight = 27.0f;

struct home_entry_card_layout {
    Rectangle title;
    Rectangle detail;
};

struct home_entry_card {
    const title_home_view::entry* entry = nullptr;
    Rectangle rect{};
    bool selected = false;
    float fade = 1.0f;
};

struct home_entry_card_style {
    Color background{};
    Color border{};
};

constexpr std::array<title_home_view::entry, 4> kHomeEntries = {{
    {"PLAY", "Solo song select.", true, title_home_view::action::play},
    {"MULTIPLAY", "Room battles.", true, title_home_view::action::multiplayer},
    {"BROWSE", "Browse and download.", true, title_home_view::action::online},
    {"CREATE", "Create, import, export.", true, title_home_view::action::create},
}};

home_entry_card_layout make_home_entry_card_layout(Rectangle rect) {
    return {
        {
            rect.x + kTitlePaddingX,
            rect.y + kTitleOffsetY,
            rect.width - kTitlePaddingX * 2.0f,
            kTitleHeight,
        },
        {
            rect.x + kDetailPaddingX,
            rect.y + kDetailOffsetY,
            rect.width - kDetailPaddingX * 2.0f,
            kDetailHeight,
        },
    };
}

Rectangle status_message_rect() {
    return {
        0.0f,
        kHomeButtonRowY + kHomeButtonHeight + kStatusOffsetY,
        static_cast<float>(kScreenWidth),
        kStatusHeight,
    };
}

std::array<Rectangle, kHomeEntries.size()> home_button_rects(float anim_t) {
    const float eased = tween::ease_out_cubic(anim_t);
    std::array<Rectangle, kHomeEntries.size()> buttons{};
    ui::centered_hstack({0.0f,
                         kHomeButtonRowY + (1.0f - eased) * kHomeButtonIntroOffsetY,
                         static_cast<float>(kScreenWidth),
                         kHomeButtonHeight},
                        kHomeButtonWidth,
                        kHomeButtonHeight,
                        kHomeButtonGap,
                        buttons);
    return buttons;
}

home_entry_card home_entry_card_for(const title_home_view::entry& entry,
                                    Rectangle raw_rect,
                                    int index,
                                    int selected_index,
                                    float play_anim_t,
                                    float menu_t) {
    return {
        .entry = &entry,
        .rect = ui::translated(raw_rect, 0.0f, -kPlayTransitionOffsetY * play_anim_t),
        .selected = index == selected_index,
        .fade = (1.0f - play_anim_t) * menu_t,
    };
}

home_entry_card_style home_entry_card_style_for(const home_entry_card& card) {
    const auto& t = *g_theme;
    const bool enabled = card.entry != nullptr && card.entry->enabled;
    const unsigned char base_alpha =
        static_cast<unsigned char>(static_cast<float>(t.row_soft_alpha) * card.fade);
    const unsigned char selected_alpha =
        static_cast<unsigned char>(static_cast<float>(t.row_soft_selected_alpha) * card.fade);
    const Color button_base = t.row_soft;
    const Color button_selected = t.row_soft_selected;
    return {
        .background = !enabled
            ? with_alpha(button_base,
                         static_cast<unsigned char>(static_cast<float>(t.row_soft_alpha) * 0.85f * card.fade))
            : (card.selected ? with_alpha(button_selected, selected_alpha)
                             : with_alpha(button_base, base_alpha)),
        .border = !enabled
            ? with_alpha(t.border_light, static_cast<unsigned char>(180.0f * card.fade))
            : (card.selected ? with_alpha(t.border_active, static_cast<unsigned char>(255.0f * card.fade))
                             : with_alpha(t.border, static_cast<unsigned char>(230.0f * card.fade))),
    };
}

void draw_home_entry_card(const home_entry_card& card,
                          const home_entry_card_style& style) {
    if (card.entry == nullptr) {
        return;
    }
    const title_home_view::entry& current = *card.entry;
    const auto& t = *g_theme;
    const home_entry_card_layout layout = make_home_entry_card_layout(card.rect);
    ui::surface(card.rect, style.background, style.border, kButtonBorderWidth, {
        .fill = style.background,
        .border_color = style.border,
        .custom_colors = true,
    });
    ui::draw_text_in_rect(current.label, 24,
                          layout.title,
                          with_alpha(current.enabled ? t.text : t.text_muted,
                                     static_cast<unsigned char>(255.0f * card.fade)),
                          ui::text_align::center);
    ui::draw_text_in_rect(current.detail, 13,
                          layout.detail,
                          with_alpha(current.enabled ? t.text_muted : t.text_hint,
                                     static_cast<unsigned char>(220.0f * card.fade)),
                          ui::text_align::center);
}

void draw_home_entry_cards(float menu_anim_t, float play_anim_t, int selected_index, float menu_t) {
    const std::array<Rectangle, kHomeEntries.size()> buttons = home_button_rects(menu_anim_t);
    for (int index = 0; index < static_cast<int>(kHomeEntries.size()); ++index) {
        const title_home_view::entry& current = kHomeEntries[static_cast<std::size_t>(index)];
        const Rectangle raw_rect = buttons[static_cast<std::size_t>(index)];
        const home_entry_card card =
            home_entry_card_for(current, raw_rect, index, selected_index, play_anim_t, menu_t);
        if (card.fade <= 0.01f) {
            continue;
        }
        draw_home_entry_card(card, home_entry_card_style_for(card));
    }
}

void draw_status_message(std::string_view status_message, float menu_t, float play_anim_t) {
    if (status_message.empty() || (1.0f - play_anim_t) <= 0.01f) {
        return;
    }
    ui::draw_text_in_rect(status_message.data(), 16, status_message_rect(),
                          with_alpha(g_theme->text_muted,
                                     static_cast<unsigned char>(230.0f * menu_t * (1.0f - play_anim_t))),
                          ui::text_align::center);
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
    const std::array<Rectangle, kHomeEntries.size()> buttons = home_button_rects(anim_t);
    return buttons[static_cast<std::size_t>(index)];
}

void draw(float menu_anim_t, float play_anim_t, int selected_index, std::string_view status_message) {
    const float menu_t = tween::ease_out_cubic(menu_anim_t);
    if (menu_t <= 0.01f) {
        return;
    }

    draw_home_entry_cards(menu_anim_t, play_anim_t, selected_index, menu_t);
    draw_status_message(status_message, menu_t, play_anim_t);
}

}  // namespace title_home_view
