#include "title/ranking_panel_view.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "ranking_service.h"
#include "scene_common.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

constexpr float kRankingRowHeight = 52.0f;
constexpr float kScrollPadding = 12.0f;
constexpr float kEmptyMessageOffsetY = 60.0f;
constexpr float kEmptyMessageHeight = 42.0f;
constexpr float kHeaderRowHeight = 38.0f;
constexpr float kBrowseRankRowHeight = 56.0f;
constexpr float kBrowseRankRowStep = 61.0f;
constexpr float kAvatarSize = 44.0f;
constexpr float kRevealOffsetX = 0.0f;
constexpr float kClipSlack = 6.0f;

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

std::string avatar_initial(const ranking_service::entry& entry) {
    const std::string& name = entry.player_display_name;
    if (name.empty()) {
        return "?";
    }
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

void draw_avatar(Rectangle rect,
                 const ranking_service::entry& entry,
                 unsigned char alpha,
                 const std::string& avatar_base_url) {
    avatar_texture_cache::draw_avatar(
        rect,
        entry.player_avatar_url,
        avatar_initial(entry),
        with_alpha(g_theme->row_soft_selected, static_cast<unsigned char>(alpha * 0.78f)),
        with_alpha(g_theme->text_secondary, alpha),
        13,
        avatar_base_url);
    ui::draw_rect_lines(rect, 1.0f, with_alpha(g_theme->border_light, alpha));
}

bool is_self_entry(const ranking_service::entry& entry,
                   const song_select::ranking_panel_state& panel,
                   const title_ranking_view::draw_config& config) {
    if (!config.self_player_display_name.empty() &&
        entry.player_display_name == config.self_player_display_name) {
        return true;
    }
    return panel.best_entry.has_value() &&
           panel.best_entry->player_display_name == entry.player_display_name &&
           panel.best_entry->score == entry.score;
}

}  // namespace

