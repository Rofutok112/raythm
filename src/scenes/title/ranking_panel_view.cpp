#include "title/ranking_panel_view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "ranking_service.h"
#include "scene_common.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

constexpr float kRankingRowHeight = 90.0f;
constexpr float kScrollPadding = 12.0f;
constexpr float kEmptyMessageOffsetY = 60.0f;
constexpr float kEmptyMessageHeight = 42.0f;
constexpr float kRowGap = 6.0f;
constexpr float kRevealOffsetX = 66.0f;
constexpr float kClipSlack = 6.0f;
constexpr float kRowBorderWidth = 1.5f;
constexpr float kTopLineOffsetY = 12.0f;
constexpr float kStatLabelOffsetY = 40.5f;
constexpr float kStatValueOffsetY = 54.0f;
constexpr float kPlacementOffsetX = 12.0f;
constexpr float kPlacementOffsetY = 9.0f;
constexpr float kPlacementWidth = 66.0f;
constexpr float kPlacementVerticalInset = 18.0f;
constexpr float kRankRightInset = 72.0f;
constexpr float kRankOffsetY = 13.5f;
constexpr float kRankWidth = 60.0f;
constexpr float kRankVerticalInset = 27.0f;
constexpr float kScoreOffsetFromRank = 144.0f;
constexpr float kScoreOffsetY = 19.5f;
constexpr float kScoreWidth = 126.0f;
constexpr float kScoreHeight = 39.0f;
constexpr float kDetailsOffsetX = 96.0f;
constexpr float kDetailsScoreGap = 4.5f;
constexpr float kTimeWidth = 33.0f;
constexpr float kNameTimeGap = 1.5f;
constexpr float kStatColumnGap = 27.0f;
constexpr float kComboMaxWidth = 114.0f;

const char* rank_label(rank value) {
    switch (value) {
        case rank::ss: return "SS";
        case rank::s: return "S";
        case rank::aa: return "AA";
        case rank::a: return "A";
        case rank::b: return "B";
        case rank::c: return "C";
        case rank::f: return "F";
    }
    return "?";
}

Color rank_color(rank value) {
    const auto& theme = *g_theme;
    switch (value) {
        case rank::ss: return theme.rank_ss;
        case rank::s: return theme.rank_s;
        case rank::aa: return theme.rank_aa;
        case rank::a: return theme.rank_a;
        case rank::b: return theme.rank_b;
        case rank::c: return theme.rank_c;
        case rank::f: return theme.rank_f;
    }
    return theme.text_secondary;
}

std::string format_score(int value) {
    std::string digits = std::to_string(std::max(0, value));
    for (int insert_at = static_cast<int>(digits.size()) - 3; insert_at > 0; insert_at -= 3) {
        digits.insert(static_cast<size_t>(insert_at), ",");
    }
    return digits;
}

std::optional<std::chrono::system_clock::time_point> parse_recorded_at_utc(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    std::tm utc_tm{};
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(value.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return std::nullopt;
    }

    utc_tm.tm_year = year - 1900;
    utc_tm.tm_mon = month - 1;
    utc_tm.tm_mday = day;
    utc_tm.tm_hour = hour;
    utc_tm.tm_min = minute;
    utc_tm.tm_sec = second;
    utc_tm.tm_isdst = 0;

#if defined(_WIN32)
    const std::time_t utc_time = _mkgmtime(&utc_tm);
#else
    const std::time_t utc_time = timegm(&utc_tm);
#endif
    if (utc_time < 0) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(utc_time);
}

std::string format_relative_recorded_at(const std::string& recorded_at) {
    const auto tp = parse_recorded_at_utc(recorded_at);
    if (!tp.has_value()) {
        return "-";
    }

    const auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - *tp).count();
    if (diff < 0) {
        diff = 0;
    }
    if (diff < 60) {
        return "now";
    }
    if (diff < 3600) {
        return std::to_string(diff / 60) + "m";
    }
    if (diff < 86400) {
        return std::to_string(diff / 3600) + "h";
    }
    return std::to_string(diff / 86400) + "d";
}

}  // namespace

