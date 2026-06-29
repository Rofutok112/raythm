#include "shared/public_profile_view.h"

#include <array>
#include <cctype>
#include <string>

#include "shared/avatar_texture_cache.h"
#include "shared/public_profile_state.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui_modal.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace public_profile_view {
namespace {

constexpr Rectangle kModalRect{210.0f, 96.0f, 1500.0f, 888.0f};
constexpr float kHeaderHeight = 190.0f;
constexpr float kTabHeight = 46.0f;
constexpr float kContentTopGap = 30.0f;
constexpr float kContentOuterPadding = 42.0f;
constexpr float kAvatarSize = 126.0f;
constexpr int kTabCount = 4;
constexpr float kHeaderActionTop = 24.0f;
constexpr float kHeaderActionRightInset = 40.0f;
constexpr float kHeaderActionHeight = 42.0f;
constexpr float kHeaderActionGap = 14.0f;
constexpr float kCloseButtonWidth = 92.0f;
constexpr float kRelationshipButtonWidth = 112.0f;
constexpr float kHeaderAvatarTop = 40.0f;
constexpr float kHeaderTextGap = 28.0f;
constexpr float kHeaderTextReservedWidth = 420.0f;
constexpr float kTabWidth = 178.0f;
constexpr float kTabGap = 8.0f;
constexpr float kLinkRowHeight = 70.0f;
constexpr float kLinkRowGap = 10.0f;
constexpr float kWheelStep = 78.0f;

struct profile_header_layout {
    Rectangle avatar{};
    Rectangle display_name{};
    Rectangle label{};
    Rectangle rating_panel{};
    Rectangle rating_label{};
    Rectangle rating_value{};
    Rectangle rating_plays{};
};

struct metric_card_layout {
    Rectangle label{};
    Rectangle value{};
};

struct best_rc_row_layout {
    Rectangle rank{};
    Rectangle title{};
    Rectangle subtitle{};
    Rectangle summary{};
    Rectangle placement{};
};

struct link_row_layout {
    Rectangle label{};
    Rectangle url{};
};

struct label_value_layout {
    Rectangle label{};
    Rectangle value{};
};

struct overview_layout {
    std::array<Rectangle, kTabCount> metric_cards{};
    Rectangle summary_panel{};
    Rectangle summary_title{};
    std::array<label_value_layout, 3> summary_items{};
};

struct social_layout {
    Rectangle relationship_panel{};
    Rectangle title{};
    Rectangle relationship_value{};
    Rectangle relationship_hint{};
};

struct modal_layout {
    Rectangle modal_rect{};
    Rectangle content_rect{};
    Rectangle close_button_rect{};
    Rectangle relationship_button_rect{};
    Rectangle tab_bar_rect{};
    std::array<Rectangle, kTabCount> tab_rects{};
    profile_header_layout header{};
    ui::draw_layer layer = ui::draw_layer::modal;
};

struct tab_descriptor {
    tab value = tab::overview;
    const char* label = "";
};

struct tab_button_layout {
    tab_descriptor descriptor{};
    Rectangle rect{};
};

enum class header_action_style {
    close,
    relationship,
};

struct header_action_button {
    Rectangle rect{};
    const char* label = "";
    command_type command = command_type::none;
    bool enabled = true;
    header_action_style style = header_action_style::close;
};

constexpr std::array<tab_descriptor, kTabCount> kTabs = {{
    {tab::overview, "OVERVIEW"},
    {tab::best_rc, "BEST RC"},
    {tab::links, "LINKS"},
    {tab::social, "SOCIAL"},
}};

std::string avatar_label(const auth::public_profile& profile) {
    const std::string& source = profile.display_name.empty() ? profile.id : profile.display_name;
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
    return result.empty() ? "P" : result;
}

profile_header_layout make_profile_header_layout(Rectangle modal) {
    const Rectangle avatar{
        modal.x + kContentOuterPadding,
        modal.y + kHeaderAvatarTop,
        kAvatarSize,
        kAvatarSize,
    };
    const Rectangle text_block{
        avatar.x + avatar.width + kHeaderTextGap,
        avatar.y,
        modal.width - kHeaderTextReservedWidth,
        avatar.height,
    };
    const Rectangle rating_panel{
        text_block.x,
        avatar.y + 96.0f,
        360.0f,
        42.0f,
    };
    return {
        .avatar = avatar,
        .display_name = {text_block.x, avatar.y + 14.0f, text_block.width, 46.0f},
        .label = {text_block.x, avatar.y + 66.0f, text_block.width, 22.0f},
        .rating_panel = rating_panel,
        .rating_label = {rating_panel.x + 12.0f, rating_panel.y + 6.0f, 138.0f, 14.0f},
        .rating_value = {rating_panel.x + 12.0f, rating_panel.y + 18.0f, 112.0f, 22.0f},
        .rating_plays = {rating_panel.x + 180.0f, rating_panel.y + 14.0f, 150.0f, 18.0f},
    };
}

Rectangle tab_bar_rect_for(Rectangle modal) {
    return {
        modal.x + kContentOuterPadding,
        modal.y + kHeaderHeight,
        modal.width - kContentOuterPadding * 2.0f,
        kTabHeight,
    };
}

std::array<Rectangle, kTabCount> tab_rects_for(Rectangle modal) {
    std::array<Rectangle, kTabCount> tabs{};
    ui::hstack(tab_bar_rect_for(modal),
               kTabWidth,
               kTabGap,
               tabs);
    return tabs;
}

std::array<tab_button_layout, kTabCount> tab_buttons_for(const modal_layout& layout) {
    std::array<tab_button_layout, kTabCount> buttons{};
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        buttons[index] = {
            .descriptor = kTabs[index],
            .rect = layout.tab_rects[index],
        };
    }
    return buttons;
}

