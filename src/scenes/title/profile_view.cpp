#include "title/profile_view.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "content_lifecycle.h"
#include "localization/localization.h"
#include "scene_common.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "title/title_layout.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

#include "rlgl.h"

namespace title_profile_view {
namespace {

constexpr Rectangle kDialogRect = {210.0f, 96.0f, 1500.0f, 888.0f};
constexpr float kHeaderHeight = 190.0f;
constexpr float kTabHeight = 46.0f;
constexpr float kContentTopGap = 30.0f;
constexpr float kContentOuterPadding = 42.0f;
constexpr float kOpenAnimOffsetY = 30.0f;
constexpr float kOpenAnimScaleInset = 0.035f;
constexpr int kEditableExternalLinkCount = 3;
constexpr float kHeaderAvatarSize = 96.0f;
constexpr Vector2 kHeaderAvatarOffset = {44.0f, 42.0f};
constexpr float kHeaderInfoGap = 24.0f;
constexpr float kHeaderInfoWidth = 620.0f;
constexpr float kHeaderCountXOffset = 900.0f;
constexpr float kTabBarInsetX = 42.0f;
constexpr float kRowHeight = 70.0f;
constexpr float kRowGap = 10.0f;
constexpr float kWheelStep = 78.0f;
constexpr float kStatusLeftInset = 42.0f;
constexpr float kStatusBottomOffset = 104.0f;
constexpr float kSettingsInsetX = 18.0f;
constexpr float kSettingsDividerY = 306.0f;
constexpr float kSettingsAvatarLabelY = 316.0f;
constexpr float kSettingsAvatarButtonY = 354.0f;
constexpr float kSettingsAccountDividerY = 432.0f;
constexpr float kSettingsAccountLabelY = 446.0f;
constexpr float kSettingsAccountDescriptionY = 470.0f;
constexpr float kSettingsAccountButtonY = 502.0f;
constexpr float kSettingsLabelHeight = 22.0f;
constexpr float kSettingsButtonHeight = 42.0f;
constexpr float kSettingsTitleY = 18.0f;
constexpr float kSettingsTitleHeight = 30.0f;
constexpr float kSettingsLinkHeadingY = 48.0f;
constexpr float kSettingsLinkHeadingHeight = 22.0f;
constexpr float kSettingsLinkRowY = 76.0f;
constexpr float kSettingsLinkRowHeight = 42.0f;
constexpr float kSettingsLinkRowStep = 58.0f;
constexpr float kSettingsLinkLabelWidth = 330.0f;
constexpr float kSettingsLinkGap = 20.0f;
constexpr float kSettingsLinkHintY = 262.0f;
constexpr float kSettingsTextWidth = 560.0f;
constexpr float kUploadTextLeftInset = 18.0f;
constexpr float kUploadTextActionGap = 30.0f;
constexpr float kOverviewMetricGap = 12.0f;
constexpr float kOverviewMetricHeight = 92.0f;
constexpr float kOverviewSummaryTop = 118.0f;
constexpr float kOverviewSummaryHeight = 112.0f;
constexpr float kOverviewSummaryGap = 12.0f;
constexpr float kSummaryLeftInset = 18.0f;
constexpr float kSummaryPrimaryWidth = 540.0f;
constexpr float kSummaryValueX = 650.0f;
constexpr float kItemDeletePanelWidth = 600.0f;
constexpr float kItemDeletePanelHeight = 74.0f;
constexpr Vector2 kItemDeletePanelOffset = {540.0f, -124.0f};
constexpr float kAccountDeletePanelWidth = 560.0f;
constexpr float kAccountDeletePanelHeight = 232.0f;
constexpr ui::row_action_layout_options kUploadRowActionLayout{
    .button_width = 92.0f,
    .button_height = 42.0f,
    .button_gap = 12.0f,
    .right_padding = 20.0f,
};

struct profile_tab_definition {
    tab value;
    const char* label;
};

constexpr std::array<profile_tab_definition, 6> kProfileTabs = {{
    {tab::overview, "OVERVIEW"},
    {tab::activity, "ACTIVITY"},
    {tab::best_rc, "BEST RC"},
    {tab::songs, "SONGS"},
    {tab::charts, "CHARTS"},
    {tab::settings, "SETTINGS"},
}};

struct upload_row_layout {
    Rectangle title;
    Rectangle subtitle;
    Rectangle delete_button;
};

struct activity_row_layout {
    Rectangle title;
    Rectangle subtitle;
    Rectangle local_summary;
    Rectangle online_summary;
};

struct best_rc_row_layout {
    Rectangle rank;
    Rectangle title;
    Rectangle subtitle;
    Rectangle summary;
    Rectangle online_summary;
};

struct summary_card_layout {
    Rectangle heading;
    Rectangle empty_message;
    Rectangle title;
    Rectangle subtitle;
    Rectangle local_summary;
    Rectangle online_summary;
    Rectangle rc_summary;
};

struct overview_layout {
    Rectangle metric_cards[4];
    Rectangle recent_activity;
    Rectangle top_rc;
};

struct settings_link_row_layout {
    Rectangle label;
    Rectangle url;
};

struct settings_layout {
    Rectangle title;
    Rectangle link_heading;
    Rectangle save_links_button;
    Rectangle link_hint;
    Rectangle avatar_divider;
    Rectangle avatar_label;
    Rectangle change_avatar_button;
    Rectangle remove_avatar_button;
    Rectangle account_divider;
    Rectangle account_label;
    Rectangle account_description;
    Rectangle delete_account_button;
};

enum class profile_button_tone {
    accent,
    error,
    muted,
};

struct settings_action_button {
    Rectangle rect{};
    const char* label = "";
    command_type command = command_type::none;
    delete_target pending_delete = delete_target::none;
    profile_button_tone tone = profile_button_tone::accent;
    bool requires_avatar_for_enabled_draw = false;
};

struct prompt_action_button {
    Rectangle rect{};
    const char* label = "";
    command_type command = command_type::none;
    profile_button_tone tone = profile_button_tone::accent;
};

struct upload_row_action_button {
    Rectangle rect{};
    const char* label = "";
    command_type command = command_type::none;
    profile_button_tone tone = profile_button_tone::accent;
};

struct item_delete_prompt_layout {
    Rectangle panel;
    Rectangle message;
    Rectangle confirm_button;
    Rectangle cancel_button;
};

struct account_delete_prompt_layout {
    Rectangle panel;
    Rectangle title;
    Rectangle description;
    Rectangle password;
    Rectangle confirm_button;
    Rectangle cancel_button;
};

struct profile_layout {
    Rectangle dialog_rect{};
    Rectangle content_rect{};
    Rectangle close_rect{};
    Rectangle avatar_rect{};
    Rectangle display_name_rect{};
    Rectangle email_rect{};
    Rectangle verification_rect{};
    Rectangle external_links_rect{};
    Rectangle songs_count_rect{};
    Rectangle charts_count_rect{};
    Rectangle tab_bar_rect{};
};

profile_layout make_profile_layout() {
    const Rectangle dialog = kDialogRect;
    const Rectangle avatar = ui::place(dialog, kHeaderAvatarSize, kHeaderAvatarSize,
                                       ui::anchor::top_left, ui::anchor::top_left,
                                       kHeaderAvatarOffset);
    const float info_x = avatar.x + avatar.width + kHeaderInfoGap;
    return {
        .dialog_rect = dialog,
        .content_rect = {
            dialog.x + kContentOuterPadding,
            dialog.y + kHeaderHeight + kTabHeight + kContentTopGap,
            dialog.width - kContentOuterPadding * 2.0f,
            dialog.height - kHeaderHeight - kTabHeight - kContentTopGap - kContentOuterPadding,
        },
        .close_rect = {
            dialog.x + dialog.width - 132.0f,
            dialog.y + 24.0f,
            92.0f,
            42.0f,
        },
        .avatar_rect = avatar,
        .display_name_rect = {info_x, avatar.y + 4.0f, kHeaderInfoWidth, 38.0f},
        .email_rect = {info_x + 1.0f, avatar.y + 47.0f, kHeaderInfoWidth, 24.0f},
        .verification_rect = {info_x + 1.0f, avatar.y + 76.0f, 260.0f, 24.0f},
        .external_links_rect = {info_x + 1.0f, avatar.y + 106.0f, kHeaderInfoWidth, 22.0f},
        .songs_count_rect = {dialog.x + kHeaderCountXOffset, avatar.y + 22.0f, 180.0f, 28.0f},
        .charts_count_rect = {dialog.x + kHeaderCountXOffset, avatar.y + 58.0f, 180.0f, 28.0f},
        .tab_bar_rect = {
            dialog.x + kTabBarInsetX,
            dialog.y + kHeaderHeight,
            dialog.width - kTabBarInsetX * 2.0f,
            kTabHeight,
        },
    };
}

Rectangle content_rect() {
    return make_profile_layout().content_rect;
}

Rectangle close_rect() {
    return make_profile_layout().close_rect;
}

Rectangle dialog_rect() {
    return make_profile_layout().dialog_rect;
}

item_delete_prompt_layout item_delete_prompt_layout_for() {
    return {
        .panel = {
            kDialogRect.x + kItemDeletePanelOffset.x,
            kDialogRect.y + kDialogRect.height + kItemDeletePanelOffset.y,
            kItemDeletePanelWidth,
            kItemDeletePanelHeight,
        },
        .message = {
            kDialogRect.x + kItemDeletePanelOffset.x + 18.0f,
            kDialogRect.y + kDialogRect.height + kItemDeletePanelOffset.y + 14.0f,
            kItemDeletePanelWidth - 36.0f,
            22.0f,
        },
        .confirm_button = {
            kDialogRect.x + kDialogRect.width - 334.0f,
            kDialogRect.y + kDialogRect.height - 66.0f,
            138.0f,
            42.0f,
        },
        .cancel_button = {
            kDialogRect.x + kDialogRect.width - 184.0f,
            kDialogRect.y + kDialogRect.height - 66.0f,
            132.0f,
            42.0f,
        },
    };
}

Rectangle status_rect(float width = 260.0f) {
    return {
        kDialogRect.x + kStatusLeftInset,
        kDialogRect.y + kDialogRect.height - kStatusBottomOffset,
        width,
        24.0f,
    };
}

Rectangle tab_rect(std::size_t index) {
    constexpr float width = 150.0f;
    constexpr float gap = 8.0f;
    std::array<Rectangle, kProfileTabs.size()> tabs{};
    ui::hstack({kDialogRect.x + 42.0f, kDialogRect.y + kHeaderHeight, kDialogRect.width, kTabHeight},
               width,
               gap,
               tabs);
    return tabs[index];
}

float list_content_height(int item_count) {
    return ui::vertical_list_content_height(item_count, kRowHeight, kRowGap);
}

Rectangle row_rect(Rectangle list_rect, int visible_index, float scroll_y) {
    return ui::vertical_list_row_rect(list_rect, visible_index, kRowHeight, kRowGap, scroll_y);
}

upload_row_layout upload_row_layout_for(Rectangle row) {
    const Rectangle delete_button = ui::row_action_rect(row, 0, kUploadRowActionLayout);
    const float text_x = row.x + kUploadTextLeftInset;
    const float text_width = delete_button.x - text_x - kUploadTextActionGap;
    return {
        .title = {text_x, row.y + 9.0f, text_width, 24.0f},
        .subtitle = {text_x, row.y + 38.0f, text_width, 18.0f},
        .delete_button = delete_button,
    };
}

Rectangle upload_row_action_rect(Rectangle row) {
    return upload_row_layout_for(row).delete_button;
}

upload_row_action_button upload_row_delete_action_for(Rectangle row) {
    return {
        .rect = upload_row_action_rect(row),
        .label = "DELETE",
        .command = command_type::begin_delete,
        .tone = profile_button_tone::error,
    };
}

activity_row_layout activity_row_layout_for(Rectangle row) {
    return {
        .title = {row.x + 18.0f, row.y + 9.0f, 520.0f, 24.0f},
        .subtitle = {row.x + 18.0f, row.y + 38.0f, 520.0f, 18.0f},
        .local_summary = {row.x + 650.0f, row.y + 10.0f, 310.0f, 22.0f},
        .online_summary = {row.x + 650.0f, row.y + 38.0f, 360.0f, 22.0f},
    };
}

best_rc_row_layout best_rc_row_layout_for(Rectangle row) {
    return {
        .rank = {row.x + 18.0f, row.y + 18.0f, 54.0f, 30.0f},
        .title = {row.x + 78.0f, row.y + 9.0f, 520.0f, 24.0f},
        .subtitle = {row.x + 78.0f, row.y + 38.0f, 520.0f, 18.0f},
        .summary = {row.x + 650.0f, row.y + 12.0f, 430.0f, 22.0f},
        .online_summary = {row.x + 650.0f, row.y + 38.0f, 360.0f, 20.0f},
    };
}

bool is_title_header_chrome(Rectangle rect) {
    return ui::contains_point(rect, virtual_screen::get_virtual_mouse());
}

bool is_background_close_exclusion() {
    return is_title_header_chrome(title_layout::account_chip_rect()) ||
           is_title_header_chrome(title_layout::settings_chip_rect()) ||
           is_title_header_chrome(title_layout::refresh_chip_rect()) ||
           is_title_header_chrome(title_layout::rating_rankings_chip_rect());
}

bool should_close_from_background_click(Vector2 mouse) {
    if (is_background_close_exclusion()) {
        return false;
    }
    return ui::is_mouse_button_pressed_outside(dialog_rect(), mouse);
}

overview_layout overview_layout_for(Rectangle content) {
    overview_layout layout{};
    ui::hstack_fill({content.x, content.y, content.width, kOverviewMetricHeight},
                    kOverviewMetricGap,
                    layout.metric_cards);
    layout.recent_activity = {
        content.x,
        content.y + kOverviewSummaryTop,
        content.width,
        kOverviewSummaryHeight,
    };
    layout.top_rc = {
        content.x,
        layout.recent_activity.y + layout.recent_activity.height + kOverviewSummaryGap,
        content.width,
        kOverviewSummaryHeight,
    };
    return layout;
}

summary_card_layout summary_card_layout_for(Rectangle card) {
    return {
        .heading = {card.x + kSummaryLeftInset, card.y + 16.0f,
                    card.width - kSummaryLeftInset * 2.0f, 26.0f},
        .empty_message = {card.x + kSummaryLeftInset, card.y + 54.0f,
                          card.width - kSummaryLeftInset * 2.0f, 22.0f},
        .title = {card.x + kSummaryLeftInset, card.y + 48.0f, kSummaryPrimaryWidth, 24.0f},
        .subtitle = {card.x + kSummaryLeftInset, card.y + 76.0f, kSummaryPrimaryWidth, 18.0f},
        .local_summary = {card.x + kSummaryValueX, card.y + 50.0f, 310.0f, 22.0f},
        .online_summary = {card.x + kSummaryValueX, card.y + 76.0f, 360.0f, 22.0f},
        .rc_summary = {card.x + kSummaryValueX, card.y + 62.0f, 360.0f, 22.0f},
    };
}

settings_layout settings_layout_for(Rectangle content) {
    return {
        .title = {
            content.x + kSettingsInsetX,
            content.y + kSettingsTitleY,
            content.width - kSettingsInsetX * 2.0f,
            kSettingsTitleHeight,
        },
        .link_heading = {
            content.x + kSettingsInsetX,
            content.y + kSettingsLinkHeadingY,
            kSettingsTextWidth,
            kSettingsLinkHeadingHeight,
        },
        .save_links_button = {
            content.x + content.width - 202.0f,
            content.y + 256.0f,
            164.0f,
            42.0f,
        },
        .link_hint = {
            content.x + kSettingsInsetX,
            content.y + kSettingsLinkHintY,
            kSettingsTextWidth,
            20.0f,
        },
        .avatar_divider = {
            content.x + kSettingsInsetX,
            content.y + kSettingsDividerY,
            content.width - kSettingsInsetX * 2.0f,
            1.0f,
        },
        .avatar_label = {
            content.x + kSettingsInsetX,
            content.y + kSettingsAvatarLabelY,
            kSettingsTextWidth,
            kSettingsLabelHeight,
        },
        .change_avatar_button = {
            content.x + kSettingsInsetX,
            content.y + kSettingsAvatarButtonY,
            176.0f,
            kSettingsButtonHeight,
        },
        .remove_avatar_button = {
            content.x + 210.0f,
            content.y + kSettingsAvatarButtonY,
            176.0f,
            kSettingsButtonHeight,
        },
        .account_divider = {
            content.x + kSettingsInsetX,
            content.y + kSettingsAccountDividerY,
            content.width - kSettingsInsetX * 2.0f,
            1.0f,
        },
        .account_label = {
            content.x + kSettingsInsetX,
            content.y + kSettingsAccountLabelY,
            kSettingsTextWidth,
            kSettingsLabelHeight,
        },
        .account_description = {
            content.x + kSettingsInsetX,
            content.y + kSettingsAccountDescriptionY,
            kSettingsTextWidth,
            20.0f,
        },
        .delete_account_button = {
            content.x + kSettingsInsetX,
            content.y + kSettingsAccountButtonY,
            238.0f,
            kSettingsButtonHeight,
        },
    };
}

settings_link_row_layout settings_link_row_layout_for(Rectangle content, int index) {
    constexpr float row_gap = kSettingsLinkRowStep - kSettingsLinkRowHeight;
    const Rectangle list = {
        content.x + kSettingsInsetX,
        content.y + kSettingsLinkRowY,
        content.width - kSettingsInsetX * 2.0f,
        content.height - kSettingsLinkRowY,
    };
    const Rectangle row = ui::vertical_list_row_rect(list, index, kSettingsLinkRowHeight, row_gap, 0.0f);
    const ui::rect_pair fields = ui::split_columns(row, kSettingsLinkLabelWidth, kSettingsLinkGap);
    return {.label = fields.first, .url = fields.second};
}

std::array<settings_action_button, 4> settings_action_buttons_for(Rectangle content) {
    const settings_layout settings = settings_layout_for(content);
    return {{
        {
            .rect = settings.save_links_button,
            .label = "SAVE LINKS",
            .command = command_type::save_external_links,
            .tone = profile_button_tone::accent,
        },
        {
            .rect = settings.change_avatar_button,
            .label = "CHANGE IMAGE",
            .command = command_type::change_avatar,
            .tone = profile_button_tone::accent,
        },
        {
            .rect = settings.remove_avatar_button,
            .label = "REMOVE IMAGE",
            .command = command_type::remove_avatar,
            .tone = profile_button_tone::error,
            .requires_avatar_for_enabled_draw = true,
        },
        {
            .rect = settings.delete_account_button,
            .label = "DELETE ACCOUNT",
            .command = command_type::begin_delete,
            .pending_delete = delete_target::account,
            .tone = profile_button_tone::error,
        },
    }};
}

account_delete_prompt_layout account_delete_prompt_layout_for() {
    const Rectangle panel = ui::place(kDialogRect, kAccountDeletePanelWidth, kAccountDeletePanelHeight,
                                      ui::anchor::center, ui::anchor::center);
    return {
        .panel = panel,
        .title = {panel.x + 32.0f, panel.y + 24.0f, panel.width - 64.0f, 30.0f},
        .description = {panel.x + 32.0f, panel.y + 58.0f, panel.width - 64.0f, 22.0f},
        .password = {panel.x + 32.0f, panel.y + 94.0f, panel.width - 64.0f, 42.0f},
        .confirm_button = {panel.x + panel.width - 308.0f, panel.y + panel.height - 62.0f, 132.0f, 42.0f},
        .cancel_button = {panel.x + panel.width - 164.0f, panel.y + panel.height - 62.0f, 132.0f, 42.0f},
    };
}

std::string song_label(const auth::community_song_upload& song) {
    return song.title.empty() ? song.id : song.title;
}

std::string song_subtitle(const auth::community_song_upload& song) {
    if (!song.genre.empty() && !song.artist.empty()) {
        return song.artist + " / " + song.genre;
    }
    if (!song.genre.empty()) {
        return song.genre;
    }
    return song.artist;
}

std::string append_upload_status(std::string value,
                                 const std::string& review_status,
                                 const std::string& lifecycle_status) {
    const std::string label = content_lifecycle::display_label(review_status, lifecycle_status);
    if (label.empty()) {
        return value;
    }
    return value.empty() ? label : value + " | " + label;
}

std::string ranking_subtitle(const activity_item& item) {
    std::string result = item.artist;
    if (!item.genre.empty()) {
        result += result.empty() ? item.genre : " / " + item.genre;
    }
    if (!item.difficulty_name.empty()) {
        result += result.empty() ? item.difficulty_name : " / " + item.difficulty_name;
    }
    return result;
}

std::string profile_link_label(const auth::external_link& link) {
    return link.label.empty() ? link.url : link.label;
}

std::string chart_label(const auth::community_chart_upload& chart) {
    if (!chart.song_title.empty() && !chart.difficulty_name.empty()) {
        return chart.song_title + " / " + chart.difficulty_name;
    }
    if (!chart.difficulty_name.empty()) {
        return chart.difficulty_name;
    }
    return chart.id;
}

std::string make_avatar_label(const song_select::auth_state& auth_state) {
    const std::string source = auth_state.display_name.empty() ? auth_state.email : auth_state.display_name;
    if (source.empty()) {
        return "A";
    }

    std::string result;
    result.reserve(2);
    for (char ch : source) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "A" : result;
}

void draw_profile_button(Rectangle rect, const char* label, bool enabled, Color tone, ui::draw_layer layer) {
    ui::toned_action_button(rect, label, tone, {
        .layer = layer,
        .font_size = 13,
        .border_width = 1.5f,
        .enabled = enabled,
    });
}

std::array<prompt_action_button, 2> account_delete_prompt_buttons_for(const account_delete_prompt_layout& layout) {
    return {{
        {
            .rect = layout.confirm_button,
            .label = "DELETE",
            .command = command_type::delete_account,
            .tone = profile_button_tone::error,
        },
        {
            .rect = layout.cancel_button,
            .label = "CANCEL",
            .command = command_type::cancel_delete,
            .tone = profile_button_tone::muted,
        },
    }};
}

std::array<prompt_action_button, 2> item_delete_prompt_buttons_for(const item_delete_prompt_layout& layout,
                                                                   delete_target target) {
    const command_type confirm_command = target == delete_target::song
        ? command_type::delete_song
        : command_type::delete_chart;
    return {{
        {
            .rect = layout.confirm_button,
            .label = "DELETE",
            .command = confirm_command,
            .tone = profile_button_tone::error,
        },
        {
            .rect = layout.cancel_button,
            .label = "CANCEL",
            .command = command_type::cancel_delete,
            .tone = profile_button_tone::muted,
        },
    }};
}

Color profile_button_tone_color(profile_button_tone tone) {
    const auto& t = *g_theme;
    switch (tone) {
        case profile_button_tone::accent: return t.accent;
        case profile_button_tone::error: return t.error;
        case profile_button_tone::muted: return t.text_muted;
    }
    return t.accent;
}

void draw_settings_action_buttons(Rectangle content,
                                  bool busy,
                                  bool has_avatar,
                                  ui::draw_layer layer) {
    for (const settings_action_button& button : settings_action_buttons_for(content)) {
        const bool enabled = !busy && (!button.requires_avatar_for_enabled_draw || has_avatar);
        draw_profile_button(button.rect, button.label, enabled, profile_button_tone_color(button.tone), layer);
    }
}

void draw_settings_link_inputs(Rectangle content, state& profile, ui::draw_layer layer) {
    for (int i = 0; i < kEditableExternalLinkCount; ++i) {
        const settings_link_row_layout link_row = settings_link_row_layout_for(content, i);
        ui::text_input(link_row.label,
                       profile.link_label_inputs[static_cast<size_t>(i)],
                       "Label", "X / YouTube / Site", {
                           .layer = layer,
                           .font_size = 13,
                           .max_length = 40,
                           .filter = ui::default_text_input_filter,
                           .label_width = 76.0f,
                       });
        ui::text_input(link_row.url,
                       profile.link_url_inputs[static_cast<size_t>(i)],
                       "URL", "https://example.com/you", {
                           .layer = layer,
                           .font_size = 13,
                           .max_length = 240,
                           .filter = ui::default_text_input_filter,
                           .label_width = 56.0f,
                       });
    }
}

void draw_settings_tab(Rectangle content,
                       state& profile,
                       const song_select::auth_state& auth_state,
                       bool busy,
                       ui::draw_layer layer) {
    const auto& t = *g_theme;
    const settings_layout settings = settings_layout_for(content);
    ui::section(content);
    ui::draw_text_in_rect("Settings", 18, settings.title, t.text, ui::text_align::left);
    ui::draw_text_in_rect("Profile Links", 13, settings.link_heading,
                          t.text_secondary, ui::text_align::left);
    draw_settings_link_inputs(content, profile, layer);
    draw_settings_action_buttons(content, busy, !auth_state.avatar_url.empty(), layer);
    ui::draw_text_in_rect("Public on profile and uploaded content cards.", 12, settings.link_hint,
                          t.text_muted, ui::text_align::left);
    ui::divider(settings.avatar_divider, with_alpha(t.border, 150));
    ui::draw_text_in_rect("Profile Image", 13, settings.avatar_label,
                          t.text_secondary, ui::text_align::left);
    ui::divider(settings.account_divider, with_alpha(t.border, 150));
    ui::draw_text_in_rect("Delete this account from raythm-Server.", 13, settings.account_label,
                          t.text_secondary, ui::text_align::left);
    ui::draw_text_in_rect(localization::tr_literal("This does not delete local songs or charts."),
                          12, settings.account_description,
                          t.text_muted, ui::text_align::left);
}

void draw_empty(Rectangle content, const char* message);
void draw_upload_row(Rectangle row,
                     const std::string& title,
                     const std::string& subtitle,
                     bool delete_enabled,
                     ui::draw_layer layer);
void draw_activity_row(Rectangle row, const activity_item& item, ui::draw_layer layer);
void draw_best_rc_row(Rectangle row, const activity_item& item, int index, ui::draw_layer layer);

template <typename RowCallback>
void draw_visible_rows(Rectangle content, std::size_t item_count, float scroll_y, RowCallback draw_row) {
    ui::scoped_clip_rect clip(content);
    const ui::index_range visible_rows =
        ui::vertical_list_visible_range(item_count, content, kRowHeight, kRowGap, scroll_y);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        draw_row(row_rect(content, i, scroll_y), i);
    }
}

void draw_activity_tab(Rectangle content, const state& profile, ui::draw_layer layer) {
    if (profile.activity.empty()) {
        draw_empty(content, localization::tr_literal("No recent play activity yet."));
        return;
    }
    draw_visible_rows(content, profile.activity.size(), profile.activity_scroll,
                      [&](Rectangle row, int index) {
        draw_activity_row(row, profile.activity[static_cast<size_t>(index)], layer);
    });
}

void draw_best_rc_tab(Rectangle content, const state& profile, ui::draw_layer layer) {
    if (profile.best_rating_records.empty()) {
        draw_empty(content, "No rating best plays yet.");
        return;
    }
    draw_visible_rows(content, profile.best_rating_records.size(), profile.best_rating_scroll,
                      [&](Rectangle row, int index) {
        draw_best_rc_row(row, profile.best_rating_records[static_cast<size_t>(index)], index, layer);
    });
}

void draw_uploaded_songs_tab(Rectangle content, const state& profile, bool busy, ui::draw_layer layer) {
    if (profile.uploads.songs.empty()) {
        draw_empty(content, "No uploaded songs.");
        return;
    }
    draw_visible_rows(content, profile.uploads.songs.size(), profile.song_scroll,
                      [&](Rectangle row, int index) {
        const auto& song = profile.uploads.songs[static_cast<size_t>(index)];
        const std::string subtitle =
            append_upload_status(song_subtitle(song), song.review_status, song.lifecycle_status);
        draw_upload_row(row, song_label(song), subtitle, !busy, layer);
    });
}

void draw_uploaded_charts_tab(Rectangle content, const state& profile, bool busy, ui::draw_layer layer) {
    if (profile.uploads.charts.empty()) {
        draw_empty(content, "No uploaded charts.");
        return;
    }
    draw_visible_rows(content, profile.uploads.charts.size(), profile.chart_scroll,
                      [&](Rectangle row, int index) {
        const auto& chart = profile.uploads.charts[static_cast<size_t>(index)];
        const std::string subtitle =
            append_upload_status(chart.chart_author, chart.review_status, chart.lifecycle_status);
        draw_upload_row(row, chart_label(chart), subtitle, !busy, layer);
    });
}

void draw_prompt_action_buttons(std::span<const prompt_action_button> buttons,
                                bool busy,
                                ui::draw_layer layer) {
    for (const prompt_action_button& button : buttons) {
        draw_profile_button(button.rect, button.label, !busy, profile_button_tone_color(button.tone), layer);
    }
}

std::optional<command> hit_prompt_action_button(std::span<const prompt_action_button> buttons, Vector2 mouse) {
    for (const prompt_action_button& button : buttons) {
        if (!ui::contains_point(button.rect, mouse)) {
            continue;
        }
        command result;
        result.type = button.command;
        return result;
    }
    return std::nullopt;
}

void draw_empty(Rectangle content, const char* message) {
    ui::section(content);
    ui::draw_text_in_rect(message, 15, content, g_theme->text_muted);
}

void draw_metric_card(Rectangle rect, const char* label, const std::string& value, Color tone) {
    const auto& t = *g_theme;
    ui::surface(rect, with_alpha(t.row, 205), with_alpha(lerp_color(t.border, tone, 0.35f), 210), 1.2f);
    ui::draw_text_in_rect(label, 12,
                          {rect.x + 18.0f, rect.y + 14.0f, rect.width - 36.0f, 22.0f},
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value.c_str(), 23,
                          {rect.x + 18.0f, rect.y + 42.0f, rect.width - 36.0f, 34.0f},
                          tone, ui::text_align::left);
}

std::string best_rc_summary(const activity_item& item);

void draw_overview_metric_cards(const overview_layout& overview, const state& profile) {
    const auto& t = *g_theme;
    draw_metric_card(overview.metric_cards[0], "Uploaded Songs",
                     std::to_string(profile.uploads.songs.size()), t.accent);
    draw_metric_card(overview.metric_cards[1], "Uploaded Charts",
                     std::to_string(profile.uploads.charts.size()), t.success);
    draw_metric_card(overview.metric_cards[2], "Recent Plays",
                     std::to_string(profile.activity.size()), t.text);
    draw_metric_card(overview.metric_cards[3], "Best RC Plays",
                     std::to_string(profile.best_rating_records.size()), t.rank_ss);
}

void draw_recent_activity_summary_card(Rectangle rect, const state& profile) {
    const auto& t = *g_theme;
    const summary_card_layout layout = summary_card_layout_for(rect);
    ui::surface(rect, with_alpha(t.row, 185), with_alpha(t.border, 205), 1.2f);
    ui::draw_text_in_rect("Recent Activity", 16, layout.heading,
                          t.text, ui::text_align::left);
    if (profile.activity.empty()) {
        ui::draw_text_in_rect(localization::tr_literal("No recent play activity yet."), 13,
                              layout.empty_message,
                              t.text_muted, ui::text_align::left);
        return;
    }

    const activity_item& item = profile.activity.front();
    ui::draw_text_in_rect(item.song_title.c_str(), 15, layout.title,
                          t.text, ui::text_align::left);
    const std::string subtitle = ranking_subtitle(item);
    ui::draw_text_in_rect(subtitle.c_str(), 11, layout.subtitle,
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(item.local_summary.c_str(), 13, layout.local_summary,
                          t.text_secondary, ui::text_align::left);
    ui::draw_text_in_rect(item.online_summary.c_str(), 13, layout.online_summary,
                          t.accent, ui::text_align::left);
}

void draw_top_rc_summary_card(Rectangle rect, const state& profile) {
    const auto& t = *g_theme;
    const summary_card_layout layout = summary_card_layout_for(rect);
    ui::surface(rect, with_alpha(t.row, 185), with_alpha(t.border, 205), 1.2f);
    ui::draw_text_in_rect("Top RC Contributor", 16, layout.heading,
                          t.text, ui::text_align::left);
    if (profile.best_rating_records.empty()) {
        ui::draw_text_in_rect("No rating best plays yet.", 13,
                              layout.empty_message,
                              t.text_muted, ui::text_align::left);
        return;
    }

    const activity_item& item = profile.best_rating_records.front();
    ui::draw_text_in_rect(item.song_title.c_str(), 15, layout.title,
                          t.text, ui::text_align::left);
    const std::string subtitle = ranking_subtitle(item);
    ui::draw_text_in_rect(subtitle.c_str(), 11, layout.subtitle,
                          t.text_muted, ui::text_align::left);
    const std::string summary = best_rc_summary(item);
    ui::draw_text_in_rect(summary.c_str(), 13, layout.rc_summary,
                          t.rank_ss, ui::text_align::left);
}

void draw_overview_tab(Rectangle content, const state& profile) {
    const overview_layout overview = overview_layout_for(content);
    ui::section(content);
    draw_overview_metric_cards(overview, profile);
    draw_recent_activity_summary_card(overview.recent_activity, profile);
    draw_top_rc_summary_card(overview.top_rc, profile);
}

void draw_row_shell(Rectangle row, ui::draw_layer layer) {
    const auto& t = *g_theme;
    ui::hover_surface(row, {
        .layer = layer,
        .border_width = 1.2f,
        .fill = with_alpha(t.row, 195),
        .fill_hover = with_alpha(t.row_hover, 225),
        .border_color = with_alpha(t.border, 205),
        .custom_colors = true,
    });
}

void draw_upload_row(Rectangle row,
                     const std::string& title,
                     const std::string& subtitle,
                     bool delete_enabled,
                     ui::draw_layer layer) {
    const auto& t = *g_theme;
    const upload_row_layout layout = upload_row_layout_for(row);
    const upload_row_action_button delete_action = upload_row_delete_action_for(row);
    draw_row_shell(row, layer);
    ui::draw_text_in_rect(title.c_str(), 15, layout.title, t.text, ui::text_align::left);
    ui::draw_text_in_rect(subtitle.c_str(), 11, layout.subtitle, t.text_muted, ui::text_align::left);
    draw_profile_button(delete_action.rect,
                        delete_action.label,
                        delete_enabled,
                        profile_button_tone_color(delete_action.tone),
                        layer);
}

void draw_activity_row(Rectangle row, const activity_item& item, ui::draw_layer layer) {
    const auto& t = *g_theme;
    const activity_row_layout layout = activity_row_layout_for(row);
    draw_row_shell(row, layer);
    ui::draw_text_in_rect(item.song_title.c_str(), 15, layout.title, t.text, ui::text_align::left);
    const std::string subtitle = ranking_subtitle(item);
    ui::draw_text_in_rect(subtitle.c_str(), 11, layout.subtitle, t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(item.local_summary.c_str(), 13, layout.local_summary,
                          t.text_secondary, ui::text_align::left);
    ui::draw_text_in_rect(item.online_summary.c_str(), 13, layout.online_summary,
                          t.accent, ui::text_align::left);
}

std::string best_rc_summary(const activity_item& item) {
    return TextFormat("RC %.2f  |  +%.2f  |  %.1f%%",
                      item.play_rating,
                      item.rating_contribution,
                      item.rating_contribution_percent);
}

void draw_best_rc_row(Rectangle row, const activity_item& item, int index, ui::draw_layer layer) {
    const auto& t = *g_theme;
    const best_rc_row_layout layout = best_rc_row_layout_for(row);
    draw_row_shell(row, layer);
    ui::draw_text_in_rect(TextFormat("#%d", index + 1),
                          16,
                          layout.rank,
                          t.accent,
                          ui::text_align::left);
    ui::draw_text_in_rect(item.song_title.c_str(), 15,
                          layout.title,
                          t.text,
                          ui::text_align::left);
    const std::string subtitle = ranking_subtitle(item);
    ui::draw_text_in_rect(subtitle.c_str(), 11,
                          layout.subtitle,
                          t.text_muted,
                          ui::text_align::left);
    const std::string summary = best_rc_summary(item);
    ui::draw_text_in_rect(summary.c_str(), 13,
                          layout.summary,
                          t.rank_ss,
                          ui::text_align::left);
    ui::draw_text_in_rect(item.online_summary.c_str(), 12,
                          layout.online_summary,
                          t.text_secondary,
                          ui::text_align::left);
}

const profile_tab_definition& profile_tab_for_index(std::size_t index) {
    return index < kProfileTabs.size() ? kProfileTabs[index] : kProfileTabs.back();
}

bool is_upload_tab(tab selected_tab) {
    return selected_tab == tab::songs || selected_tab == tab::charts;
}

std::string profile_external_links_label(const song_select::auth_state& auth_state) {
    std::string links_label;
    const size_t count = std::min<size_t>(auth_state.external_links.size(), 3);
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            links_label += " / ";
        }
        links_label += profile_link_label(auth_state.external_links[i]);
    }
    return links_label;
}

void draw_profile_header(const profile_layout& layout,
                         const state& profile,
                         const song_select::auth_state& auth_state,
                         bool busy,
                         ui::draw_layer layer) {
    const auto& t = *g_theme;
    avatar_texture_cache::draw_avatar(
        layout.avatar_rect,
        auth_state.avatar_url,
        make_avatar_label(auth_state),
        with_alpha(t.accent, 210),
        t.bg,
        28,
        auth_state.server_url);
    ui::frame(layout.avatar_rect, with_alpha(t.border_active, 230), 2.0f);

    const std::string display_name =
        auth_state.display_name.empty() ? auth_state.email : auth_state.display_name;
    ui::draw_text_in_rect(display_name.c_str(), 27, layout.display_name_rect,
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect(auth_state.email.c_str(), 14, layout.email_rect,
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(auth_state.email_verified ? "Verified profile" : "Email verification pending",
                          13,
                          layout.verification_rect,
                          auth_state.email_verified ? t.success : t.error, ui::text_align::left);
    if (!auth_state.external_links.empty()) {
        const std::string links_label = profile_external_links_label(auth_state);
        ui::draw_text_in_rect(links_label.c_str(), 12, layout.external_links_rect,
                              t.accent, ui::text_align::left);
    }

    const std::string songs_count = std::to_string(profile.uploads.songs.size()) + " songs";
    const std::string charts_count = std::to_string(profile.uploads.charts.size()) + " charts";
    ui::draw_text_in_rect(songs_count.c_str(), 15, layout.songs_count_rect,
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect(charts_count.c_str(), 15, layout.charts_count_rect,
                          t.text, ui::text_align::left);

    draw_profile_button(close_rect(), "CLOSE", !busy, t.text_muted, layer);
}

void draw_profile_tabs(const profile_layout& layout, tab selected_tab, ui::draw_layer layer) {
    const auto& t = *g_theme;
    ui::bar_surface(layout.tab_bar_rect, with_alpha(t.bg, 70), with_alpha(t.border, 180));
    for (std::size_t i = 0; i < kProfileTabs.size(); ++i) {
        const profile_tab_definition& current_tab = profile_tab_for_index(i);
        const bool selected = current_tab.value == selected_tab;
        const Rectangle rect = tab_rect(i);
        ui::tab_button(rect, current_tab.label, {
            .layer = layer,
            .font_size = 14,
            .selected = selected,
            .style = ui::tab_button_style::underline,
            .bg_hover = with_alpha(t.row_hover, 90),
            .bg_selected = with_alpha(t.row_selected, 135),
            .text_color = t.text_secondary,
            .selected_text_color = t.text,
            .underline_color = with_alpha(t.accent, 235),
            .custom_colors = true,
        });
    }
}

std::vector<auth::external_link> collect_settings_links(const state& profile) {
    std::vector<auth::external_link> links;
    for (int i = 0; i < kEditableExternalLinkCount; ++i) {
        const std::string& url = profile.link_url_inputs[static_cast<size_t>(i)].value;
        if (url.empty()) {
            continue;
        }
        links.push_back({
            .label = profile.link_label_inputs[static_cast<size_t>(i)].value,
            .url = url,
        });
    }
    return links;
}

}  // namespace

Rectangle bounds() {
    return dialog_rect();
}

void open(state& profile) {
    if (!profile.open) {
        profile.open_anim = 0.0f;
    }
    profile.open = true;
    profile.closing = false;
    profile.suppress_background_close_until_release = true;
    profile.pending_delete = delete_target::none;
    profile.pending_id.clear();
    profile.pending_label.clear();
    profile.delete_password_input.active = false;
    profile.delete_password_input.has_selection = false;
    profile.delete_password_input.mouse_selecting = false;
    profile.settings_links_initialized = false;
}

void close(state& profile) {
    if (profile.open) {
        profile.closing = true;
    } else {
        profile.open_anim = 0.0f;
    }
    profile.pending_delete = delete_target::none;
    profile.pending_id.clear();
    profile.pending_label.clear();
    profile.delete_password_input.active = false;
    profile.delete_password_input.has_selection = false;
    profile.delete_password_input.mouse_selecting = false;
}

scroll_offsets clamped_scroll_offsets(const state& profile) {
    const Rectangle content = content_rect();
    return {
        .activity = ui::scroll_offset_state_for(
            content,
            list_content_height(static_cast<int>(profile.activity.size())),
            profile.activity_scroll).offset,
        .best_rating = ui::scroll_offset_state_for(
            content,
            list_content_height(static_cast<int>(profile.best_rating_records.size())),
            profile.best_rating_scroll).offset,
        .songs = ui::scroll_offset_state_for(
            content,
            list_content_height(static_cast<int>(profile.uploads.songs.size())),
            profile.song_scroll).offset,
        .charts = ui::scroll_offset_state_for(
            content,
            list_content_height(static_cast<int>(profile.uploads.charts.size())),
            profile.chart_scroll).offset,
    };
}

input_result update(const state& profile, bool request_active) {
    input_result result;
    auto action = [&result](command command_value) {
        input_result with_action = result;
        with_action.action = std::move(command_value);
        return with_action;
    };
    if (!profile.open) {
        return result;
    }
    if (profile.closing) {
        return result;
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = ui::is_mouse_button_pressed();
    const float wheel = ui::mouse_wheel_move();
    const bool busy = profile.loading || profile.deleting || profile.saving_links || profile.saving_avatar || request_active;
    const Rectangle content = content_rect();
    scroll_offsets scroll = clamped_scroll_offsets(profile);

    if (profile.suppress_background_close_until_release) {
        if (ui::is_mouse_button_down_or_released()) {
            return result;
        }
        result.release_background_close_suppression = true;
    }

    if (!busy && profile.pending_delete != delete_target::none && ui::is_cancel_pressed()) {
        return action({.type = command_type::cancel_delete});
    }

    if (!busy && ui::is_cancel_pressed()) {
        return action({.type = command_type::close});
    }

    if (!busy) {
        if (profile.selected_tab == tab::activity) {
            scroll.activity = ui::wheel_scrolled_offset_state(
                content,
                mouse,
                wheel,
                list_content_height(static_cast<int>(profile.activity.size())),
                scroll.activity,
                kWheelStep).offset;
        } else if (profile.selected_tab == tab::best_rc) {
            scroll.best_rating = ui::wheel_scrolled_offset_state(
                content,
                mouse,
                wheel,
                list_content_height(static_cast<int>(profile.best_rating_records.size())),
                scroll.best_rating,
                kWheelStep).offset;
        } else if (profile.selected_tab == tab::songs) {
            scroll.songs = ui::wheel_scrolled_offset_state(
                content,
                mouse,
                wheel,
                list_content_height(static_cast<int>(profile.uploads.songs.size())),
                scroll.songs,
                kWheelStep).offset;
        } else if (profile.selected_tab == tab::charts) {
            scroll.charts = ui::wheel_scrolled_offset_state(
                content,
                mouse,
                wheel,
                list_content_height(static_cast<int>(profile.uploads.charts.size())),
                scroll.charts,
                kWheelStep).offset;
        }
    }
    if (scroll.activity != profile.activity_scroll ||
        scroll.best_rating != profile.best_rating_scroll ||
        scroll.songs != profile.song_scroll ||
        scroll.charts != profile.chart_scroll) {
        result.scroll_changed = true;
        result.scroll = scroll;
    }

    if (!left_pressed) {
        return result;
    }

    if (!busy && should_close_from_background_click(mouse)) {
        result.action = {.type = command_type::close};
        return result;
    }

    if (profile.pending_delete != delete_target::none) {
        if (profile.pending_delete == delete_target::account) {
            const account_delete_prompt_layout delete_layout = account_delete_prompt_layout_for();
            const std::array<prompt_action_button, 2> buttons = account_delete_prompt_buttons_for(delete_layout);
            if (!busy) {
                if (std::optional<command> hit = hit_prompt_action_button(buttons, mouse)) {
                    if (hit->type == command_type::delete_account) {
                        hit->password = profile.delete_password_input.value;
                    }
                    return action(std::move(*hit));
                }
            }
            return result;
        }
        const item_delete_prompt_layout delete_layout = item_delete_prompt_layout_for();
        const std::array<prompt_action_button, 2> buttons =
            item_delete_prompt_buttons_for(delete_layout, profile.pending_delete);
        if (!busy) {
            if (std::optional<command> hit = hit_prompt_action_button(buttons, mouse)) {
                if (hit->type == command_type::delete_song || hit->type == command_type::delete_chart) {
                    hit->id = profile.pending_id;
                }
                return action(std::move(*hit));
            }
        }
        return result;
    }

    if (!busy && ui::contains_point(close_rect(), mouse)) {
        return action({.type = command_type::close});
    }

    for (std::size_t i = 0; i < kProfileTabs.size(); ++i) {
        if (ui::contains_point(tab_rect(i), mouse)) {
            return action({.type = command_type::select_tab, .selected_tab = profile_tab_for_index(i).value});
        }
    }

    if (!busy && profile.uploads.success && profile.selected_tab == tab::songs) {
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            profile.uploads.songs.size(), content, kRowHeight, kRowGap, scroll.songs);
        for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
            const Rectangle row = row_rect(content, i, scroll.songs);
            const upload_row_action_button delete_action = upload_row_delete_action_for(row);
            if (ui::contains_point(delete_action.rect, mouse)) {
                const auto& song = profile.uploads.songs[static_cast<size_t>(i)];
                return action({
                    .type = delete_action.command,
                    .id = song.id,
                    .pending_delete = delete_target::song,
                    .delete_label = song_label(song),
                });
            }
        }
    }

    if (!busy && profile.uploads.success && profile.selected_tab == tab::charts) {
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            profile.uploads.charts.size(), content, kRowHeight, kRowGap, scroll.charts);
        for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
            const Rectangle row = row_rect(content, i, scroll.charts);
            const upload_row_action_button delete_action = upload_row_delete_action_for(row);
            if (ui::contains_point(delete_action.rect, mouse)) {
                const auto& chart = profile.uploads.charts[static_cast<size_t>(i)];
                return action({
                    .type = delete_action.command,
                    .id = chart.id,
                    .pending_delete = delete_target::chart,
                    .delete_label = chart_label(chart),
                });
            }
        }
    }