namespace title_ranking_view {

float content_height(const ranking_service::listing& listing) {
    const size_t count = listing.entries.empty() ? 1 : listing.entries.size();
    return static_cast<float>(count) * kRankingRowHeight;
}

float max_scroll(Rectangle list_rect, const ranking_service::listing& listing) {
    return std::max(0.0f, content_height(listing) - list_rect.height + kScrollPadding);
}

std::optional<ranking_service::source> hit_test_source(const draw_config& config, Vector2 point) {
    if (ui::is_hovered(config.source_local_rect) && CheckCollisionPointRec(point, config.source_local_rect)) {
        return ranking_service::source::local;
    }
    if (ui::is_hovered(config.source_online_rect) && CheckCollisionPointRec(point, config.source_online_rect)) {
        return ranking_service::source::online;
    }
    return std::nullopt;
}

void draw(const song_select::ranking_panel_state& panel, const draw_config& config) {
    const auto& t = *g_theme;
    ui::draw_text_in_rect("RANKINGS", 24, config.header_rect,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    ui::draw_button_colored(config.source_local_rect, "LOCAL", 15,
                            with_alpha(panel.selected_source == ranking_service::source::local ? config.button_selected : config.button_base,
                                       panel.selected_source == ranking_service::source::local ? config.selected_row_alpha : config.normal_row_alpha),
                            with_alpha(panel.selected_source == ranking_service::source::local ? config.button_selected_hover : config.button_hover,
                                       panel.selected_source == ranking_service::source::local ? config.selected_hover_row_alpha : config.hover_row_alpha),
                            with_alpha(t.text, config.alpha), 1.5f);
    ui::draw_button_colored(config.source_online_rect, "ONLINE", 15,
                            with_alpha(panel.selected_source == ranking_service::source::online ? config.button_selected : config.button_base,
                                       panel.selected_source == ranking_service::source::online ? config.selected_row_alpha : config.normal_row_alpha),
                            with_alpha(panel.selected_source == ranking_service::source::online ? config.button_selected_hover : config.button_hover,
                                       panel.selected_source == ranking_service::source::online ? config.selected_hover_row_alpha : config.hover_row_alpha),
                            with_alpha(t.text, config.alpha), 1.5f);

    ui::scoped_clip_rect clip(config.list_rect);
    const float base_y = config.list_rect.y - panel.scroll_y;
    const float reveal_anim = panel.reveal_anim;
    if (!panel.listing.available || panel.listing.entries.empty()) {
        const std::string message = panel.listing.message.empty()
            ? std::string("No ") + (panel.selected_source == ranking_service::source::local ? "local" : "online") + " entries yet."
            : panel.listing.message;
        ui::draw_text_in_rect(message.c_str(), 20,
                              {config.list_rect.x, config.list_rect.y + kEmptyMessageOffsetY,
                               config.list_rect.width, kEmptyMessageHeight},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        return;
    }

    for (int i = 0; i < static_cast<int>(panel.listing.entries.size()); ++i) {
        const ranking_service::entry& entry = panel.listing.entries[static_cast<size_t>(i)];
        const float row_reveal_t =
            tween::ease_out_quad(std::clamp((reveal_anim - static_cast<float>(i) * 0.075f) / 0.24f, 0.0f, 1.0f));
        if (row_reveal_t <= 0.0f) {
            continue;
        }

        const Rectangle base_row = {
            config.list_rect.x,
            base_y + static_cast<float>(i) * kRankingRowHeight,
            config.list_rect.width,
            kRankingRowHeight - kRowGap
        };
        const Rectangle row = {
            base_row.x + (1.0f - row_reveal_t) * kRevealOffsetX,
            base_row.y,
            base_row.width,
            base_row.height
        };
        if (row.y + row.height < config.list_rect.y - kClipSlack ||
            row.y > config.list_rect.y + config.list_rect.height + kClipSlack) {
            continue;
        }

        const unsigned char row_alpha = static_cast<unsigned char>(config.normal_row_alpha * row_reveal_t);
        const unsigned char content_alpha = static_cast<unsigned char>(config.alpha * row_reveal_t);
        ui::draw_rect_f(row, with_alpha(config.button_base, row_alpha));
        ui::draw_rect_lines(row, kRowBorderWidth,
                            with_alpha(t.border_light,
                                       static_cast<unsigned char>(130.0f * config.play_t * row_reveal_t)));

        constexpr int kNameFontSize = 13;
        constexpr int kTimeFontSize = 12;
        constexpr int kStatLabelFontSize = 10;
        constexpr int kStatValueFontSize = 12;
        const float top_line_y = row.y + kTopLineOffsetY;
        const float stat_label_y = row.y + kStatLabelOffsetY;
        const float stat_value_y = row.y + kStatValueOffsetY;
        const Rectangle placement_rect = {
            row.x + kPlacementOffsetX,
            row.y + kPlacementOffsetY,
            kPlacementWidth,
            row.height - kPlacementVerticalInset
        };
        const Rectangle rank_rect = {
            row.x + row.width - kRankRightInset,
            row.y + kRankOffsetY,
            kRankWidth,
            row.height - kRankVerticalInset
        };
        const Rectangle score_rect = {
            rank_rect.x - kScoreOffsetFromRank,
            row.y + kScoreOffsetY,
            kScoreWidth,
            kScoreHeight
        };
        const float details_x = row.x + kDetailsOffsetX;
        const float details_right = std::max(details_x, score_rect.x - kDetailsScoreGap);
        const Rectangle time_rect = {
            details_right - kTimeWidth,
            top_line_y,
            kTimeWidth,
            ui::text_layout_font_size(static_cast<float>(kTimeFontSize))
        };
        const Rectangle name_rect = {
            details_x,
            top_line_y,
            std::max(0.0f, time_rect.x - details_x - kNameTimeGap),
            ui::text_layout_font_size(static_cast<float>(kNameFontSize))
        };
        const float combo_width = std::min(kComboMaxWidth,
                                           std::max(0.0f, (details_right - details_x - kStatColumnGap) * 0.5f));
        const float accuracy_x = details_x + combo_width + kStatColumnGap;
        const float accuracy_width = std::max(0.0f, details_right - accuracy_x);
        const Rectangle combo_label_rect = {
            details_x,
            stat_label_y,
            combo_width,
            ui::text_layout_font_size(static_cast<float>(kStatLabelFontSize))
        };
        const Rectangle combo_value_rect = {
            details_x,
            stat_value_y,
            combo_width,
            ui::text_layout_font_size(static_cast<float>(kStatValueFontSize))
        };
        const Rectangle accuracy_label_rect = {
            accuracy_x,
            stat_label_y,
            accuracy_width,
            ui::text_layout_font_size(static_cast<float>(kStatLabelFontSize))
        };
        const Rectangle accuracy_value_rect = {
            accuracy_x,
            stat_value_y,
            accuracy_width,
            ui::text_layout_font_size(static_cast<float>(kStatValueFontSize))
        };

        ui::draw_text_in_rect(TextFormat("#%d", i + 1), 18, placement_rect,
                              with_alpha(t.text_secondary, content_alpha), ui::text_align::center);
        draw_marquee_text(entry.player_display_name.empty() ? "Unknown Player" : entry.player_display_name.c_str(),
                          name_rect, kNameFontSize, with_alpha(t.text, content_alpha), GetTime());
        ui::draw_text_in_rect(format_relative_recorded_at(entry.recorded_at).c_str(), kTimeFontSize,
                              time_rect,
                              with_alpha(t.text_muted, content_alpha), ui::text_align::right);
        ui::draw_text_in_rect("Max Combo", kStatLabelFontSize,
                              combo_label_rect,
                              with_alpha(t.text_muted, content_alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%dx", entry.max_combo), kStatValueFontSize,
                              combo_value_rect,
                              with_alpha(entry.is_full_combo ? t.success : t.text, content_alpha),
                              ui::text_align::left);
        ui::draw_text_in_rect("Accuracy", kStatLabelFontSize,
                              accuracy_label_rect,
                              with_alpha(t.text_muted, content_alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%.2f%%", entry.accuracy), kStatValueFontSize,
                              accuracy_value_rect,
                              with_alpha(t.text, content_alpha), ui::text_align::left);
        ui::draw_text_in_rect(format_score(entry.score).c_str(), 18,
                              score_rect,
                              with_alpha(t.text, content_alpha), ui::text_align::right);
        ui::draw_text_in_rect(rank_label(entry.clear_rank()), 24,
                              rank_rect,
                              with_alpha(rank_color(entry.clear_rank()), content_alpha), ui::text_align::center);
    }
}

}  // namespace title_ranking_view