header_action_button close_header_action_for(const modal_layout& layout) {
    return {
        .rect = layout.close_button_rect,
        .label = "CLOSE",
        .command = command_type::close,
        .enabled = true,
        .style = header_action_style::close,
    };
}

header_action_button relationship_header_action_for(const modal_layout& layout,
                                                    const auth::public_profile& profile,
                                                    bool operation_active) {
    const public_profile_state::relationship_action_view relationship_action =
        public_profile_state::relationship_action_for(profile, operation_active);
    return {
        .rect = layout.relationship_button_rect,
        .label = relationship_action.label,
        .command = command_type::start_relationship_action,
        .enabled = relationship_action.enabled,
        .style = header_action_style::relationship,
    };
}

modal_layout make_layout(float open_anim, ui::draw_layer layer = ui::draw_layer::modal) {
    const Rectangle modal = ui::animated_modal_rect(kModalRect, open_anim);
    const Rectangle header_actions{
        modal.x,
        modal.y + kHeaderActionTop,
        modal.width - kHeaderActionRightInset,
        kHeaderActionHeight,
    };
    const ui::rect_pair close_split = ui::split_trailing(header_actions, kCloseButtonWidth, kHeaderActionGap);
    const ui::rect_pair relationship_split =
        ui::split_trailing(close_split.first, kRelationshipButtonWidth, kHeaderActionGap);
    const Rectangle tab_bar = tab_bar_rect_for(modal);
    const std::array<Rectangle, kTabCount> tabs = tab_rects_for(modal);
    return {
        .modal_rect = modal,
        .content_rect = {
            modal.x + kContentOuterPadding,
            modal.y + kHeaderHeight + kTabHeight + kContentTopGap,
            modal.width - kContentOuterPadding * 2.0f,
            modal.height - kHeaderHeight - kTabHeight - kContentTopGap - kContentOuterPadding,
        },
        .close_button_rect = close_split.second,
        .relationship_button_rect = relationship_split.second,
        .tab_bar_rect = tab_bar,
        .tab_rects = tabs,
        .header = make_profile_header_layout(modal),
        .layer = layer,
    };
}

