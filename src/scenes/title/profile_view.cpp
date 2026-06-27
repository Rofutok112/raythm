#include "title/profile_view.h"

#include <algorithm>
#include <cctype>
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
#include "ui_layout.h"
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
constexpr int kTabCount = 6;
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

Rectangle tab_rect(int index) {
    constexpr float width = 150.0f;
    constexpr float gap = 8.0f;
    return {
        kDialogRect.x + 42.0f + static_cast<float>(index) * (width + gap),
        kDialogRect.y + kHeaderHeight,
        width,
        kTabHeight,
    };
}

float max_scroll(Rectangle list_rect, int item_count) {
    const float content_height = static_cast<float>(item_count) * (kRowHeight + kRowGap) - kRowGap;
    return std::max(0.0f, content_height - list_rect.height);
}

Rectangle row_rect(Rectangle list_rect, int visible_index, float scroll_y) {
    const float y = list_rect.y + static_cast<float>(visible_index) * (kRowHeight + kRowGap) - scroll_y;
    return {list_rect.x, y, list_rect.width, kRowHeight};
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

overview_layout overview_layout_for(Rectangle content) {
    overview_layout layout{};
    const float metric_width = (content.width - kOverviewMetricGap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        layout.metric_cards[i] = {
            content.x + static_cast<float>(i) * (metric_width + kOverviewMetricGap),
            content.y,
            metric_width,
            kOverviewMetricHeight,
        };
    }
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
    const Rectangle row = {
        content.x + kSettingsInsetX,
        content.y + kSettingsLinkRowY + static_cast<float>(index) * kSettingsLinkRowStep,
        content.width - kSettingsInsetX * 2.0f,
        kSettingsLinkRowHeight,
    };
    const ui::rect_pair fields = ui::split_columns(row, kSettingsLinkLabelWidth, kSettingsLinkGap);
    return {.label = fields.first, .url = fields.second};
}

Rectangle settings_delete_account_rect(Rectangle content) {
    return settings_layout_for(content).delete_account_button;
}

Rectangle settings_save_links_rect(Rectangle content) {
    return settings_layout_for(content).save_links_button;
}

Rectangle settings_change_avatar_rect(Rectangle content) {
    return settings_layout_for(content).change_avatar_button;
}

Rectangle settings_remove_avatar_rect(Rectangle content) {
    return settings_layout_for(content).remove_avatar_button;
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
    draw_row_shell(row, layer);
    ui::draw_text_in_rect(title.c_str(), 15, layout.title, t.text, ui::text_align::left);
    ui::draw_text_in_rect(subtitle.c_str(), 11, layout.subtitle, t.text_muted, ui::text_align::left);
    draw_profile_button(layout.delete_button, "DELETE", delete_enabled, t.error, layer);
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

tab tab_for_index(int index) {
    switch (index) {
    case 0:
        return tab::overview;
    case 1:
        return tab::activity;
    case 2:
        return tab::best_rc;
    case 3:
        return tab::songs;
    case 4:
        return tab::charts;
    default:
        return tab::settings;
    }
}

bool is_upload_tab(tab selected_tab) {
    return selected_tab == tab::songs || selected_tab == tab::charts;
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
        .activity = std::clamp(profile.activity_scroll,
                               0.0f,
                               max_scroll(content, static_cast<int>(profile.activity.size()))),
        .best_rating = std::clamp(profile.best_rating_scroll,
                                  0.0f,
                                  max_scroll(content, static_cast<int>(profile.best_rating_records.size()))),
        .songs = std::clamp(profile.song_scroll,
                            0.0f,
                            max_scroll(content, static_cast<int>(profile.uploads.songs.size()))),
        .charts = std::clamp(profile.chart_scroll,
                             0.0f,
                             max_scroll(content, static_cast<int>(profile.uploads.charts.size()))),
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
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();
    const bool busy = profile.loading || profile.deleting || profile.saving_links || profile.saving_avatar || request_active;
    const Rectangle content = content_rect();
    scroll_offsets scroll = clamped_scroll_offsets(profile);

    if (profile.suppress_background_close_until_release) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            return result;
        }
        result.release_background_close_suppression = true;
    }

    if (!busy && profile.pending_delete != delete_target::none &&
        (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        return action({.type = command_type::cancel_delete});
    }

    if (!busy && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        return action({.type = command_type::close});
    }

    if (!busy && wheel != 0.0f && ui::contains_point(content, mouse)) {
        if (profile.selected_tab == tab::activity) {
            scroll.activity = std::clamp(
                scroll.activity - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.activity.size())));
        } else if (profile.selected_tab == tab::best_rc) {
            scroll.best_rating = std::clamp(
                scroll.best_rating - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.best_rating_records.size())));
        } else if (profile.selected_tab == tab::songs) {
            scroll.songs = std::clamp(
                scroll.songs - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.uploads.songs.size())));
        } else if (profile.selected_tab == tab::charts) {
            scroll.charts = std::clamp(
                scroll.charts - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.uploads.charts.size())));
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

    if (!busy && !ui::contains_point(dialog_rect(), mouse) && !is_background_close_exclusion()) {
        result.action = {.type = command_type::close};
        return result;
    }

    if (profile.pending_delete != delete_target::none) {
        if (profile.pending_delete == delete_target::account) {
            const account_delete_prompt_layout delete_layout = account_delete_prompt_layout_for();
            if (!busy && ui::contains_point(delete_layout.confirm_button, mouse)) {
                return action({.type = command_type::delete_account, .password = profile.delete_password_input.value});
            }
            if (!busy && ui::contains_point(delete_layout.cancel_button, mouse)) {
                return action({.type = command_type::cancel_delete});
            }
            return result;
        }
        const item_delete_prompt_layout delete_layout = item_delete_prompt_layout_for();
        if (!busy && ui::contains_point(delete_layout.confirm_button, mouse)) {
            const command_type type =
                profile.pending_delete == delete_target::song ? command_type::delete_song : command_type::delete_chart;
            return action({.type = type, .id = profile.pending_id});
        }
        if (!busy && ui::contains_point(delete_layout.cancel_button, mouse)) {
            return action({.type = command_type::cancel_delete});
        }
        return result;
    }

    if (!busy && ui::contains_point(close_rect(), mouse)) {
        return action({.type = command_type::close});
    }

    for (int i = 0; i < kTabCount; ++i) {
        if (ui::contains_point(tab_rect(i), mouse)) {
            return action({.type = command_type::select_tab, .selected_tab = tab_for_index(i)});
        }
    }

    if (!busy && profile.uploads.success && profile.selected_tab == tab::songs) {
        for (int i = 0; i < static_cast<int>(profile.uploads.songs.size()); ++i) {
            const Rectangle row = row_rect(content, i, scroll.songs);
            if (row.y + row.height < content.y || row.y > content.y + content.height) {
                continue;
            }
            if (ui::contains_point(upload_row_action_rect(row), mouse)) {
                const auto& song = profile.uploads.songs[static_cast<size_t>(i)];
                return action({
                    .type = command_type::begin_delete,
                    .id = song.id,
                    .pending_delete = delete_target::song,
                    .delete_label = song_label(song),
                });
            }
        }
    }

    if (!busy && profile.uploads.success && profile.selected_tab == tab::charts) {
        for (int i = 0; i < static_cast<int>(profile.uploads.charts.size()); ++i) {
            const Rectangle row = row_rect(content, i, scroll.charts);
            if (row.y + row.height < content.y || row.y > content.y + content.height) {
                continue;
            }
            if (ui::contains_point(upload_row_action_rect(row), mouse)) {
                const auto& chart = profile.uploads.charts[static_cast<size_t>(i)];
                return action({
                    .type = command_type::begin_delete,
                    .id = chart.id,
                    .pending_delete = delete_target::chart,
                    .delete_label = chart_label(chart),
                });
            }
        }
    }

    if (!busy && profile.selected_tab == tab::settings &&
        ui::contains_point(settings_save_links_rect(content), mouse)) {
        return action({
            .type = command_type::save_external_links,
            .external_links = collect_settings_links(profile),
        });
    }

    if (!busy && profile.selected_tab == tab::settings &&
        ui::contains_point(settings_change_avatar_rect(content), mouse)) {
        return action({.type = command_type::change_avatar});
    }

    if (!busy && profile.selected_tab == tab::settings &&
        ui::contains_point(settings_remove_avatar_rect(content), mouse)) {
        return action({.type = command_type::remove_avatar});
    }

    if (!busy && profile.selected_tab == tab::settings &&
        ui::contains_point(settings_delete_account_rect(content), mouse)) {
        return action({
            .type = command_type::begin_delete,
            .pending_delete = delete_target::account,
        });
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
            std::string links_label;
            const size_t count = std::min<size_t>(auth_state.external_links.size(), 3);
            for (size_t i = 0; i < count; ++i) {
                if (i > 0) {
                    links_label += " / ";
                }
                links_label += profile_link_label(auth_state.external_links[i]);
            }
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

        ui::bar_surface(layout.tab_bar_rect, with_alpha(t.bg, 70), with_alpha(t.border, 180));

        const char* tab_labels[] = {"OVERVIEW", "ACTIVITY", "BEST RC", "SONGS", "CHARTS", "SETTINGS"};
        for (int i = 0; i < kTabCount; ++i) {
            const tab current_tab = tab_for_index(i);
            const bool selected = current_tab == profile.selected_tab;
            const Rectangle rect = tab_rect(i);
            ui::tab_button(rect, tab_labels[i], {
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

        const Rectangle content = content_rect();
        if (profile.loading && !profile.loaded_once) {
            draw_empty(content, localization::tr_literal("Loading profile..."));
        } else if (profile.selected_tab == tab::overview) {
            const overview_layout overview = overview_layout_for(content);
            ui::section(content);
            draw_metric_card(overview.metric_cards[0], "Uploaded Songs",
                             std::to_string(profile.uploads.songs.size()), t.accent);
            draw_metric_card(overview.metric_cards[1], "Uploaded Charts",
                             std::to_string(profile.uploads.charts.size()), t.success);
            draw_metric_card(overview.metric_cards[2], "Recent Plays",
                             std::to_string(profile.activity.size()), t.text);
            draw_metric_card(overview.metric_cards[3], "Best RC Plays",
                             std::to_string(profile.best_rating_records.size()), t.rank_ss);

            const Rectangle recent = overview.recent_activity;
            const summary_card_layout recent_layout = summary_card_layout_for(recent);
            ui::surface(recent, with_alpha(t.row, 185), with_alpha(t.border, 205), 1.2f);
            ui::draw_text_in_rect("Recent Activity", 16, recent_layout.heading,
                                  t.text, ui::text_align::left);
            if (profile.activity.empty()) {
                ui::draw_text_in_rect(localization::tr_literal("No recent play activity yet."), 13,
                                      recent_layout.empty_message,
                                      t.text_muted, ui::text_align::left);
            } else {
                const auto& item = profile.activity.front();
                ui::draw_text_in_rect(item.song_title.c_str(), 15, recent_layout.title,
                                      t.text, ui::text_align::left);
                const std::string subtitle = ranking_subtitle(item);
                ui::draw_text_in_rect(subtitle.c_str(), 11, recent_layout.subtitle,
                                      t.text_muted, ui::text_align::left);
                ui::draw_text_in_rect(item.local_summary.c_str(), 13, recent_layout.local_summary,
                                      t.text_secondary, ui::text_align::left);
                ui::draw_text_in_rect(item.online_summary.c_str(), 13, recent_layout.online_summary,
                                      t.accent, ui::text_align::left);
            }

            const Rectangle first_place = overview.top_rc;
            const summary_card_layout first_place_layout = summary_card_layout_for(first_place);
            ui::surface(first_place, with_alpha(t.row, 185), with_alpha(t.border, 205), 1.2f);
            ui::draw_text_in_rect("Top RC Contributor", 16, first_place_layout.heading,
                                  t.text, ui::text_align::left);
            if (profile.best_rating_records.empty()) {
                ui::draw_text_in_rect("No rating best plays yet.", 13,
                                      first_place_layout.empty_message,
                                      t.text_muted, ui::text_align::left);
            } else {
                const auto& item = profile.best_rating_records.front();
                ui::draw_text_in_rect(item.song_title.c_str(), 15, first_place_layout.title,
                                      t.text, ui::text_align::left);
                const std::string subtitle = ranking_subtitle(item);
                ui::draw_text_in_rect(subtitle.c_str(), 11, first_place_layout.subtitle,
                                      t.text_muted, ui::text_align::left);
                const std::string summary = best_rc_summary(item);
                ui::draw_text_in_rect(summary.c_str(), 13, first_place_layout.rc_summary,
                                      t.rank_ss, ui::text_align::left);
            }
        } else if (profile.selected_tab == tab::activity) {
            if (profile.activity.empty()) {
                draw_empty(content, localization::tr_literal("No recent play activity yet."));
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.activity.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.activity_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    draw_activity_row(row, profile.activity[static_cast<size_t>(i)], layer);
                }
            }
        } else if (profile.selected_tab == tab::best_rc) {
            if (profile.best_rating_records.empty()) {
                draw_empty(content, "No rating best plays yet.");
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.best_rating_records.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.best_rating_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    draw_best_rc_row(row, profile.best_rating_records[static_cast<size_t>(i)], i, layer);
                }
            }
        } else if (is_upload_tab(profile.selected_tab) && !profile.uploads.success) {
            draw_empty(content, profile.uploads.message.empty()
                                    ? "Uploaded content could not be loaded."
                                    : profile.uploads.message.c_str());
        } else if (profile.selected_tab == tab::songs) {
            if (profile.uploads.songs.empty()) {
                draw_empty(content, "No uploaded songs.");
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.uploads.songs.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.song_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    const auto& song = profile.uploads.songs[static_cast<size_t>(i)];
                    const std::string subtitle =
                        append_upload_status(song_subtitle(song), song.review_status, song.lifecycle_status);
                    draw_upload_row(row, song_label(song), subtitle, !busy, layer);
                }
            }
        } else if (profile.selected_tab == tab::charts) {
            if (profile.uploads.charts.empty()) {
                draw_empty(content, "No uploaded charts.");
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.uploads.charts.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.chart_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    const auto& chart = profile.uploads.charts[static_cast<size_t>(i)];
                    const std::string subtitle =
                        append_upload_status(chart.chart_author, chart.review_status, chart.lifecycle_status);
                    draw_upload_row(row, chart_label(chart), subtitle, !busy, layer);
                }
            }
        } else {
            const settings_layout settings = settings_layout_for(content);
            ui::section(content);
            ui::draw_text_in_rect("Settings", 18, settings.title, t.text, ui::text_align::left);
            ui::draw_text_in_rect("Profile Links", 13, settings.link_heading,
                                  t.text_secondary, ui::text_align::left);
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
            draw_profile_button(settings.save_links_button, "SAVE LINKS", !busy, t.accent, layer);
            ui::draw_text_in_rect("Public on profile and uploaded content cards.", 12, settings.link_hint,
                                  t.text_muted, ui::text_align::left);
            ui::divider(settings.avatar_divider, with_alpha(t.border, 150));
            ui::draw_text_in_rect("Profile Image", 13, settings.avatar_label,
                                  t.text_secondary, ui::text_align::left);
            draw_profile_button(settings.change_avatar_button, "CHANGE IMAGE", !busy, t.accent, layer);
            draw_profile_button(settings.remove_avatar_button, "REMOVE IMAGE",
                                !busy && !auth_state.avatar_url.empty(), t.error, layer);
            ui::divider(settings.account_divider, with_alpha(t.border, 150));
            ui::draw_text_in_rect("Delete this account from raythm-Server.", 13, settings.account_label,
                                  t.text_secondary, ui::text_align::left);
            ui::draw_text_in_rect(localization::tr_literal("This does not delete local songs or charts."),
                                  12, settings.account_description,
                                  t.text_muted, ui::text_align::left);
            draw_profile_button(settings.delete_account_button, "DELETE ACCOUNT", !busy, t.error, layer);
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
            draw_profile_button(delete_layout.confirm_button, "DELETE", !busy, t.error, layer);
            draw_profile_button(delete_layout.cancel_button, "CANCEL", !busy, t.text_muted, layer);
        } else if (profile.pending_delete != delete_target::none) {
            const item_delete_prompt_layout delete_layout = item_delete_prompt_layout_for();
            ui::surface(delete_layout.panel, with_alpha(t.panel, 245), with_alpha(t.error, 210), 2.0f);
            const std::string message = "Delete " + profile.pending_label + "? Hidden from Community listings.";
            ui::draw_text_in_rect(message.c_str(), 12, delete_layout.message, t.text, ui::text_align::left);
            draw_profile_button(delete_layout.confirm_button, "DELETE", !busy, t.error, layer);
            draw_profile_button(delete_layout.cancel_button, "CANCEL", !busy, t.text_muted, layer);
        }
        rlPopMatrix();
        if (avatar_picker.is_open()) {
            avatar_picker.draw();
        }
    });
}

}  // namespace title_profile_view