namespace title_ranking_view {

float content_height(const ranking_service::listing& listing) {
    const size_t count = listing.entries.empty() ? 1 : listing.entries.size();
    return kHeaderRowHeight + static_cast<float>(count) * kBrowseRankRowStep;
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
    ui::draw_text_in_rect(panel.selected_source == ranking_service::source::online ? "GLOBAL RANKING" : "ローカルランキング",
                          16, {config.header_rect.x, config.header_rect.y, 280.0f, config.header_rect.height},
                          with_alpha(t.accent, config.alpha), ui::text_align::left);
    ui::draw_button_colored(config.source_online_rect, "GLOBAL", 14,
                            with_alpha(panel.selected_source == ranking_service::source::online ? config.button_selected : config.button_base,
                                       panel.selected_source == ranking_service::source::online ? config.selected_row_alpha : config.normal_row_alpha),
                            with_alpha(panel.selected_source == ranking_service::source::online ? config.button_selected_hover : config.button_hover,
                                       panel.selected_source == ranking_service::source::online ? config.selected_hover_row_alpha : config.hover_row_alpha),
                            with_alpha(t.text, config.alpha), 1.5f);
    ui::draw_button_colored(config.source_local_rect, "LOCAL", 14,
                            with_alpha(panel.selected_source == ranking_service::source::local ? config.button_selected : config.button_base,
                                       panel.selected_source == ranking_service::source::local ? config.selected_row_alpha : config.normal_row_alpha),
                            with_alpha(panel.selected_source == ranking_service::source::local ? config.button_selected_hover : config.button_hover,
                                       panel.selected_source == ranking_service::source::local ? config.selected_hover_row_alpha : config.hover_row_alpha),
                            with_alpha(t.text, config.alpha), 1.5f);

    ui::draw_rect_f(config.list_rect, with_alpha(config.button_base, config.normal_row_alpha));
    ui::draw_rect_lines(config.list_rect, 1.0f, with_alpha(t.border_light, config.alpha));
    const float list_right = config.list_rect.x + config.list_rect.width;
    const float clear_right = list_right - 18.0f;
    const float acc_right = clear_right - 58.0f;
    const float date_right = acc_right - 88.0f;
    const float score_right = date_right - 92.0f;
    const float score_width = 126.0f;
    const float date_width = 58.0f;
    const float acc_width = 76.0f;
    const float clear_width = 56.0f;
    {
        const Rectangle header_clip_rect = {config.list_rect.x, config.list_rect.y,
                                            config.list_rect.width, kHeaderRowHeight};
        ui::scoped_clip_rect header_clip(header_clip_rect);
        ui::draw_text_in_rect("#", 11, {config.list_rect.x + 12.0f, config.list_rect.y + 12.0f, 32.0f, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect("PLAYER", 11, {config.list_rect.x + 104.0f, config.list_rect.y + 12.0f, 138.0f, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect("SCORE", 11,
                              {score_right - score_width, config.list_rect.y + 12.0f, score_width, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
        ui::draw_text_in_rect("DATE", 11,
                              {date_right - date_width, config.list_rect.y + 12.0f, date_width, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
        ui::draw_text_in_rect("ACC", 11,
                              {acc_right - acc_width, config.list_rect.y + 12.0f, acc_width, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
        ui::draw_text_in_rect("CLEAR", 11,
                              {clear_right - clear_width, config.list_rect.y + 12.0f, clear_width, 16.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }

    const float base_y = config.list_rect.y + kHeaderRowHeight - panel.scroll_y;
    const float reveal_anim = panel.reveal_anim;
    const Rectangle entries_clip_rect = {config.list_rect.x, config.list_rect.y + kHeaderRowHeight,
                                         config.list_rect.width,
                                         std::max(0.0f, config.list_rect.height - kHeaderRowHeight)};
    ui::scoped_clip_rect entries_clip(entries_clip_rect);
    if (!panel.listing.available || panel.listing.entries.empty()) {
        const std::string message = panel.listing.message.empty()
            ? std::string("No ") + (panel.selected_source == ranking_service::source::local ? "local" : "online") + " entries yet."
            : panel.listing.message;
        ui::draw_text_in_rect(message.c_str(), 13,
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
            config.list_rect.x + 10.0f,
            base_y + static_cast<float>(i) * kBrowseRankRowStep,
            config.list_rect.width - 20.0f,
            kBrowseRankRowHeight
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
        const bool self_entry = is_self_entry(entry, panel, config);
        const Color row_base = i % 2 == 0 ? t.section : config.button_base;
        ui::draw_rect_f(row, with_alpha(self_entry ? lerp_color(row_base, t.accent, 0.13f) : row_base, row_alpha));
        if (self_entry) {
            ui::draw_rect_f({row.x, row.y + 4.0f, 3.0f, row.height - 8.0f},
                            with_alpha(t.accent, static_cast<unsigned char>(content_alpha * 0.72f)));
            ui::draw_rect_lines(row, 1.0f, with_alpha(t.accent, static_cast<unsigned char>(content_alpha * 0.34f)));
        }
        ui::draw_text_in_rect(TextFormat("%d", entry.placement > 0 ? entry.placement : i + 1), 18,
                              {row.x + 14.0f, row.y + 16.0f, 34.0f, 22.0f},
                              with_alpha(t.text, content_alpha), ui::text_align::left);
        draw_avatar({row.x + 46.0f, row.y + 6.0f, kAvatarSize, kAvatarSize},
                    entry, content_alpha, config.avatar_base_url);
        draw_marquee_text(entry.player_display_name.empty() ? "Unknown Player" : entry.player_display_name.c_str(),
                          {row.x + 100.0f, row.y + 18.0f, 132.0f, 22.0f},
                          17, with_alpha(t.text, content_alpha), GetTime());
        ui::draw_text_in_rect(format_score(entry.score).c_str(), 14,
                              {score_right - score_width, row.y + 18.0f, score_width, 18.0f},
                              with_alpha(t.text_secondary, content_alpha), ui::text_align::right);
        ui::draw_text_in_rect(format_relative_recorded_at(entry.recorded_at).c_str(), 13,
                              {date_right - date_width, row.y + 18.0f, date_width, 18.0f},
                              with_alpha(t.text_muted, content_alpha), ui::text_align::right);
        ui::draw_text_in_rect(TextFormat("%.2f%%", entry.accuracy), 14,
                              {acc_right - acc_width, row.y + 18.0f, acc_width, 18.0f},
                              with_alpha(t.text_secondary, content_alpha), ui::text_align::right);
        ui::draw_text_in_rect(rank_label(entry.clear_rank()), 17,
                              {clear_right - clear_width, row.y + 16.0f, clear_width, 22.0f},
                              with_alpha(rank_color(entry.clear_rank()), content_alpha), ui::text_align::right);
    }
}

}  // namespace title_ranking_view