float link_list_content_height(int item_count) {
    return ui::vertical_list_content_height(item_count, kLinkRowHeight, kLinkRowGap);
}

Rectangle link_row_rect(Rectangle content, int index, float scroll_y) {
    return ui::vertical_list_row_rect(content, index, kLinkRowHeight, kLinkRowGap, scroll_y);
}

constexpr metric_card_layout metric_card_layout_for(Rectangle rect) {
    return {
        .label = {rect.x + 18.0f, rect.y + 14.0f, rect.width - 36.0f, 22.0f},
        .value = {rect.x + 18.0f, rect.y + 42.0f, rect.width - 36.0f, 34.0f},
    };
}

constexpr best_rc_row_layout best_rc_row_layout_for(Rectangle row) {
    return {
        .rank = {row.x + 18.0f, row.y + 18.0f, 54.0f, 30.0f},
        .title = {row.x + 78.0f, row.y + 9.0f, 520.0f, 24.0f},
        .subtitle = {row.x + 78.0f, row.y + 38.0f, 520.0f, 18.0f},
        .summary = {row.x + 650.0f, row.y + 12.0f, 430.0f, 22.0f},
        .placement = {row.x + 650.0f, row.y + 38.0f, 360.0f, 20.0f},
    };
}

constexpr link_row_layout link_row_layout_for(Rectangle row) {
    return {
        .label = {row.x + 18.0f, row.y + 10.0f, row.width - 36.0f, 24.0f},
        .url = {row.x + 18.0f, row.y + 40.0f, row.width - 36.0f, 20.0f},
    };
}

constexpr label_value_layout summary_item_layout_for(Rectangle summary, float x, float value_width) {
    return {
        .label = {summary.x + x, summary.y + 62.0f, value_width, 20.0f},
        .value = {summary.x + x, summary.y + 84.0f, value_width, 32.0f},
    };
}

overview_layout overview_layout_for(Rectangle content) {
    overview_layout layout{};
    constexpr float gap = 12.0f;
    ui::hstack_fill({content.x, content.y, content.width, 92.0f}, gap, layout.metric_cards);
    const Rectangle summary{content.x, content.y + 118.0f, content.width, 178.0f};
    layout.summary_panel = summary;
    layout.summary_title = {summary.x + 18.0f, summary.y + 16.0f, summary.width - 36.0f, 26.0f};
    layout.summary_items[0] = summary_item_layout_for(summary, 18.0f, 160.0f);
    layout.summary_items[1] = summary_item_layout_for(summary, 270.0f, 280.0f);
    layout.summary_items[2] = summary_item_layout_for(summary, 620.0f, 360.0f);
    return layout;
}

constexpr social_layout social_layout_for(Rectangle content) {
    const Rectangle relationship{content.x, content.y, content.width, 138.0f};
    return {
        .relationship_panel = relationship,
        .title = {relationship.x + 18.0f, relationship.y + 16.0f, relationship.width - 36.0f, 26.0f},
        .relationship_value = {relationship.x + 18.0f, relationship.y + 58.0f, 360.0f, 34.0f},
        .relationship_hint = {relationship.x + 18.0f, relationship.y + 96.0f, relationship.width - 36.0f, 22.0f},
    };
}

std::string relationship_label(const auth::public_profile& profile) {
    if (profile.relationship_status == "none") {
        return "Not connected";
    }
    if (profile.relationship_status == "pending_incoming") {
        return "Incoming request";
    }
    if (profile.relationship_status == "pending_outgoing") {
        return "Request pending";
    }
    if (profile.relationship_status == "accepted") {
        return "Friends";
    }
    if (profile.relationship_status == "blocked") {
        return "Blocked";
    }
    if (profile.relationship_status == "self") {
        return "This is you";
    }
    if (profile.relationship_status == "unavailable") {
        return "Unavailable";
    }
    return "Unknown";
}