    if (!busy && profile.selected_tab == tab::settings) {
        for (const settings_action_button& button : settings_action_buttons_for(content)) {
            if (!ui::contains_point(button.rect, mouse)) {
                continue;
            }
            command command_result;
            command_result.type = button.command;
            command_result.pending_delete = button.pending_delete;
            if (button.command == command_type::save_external_links) {
                command_result.external_links = collect_settings_links(profile);
            }
            return action(std::move(command_result));
        }
    }

    return result;
}

void draw(state& profile,
          const song_select::auth_state& auth_state,
          square_image_picker::state& avatar_picker,
          bool request_active,
          ui::draw_layer layer,
          bool draw_backdrop) {
    if (!profile.open) {
        return;
    }

    ui::enqueue_draw_command(layer, [&profile, &avatar_picker, auth_state, request_active, layer, draw_backdrop]() {
        const auto& t = *g_theme;
        const bool busy = profile.loading || profile.deleting || profile.saving_links ||
            profile.saving_avatar || request_active || profile.closing;
        const float anim_t = tween::ease_out_cubic(std::clamp(profile.open_anim, 0.0f, 1.0f));
        const float scale = 1.0f - (1.0f - anim_t) * kOpenAnimScaleInset;
        const float offset_y = (1.0f - anim_t) * kOpenAnimOffsetY;
        const Vector2 center = {
            kDialogRect.x + kDialogRect.width * 0.5f,
            kDialogRect.y + kDialogRect.height * 0.5f,
        };

        if (draw_backdrop) {
            ui::backdrop({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                         with_alpha(BLACK, static_cast<unsigned char>(160.0f * anim_t)));
        }
        rlPushMatrix();
        rlTranslatef(center.x, center.y + offset_y, 0.0f);
        rlScalef(scale, scale, 1.0f);
        rlTranslatef(-center.x, -center.y, 0.0f);
        const profile_layout layout = make_profile_layout();
        ui::panel(layout.dialog_rect);
        draw_profile_header(layout, profile, auth_state, busy, layer);
        draw_profile_tabs(layout, profile.selected_tab, layer);

        const Rectangle content = content_rect();
        if (profile.loading && !profile.loaded_once) {
            draw_empty(content, localization::tr_literal("Loading profile..."));
        } else if (profile.selected_tab == tab::overview) {
            draw_overview_tab(content, profile);
        } else if (profile.selected_tab == tab::activity) {
            draw_activity_tab(content, profile, layer);
        } else if (profile.selected_tab == tab::best_rc) {
            draw_best_rc_tab(content, profile, layer);
        } else if (is_upload_tab(profile.selected_tab) && !profile.uploads.success) {
            draw_empty(content, profile.uploads.message.empty()
                                    ? "Uploaded content could not be loaded."
                                    : profile.uploads.message.c_str());
        } else if (profile.selected_tab == tab::songs) {
            draw_uploaded_songs_tab(content, profile, busy, layer);
        } else if (profile.selected_tab == tab::charts) {
            draw_uploaded_charts_tab(content, profile, busy, layer);
        } else {
            draw_settings_tab(content, profile, auth_state, busy, layer);
        }

        if (profile.loading && profile.loaded_once) {
            ui::draw_text_in_rect(localization::tr_literal("Refreshing..."), 13,
                                  status_rect(),
                                  t.text_muted, ui::text_align::left);
        }
        if (profile.deleting) {
            ui::draw_text_in_rect(localization::tr_literal("Deleting..."), 13,
                                  status_rect(),
                                  t.text_muted, ui::text_align::left);
        }
        if (profile.saving_links) {
            ui::draw_text_in_rect(localization::tr_literal("Saving links..."), 13,
                                  status_rect(),
                                  t.text_muted, ui::text_align::left);
        }
        if (profile.saving_avatar) {
            ui::draw_text_in_rect(localization::tr_literal("Saving profile image..."), 13,
                                  status_rect(320.0f),
                                  t.text_muted, ui::text_align::left);
        }

        if (profile.pending_delete == delete_target::account) {
            const account_delete_prompt_layout delete_layout = account_delete_prompt_layout_for();
            ui::surface(delete_layout.panel, with_alpha(t.panel, 248), with_alpha(t.error, 220), 2.0f);
            ui::draw_text_in_rect("Delete Account", 20, delete_layout.title, t.text, ui::text_align::left);
            ui::draw_text_in_rect("Enter your password to permanently delete this server account.",
                                  12, delete_layout.description,
                                  t.text_secondary, ui::text_align::left);
            ui::text_input(delete_layout.password, profile.delete_password_input,
                           "Pass", "Delete account password", {
                               .layer = layer,
                               .font_size = 13,
                               .max_length = 64,
                               .filter = ui::default_text_input_filter,
                               .label_width = 92.0f,
                               .obscure_value = true,
                           });
            const std::array<prompt_action_button, 2> buttons = account_delete_prompt_buttons_for(delete_layout);
            draw_prompt_action_buttons(buttons, busy, layer);
        } else if (profile.pending_delete != delete_target::none) {
            const item_delete_prompt_layout delete_layout = item_delete_prompt_layout_for();
            ui::surface(delete_layout.panel, with_alpha(t.panel, 245), with_alpha(t.error, 210), 2.0f);
            const std::string message = "Delete " + profile.pending_label + "? Hidden from Community listings.";
            ui::draw_text_in_rect(message.c_str(), 12, delete_layout.message, t.text, ui::text_align::left);
            const std::array<prompt_action_button, 2> buttons =
                item_delete_prompt_buttons_for(delete_layout, profile.pending_delete);
            draw_prompt_action_buttons(buttons, busy, layer);
        }
        rlPopMatrix();
        if (avatar_picker.is_open()) {
            avatar_picker.draw();
        }
    });
}

}  // namespace title_profile_view