void draw_metric_card(Rectangle rect, const char* label, const std::string& value, Color tone) {
    const auto& t = *g_theme;
    const metric_card_layout card = metric_card_layout_for(rect);
    ui::surface(rect,
                with_alpha(t.row, 205),
                with_alpha(lerp_color(t.border, tone, 0.35f), 210),
                1.2f);
    ui::draw_text_in_rect(label, 12, card.label, t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value.c_str(), 23, card.value, tone, ui::text_align::left);
}

void draw_header_action_button(const header_action_button& button, ui::draw_layer layer) {
    const auto& t = *g_theme;
    if (button.style == header_action_style::relationship) {
        ui::button(button.rect, button.label, {
            .layer = layer,
            .font_size = 13,
            .border_width = 1.5f,
            .bg = button.enabled ? t.row_selected : t.row,
            .bg_hover = button.enabled ? t.row_active : t.row_hover,
            .text_color = button.enabled ? t.accent : t.text_muted,
            .custom_colors = true,
            .interactive = button.enabled,
        });
        return;
    }

    ui::button(button.rect, button.label, {
        .layer = layer,
        .font_size = 14,
        .border_width = 2.0f,
        .bg = t.row_soft,
        .bg_hover = t.row_soft_hover,
        .text_color = t.text_muted,
        .custom_colors = true,
        .interactive = button.enabled,
    });
}

void draw_tab_bar(const model& state, const modal_layout& layout) {
    const auto& t = *g_theme;
    ui::bar_surface(layout.tab_bar_rect, with_alpha(t.bg, 70), with_alpha(t.border, 180));

    for (const tab_button_layout& button : tab_buttons_for(layout)) {
        const bool selected = button.descriptor.value == state.selected_tab;
        ui::tab_button(button.rect, button.descriptor.label, {
            .layer = layout.layer,
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

void draw_empty(Rectangle content, const char* message) {
    ui::section(content);
    ui::draw_text_in_rect(message, 15, content, g_theme->text_muted);
}

std::string ranking_subtitle(const auth::profile_ranking_record& item) {
    std::string result = item.artist;
    if (!item.genre.empty()) {
        result += result.empty() ? item.genre : " / " + item.genre;
    }
    if (!item.difficulty_name.empty()) {
        result += result.empty() ? item.difficulty_name : " / " + item.difficulty_name;
    }
    return result;
}

std::string best_rc_summary(const auth::profile_ranking_record& item) {
    return TextFormat("RC %.2f  |  +%.2f  |  %.1f%%",
                      item.play_rating,
                      item.rating_contribution,
                      item.rating_contribution_percent);
}

void draw_best_rc_row(Rectangle row, const auth::profile_ranking_record& item, int index) {
    const auto& t = *g_theme;
    const best_rc_row_layout row_layout = best_rc_row_layout_for(row);
    ui::hover_surface(row, {
        .border_width = 1.2f,
        .fill = with_alpha(t.row, 195),
        .fill_hover = with_alpha(t.row_hover, 225),
        .border_color = with_alpha(t.border, 205),
        .custom_colors = true,
    });
    ui::draw_text_in_rect(TextFormat("#%d", index + 1),
                          16,
                          row_layout.rank,
                          t.accent,
                          ui::text_align::left);
    ui::draw_text_in_rect(item.song_title.c_str(), 15,
                          row_layout.title,
                          t.text,
                          ui::text_align::left);
    const std::string subtitle = ranking_subtitle(item);
    ui::draw_text_in_rect(subtitle.c_str(), 11,
                          row_layout.subtitle,
                          t.text_muted,
                          ui::text_align::left);
    const std::string summary = best_rc_summary(item);
    ui::draw_text_in_rect(summary.c_str(), 13,
                          row_layout.summary,
                          t.rank_ss,
                          ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("Online #%d / %d", item.placement, item.score),
                          12,
                          row_layout.placement,
                          t.text_secondary,
                          ui::text_align::left);
}

void draw_overview(const auth::public_profile& profile, Rectangle content) {
    const auto& t = *g_theme;
    const overview_layout layout = overview_layout_for(content);
    ui::section(content);
    draw_metric_card(layout.metric_cards[0], "Season 0 Rating",
                     TextFormat("%.0f", profile.rating.rating), t.accent);
    draw_metric_card(layout.metric_cards[1], "Rating Rank",
                     profile.rating.rank > 0 ? "#" + std::to_string(profile.rating.rank) : "Unranked",
                     t.rank_ss);
    draw_metric_card(layout.metric_cards[2], "Eligible Plays",
                     std::to_string(profile.rating.eligible_play_count), t.text);
    draw_metric_card(layout.metric_cards[3], "Best RC Plays",
                     std::to_string(profile.best_rating_records.size()), t.success);

    ui::surface(layout.summary_panel, with_alpha(t.row, 185), with_alpha(t.border, 205), 1.2f);
    ui::draw_text_in_rect("Profile Summary", 16, layout.summary_title, t.text, ui::text_align::left);
    ui::draw_text_in_rect("Public links",
                          12,
                          layout.summary_items[0].label,
                          t.text_muted, ui::text_align::left);
    const std::string link_count = std::to_string(profile.external_links.size());
    ui::draw_text_in_rect(link_count.c_str(),
                          22,
                          layout.summary_items[0].value,
                          t.accent, ui::text_align::left);
    ui::draw_text_in_rect("Relationship",
                          12,
                          layout.summary_items[1].label,
                          t.text_muted, ui::text_align::left);
    const std::string relation = relationship_label(profile);
    ui::draw_text_in_rect(relation.c_str(),
                          18,
                          layout.summary_items[1].value,
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("Ruleset",
                          12,
                          layout.summary_items[2].label,
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(profile.rating.ruleset_version.empty() ? "Season default" : profile.rating.ruleset_version.c_str(),
                          18,
                          layout.summary_items[2].value,
                          t.text, ui::text_align::left);
}

void draw_best_rc(const model& state, const auth::public_profile& profile, Rectangle content) {
    if (profile.best_rating_records.empty()) {
        draw_empty(content, "No rating best plays yet.");
        return;
    }

    ui::section(content);
    const ui::scroll_offset_state scroll =
        ui::scroll_offset_state_for(
            content,
            link_list_content_height(static_cast<int>(profile.best_rating_records.size())),
            state.best_rating_scroll);
    ui::scoped_clip_rect clip(content);
    const ui::index_range visible_rows = ui::vertical_list_visible_range(
        profile.best_rating_records.size(), content, kLinkRowHeight, kLinkRowGap, scroll.offset);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        const Rectangle row = link_row_rect(content, i, scroll.offset);
        draw_best_rc_row(row, profile.best_rating_records[static_cast<size_t>(i)], i);
    }
}

void draw_links(const model& state, const auth::public_profile& profile, Rectangle content) {
    const auto& t = *g_theme;
    if (profile.external_links.empty()) {
        draw_empty(content, "No public profile links.");
        return;
    }

    ui::section(content);
    const ui::scroll_offset_state scroll =
        ui::scroll_offset_state_for(
            content,
            link_list_content_height(static_cast<int>(profile.external_links.size())),
            state.link_scroll);
    ui::scoped_clip_rect clip(content);
    const ui::index_range visible_rows = ui::vertical_list_visible_range(
        profile.external_links.size(), content, kLinkRowHeight, kLinkRowGap, scroll.offset);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        const Rectangle row = link_row_rect(content, i, scroll.offset);
        ui::surface(row, with_alpha(t.row, 205), with_alpha(t.border, 205), 1.2f);
        const link_row_layout row_layout = link_row_layout_for(row);
        const auth::external_link& link = profile.external_links[static_cast<size_t>(i)];
        const std::string label = link.label.empty() ? "Link" : link.label;
        ui::draw_text_in_rect(label.c_str(), 17, row_layout.label, t.text, ui::text_align::left);
        ui::draw_text_in_rect(link.url.c_str(), 13, row_layout.url, t.text_muted, ui::text_align::left);
    }
}

void draw_social(const model& state, const auth::public_profile& profile, const modal_layout& layout) {
    const auto& t = *g_theme;
    const Rectangle content = layout.content_rect;
    const social_layout social = social_layout_for(content);
    ui::section(content);
    ui::surface(social.relationship_panel, with_alpha(t.row, 185), with_alpha(t.border, 205), 1.2f);
    ui::draw_text_in_rect("Relationship", 16, social.title, t.text, ui::text_align::left);
    const std::string relation = relationship_label(profile);
    ui::draw_text_in_rect(relation.c_str(), 22, social.relationship_value, t.accent, ui::text_align::left);
    const public_profile_state::relationship_action_view relationship_action =
        public_profile_state::relationship_action_for(profile, state.relationship_operation_active);
    ui::draw_text_in_rect(relationship_action.enabled ? "Use the action button in the header to update this relationship."
                                                      : "No relationship action is currently available.",
                          13,
                          social.relationship_hint,
                          t.text_muted, ui::text_align::left);
}

void draw_profile_body(const model& state, const modal_layout& layout) {
    const auto& t = *g_theme;
    const Rectangle content = layout.content_rect;
    if (state.loading && !state.loaded_once) {
        ui::draw_text_in_rect("Loading profile...", 20, content, t.text_muted, ui::text_align::center);
        return;
    }
    if (!state.result.success || !state.result.profile.has_value()) {
        const std::string message = state.result.message.empty() ? "Could not load profile." : state.result.message;
        ui::draw_text_in_rect(message.c_str(), 20, content, t.text_muted, ui::text_align::center);
        return;
    }

    const auth::public_profile& profile = *state.result.profile;
    const profile_header_layout& header = layout.header;
    avatar_texture_cache::draw_avatar(
        header.avatar,
        profile.avatar_url,
        avatar_label(profile),
        t.row_soft_selected,
        t.text,
        28,
        state.avatar_base_url);
    ui::frame(header.avatar, t.border_light, 1.4f);

    ui::draw_text_in_rect(profile.display_name.empty() ? "Unknown Player" : profile.display_name.c_str(),
                          34,
                          header.display_name,
                          t.text,
                          ui::text_align::left);
    ui::draw_text_in_rect("PROFILE", 13, header.label, t.accent, ui::text_align::left);
    ui::surface(header.rating_panel, with_alpha(t.row_soft, 226), t.border_light, 1.0f);
    ui::draw_text_in_rect("SEASON 0 RATING", 11, header.rating_label, t.text_muted, ui::text_align::left);
    const std::string rating_text = TextFormat("%.0f", profile.rating.rating);
    ui::draw_text_in_rect(rating_text.c_str(), 19, header.rating_value, t.accent, ui::text_align::left);
    const std::string plays_text = TextFormat("%d plays", profile.rating.eligible_play_count);
    ui::draw_text_in_rect(plays_text.c_str(), 12, header.rating_plays, t.text_muted, ui::text_align::right);

    draw_header_action_button(relationship_header_action_for(layout, profile, state.relationship_operation_active),
                              layout.layer);
    draw_tab_bar(state, layout);

    if (state.selected_tab == tab::best_rc) {
        draw_best_rc(state, profile, content);
    } else if (state.selected_tab == tab::links) {
        draw_links(state, profile, content);
    } else if (state.selected_tab == tab::social) {
        draw_social(state, profile, layout);
    } else {
        draw_overview(profile, content);
    }
}

}  // namespace

Rectangle bounds(float open_anim) {
    return ui::animated_modal_rect(kModalRect, open_anim);
}

scroll_offsets clamped_scroll_offsets(const model& state, ui::draw_layer layer) {
    const modal_layout layout = make_layout(state.open_anim, layer);
    if (!state.result.success || !state.result.profile.has_value()) {
        return {};
    }

    const auth::public_profile& profile = *state.result.profile;
    const float link_content_height = link_list_content_height(static_cast<int>(profile.external_links.size()));
    const float best_rating_content_height =
        link_list_content_height(static_cast<int>(profile.best_rating_records.size()));
    return {
        .link_scroll = ui::scroll_offset_state_for(
            layout.content_rect, link_content_height, state.link_scroll).offset,
        .best_rating_scroll = ui::scroll_offset_state_for(
            layout.content_rect, best_rating_content_height, state.best_rating_scroll).offset,
    };
}

input_result handle_input(const model& state, ui::draw_layer layer) {
    input_result result;
    auto action = [&result](command command_value) {
        input_result with_action = result;
        with_action.action = command_value;
        return with_action;
    };
    const modal_layout layout = make_layout(state.open_anim, layer);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = ui::mouse_wheel_move();
    scroll_offsets scroll = clamped_scroll_offsets(state, layer);
    if (scroll.link_scroll != state.link_scroll ||
        scroll.best_rating_scroll != state.best_rating_scroll) {
        result.scroll_changed = true;
        result.scroll = scroll;
    }
    const header_action_button close_action = close_header_action_for(layout);
    if (close_action.enabled && ui::is_clicked(close_action.rect, layout.layer)) {
        return action({.type = close_action.command});
    }
    if (state.result.success &&
        state.result.profile.has_value()) {
        const header_action_button relationship_action =
            relationship_header_action_for(layout, *state.result.profile, state.relationship_operation_active);
        if (relationship_action.enabled && ui::is_clicked(relationship_action.rect, layout.layer)) {
            return action({.type = relationship_action.command});
        }
    }
    if (state.result.success && state.result.profile.has_value()) {
        const auth::public_profile& profile = *state.result.profile;
        for (const tab_button_layout& button : tab_buttons_for(layout)) {
            if (ui::is_clicked(button.rect, layout.layer)) {
                return action({
                    .type = command_type::select_tab,
                    .selected_tab = button.descriptor.value,
                });
            }
        }
        if (state.selected_tab == tab::links &&
            wheel != 0.0f &&
            ui::contains_point(layout.content_rect, mouse)) {
            const ui::scroll_offset_state scroll_state = ui::wheel_scrolled_offset_state(
                layout.content_rect,
                mouse,
                wheel,
                link_list_content_height(static_cast<int>(profile.external_links.size())),
                scroll.link_scroll,
                kWheelStep);
            scroll.link_scroll = scroll_state.offset;
            result.scroll_changed = true;
            result.scroll = scroll;
            return result;
        }
        if (state.selected_tab == tab::best_rc &&
            wheel != 0.0f &&
            ui::contains_point(layout.content_rect, mouse)) {
            const ui::scroll_offset_state scroll_state = ui::wheel_scrolled_offset_state(
                layout.content_rect,
                mouse,
                wheel,
                link_list_content_height(static_cast<int>(profile.best_rating_records.size())),
                scroll.best_rating_scroll,
                kWheelStep);
            scroll.best_rating_scroll = scroll_state.offset;
            result.scroll_changed = true;
            result.scroll = scroll;
            return result;
        }
    }
    if (ui::modal_outside_released(layout.modal_rect, mouse)) {
        return action({.type = command_type::close});
    }
    return result;
}

void draw(const model& state, ui::draw_layer layer, bool draw_backdrop) {
    ui::enqueue_draw_command(layer, [state, layer, draw_backdrop]() {
        if (draw_backdrop) {
            ui::draw_fullscreen_overlay(with_alpha(BLACK, 132));
        }
        const modal_layout layout = make_layout(state.open_anim, layer);
        ui::panel(layout.modal_rect);
        draw_header_action_button(close_header_action_for(layout), layout.layer);
        draw_profile_body(state, layout);
    });
}

}  // namespace public_profile_view
