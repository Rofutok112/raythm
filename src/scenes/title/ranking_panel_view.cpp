#include "title/ranking_panel_view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "ranking_service.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

constexpr float kRankingRowHeight = 58.0f;

float ease_out_quad(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv;
}

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
    return std::max(0.0f, content_height(listing) - list_rect.height + 8.0f);
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
                              {config.list_rect.x, config.list_rect.y + 40.0f, config.list_rect.width, 28.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        return;
    }

    for (int i = 0; i < static_cast<int>(panel.listing.entries.size()); ++i) {
        const ranking_service::entry& entry = panel.listing.entries[static_cast<size_t>(i)];
        const float row_reveal_t = ease_out_quad(std::clamp((reveal_anim - static_cast<float>(i) * 0.075f) / 0.24f, 0.0f, 1.0f));
        if (row_reveal_t <= 0.0f) {
            continue;
        }

        const Rectangle base_row = {
            config.list_rect.x,
            base_y + static_cast<float>(i) * kRankingRowHeight,
            config.list_rect.width,
            kRankingRowHeight - 4.0f
        };
        const Rectangle row = {
            base_row.x + (1.0f - row_reveal_t) * 44.0f,
            base_row.y,
            base_row.width,
            base_row.height
        };
        if (row.y + row.height < config.list_rect.y - 4.0f || row.y > config.list_rect.y + config.list_rect.height + 4.0f) {
            continue;
        }

        const unsigned char row_alpha = static_cast<unsigned char>(config.normal_row_alpha * row_reveal_t);
        const unsigned char content_alpha = static_cast<unsigned char>(config.alpha * row_reveal_t);
        DrawRectangleRec(row, with_alpha(config.button_base, row_alpha));
        DrawRectangleLinesEx(row, 1.0f, with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t * row_reveal_t)));

        const Rectangle placement_rect = {row.x + 4.0f, row.y + 7.0f, 46.0f, row.height - 14.0f};
        const Rectangle rank_rect = {row.x + row.width - 34.0f, row.y + 7.0f, 24.0f, row.height - 14.0f};
        const Rectangle score_rect = {row.x + row.width - 148.0f, row.y + 17.0f, 104.0f, 24.0f};
        const Rectangle middle_rect = {
            row.x + 56.0f,
            row.y + 6.0f,
            score_rect.x - (row.x + 56.0f) - 8.0f,
            row.height - 12.0f
        };
        const Rectangle time_rect = {score_rect.x - 22.0f, middle_rect.y + 1.0f, 16.0f, 14.0f};
        const Rectangle name_rect = {middle_rect.x, middle_rect.y, time_rect.x - middle_rect.x - 6.0f, 18.0f};

        ui::draw_text_in_rect(TextFormat("#%d", i + 1), 18, placement_rect,
                              with_alpha(t.text_secondary, content_alpha), ui::text_align::center);
        draw_marquee_text(entry.player_display_name.empty() ? "Unknown Player" : entry.player_display_name.c_str(),
                          name_rect, 15, with_alpha(t.text, content_alpha), GetTime());
        ui::draw_text_in_rect(format_relative_recorded_at(entry.recorded_at).c_str(), 12,
                              time_rect,
                              with_alpha(t.text_muted, content_alpha), ui::text_align::right);
        ui::draw_text_in_rect("Max Combo", 11,
                              {middle_rect.x, middle_rect.y + 20.0f, 78.0f, 12.0f},
                              with_alpha(t.text_muted, content_alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%dx", entry.max_combo), 13,
                              {middle_rect.x, middle_rect.y + 31.0f, 78.0f, 14.0f},
                              with_alpha(entry.is_full_combo ? t.success : t.text, content_alpha), ui::text_align::left);
        ui::draw_text_in_rect("Accuracy", 11,
                              {middle_rect.x + 94.0f, middle_rect.y + 20.0f, middle_rect.width - 94.0f, 12.0f},
                              with_alpha(t.text_muted, content_alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%.2f%%", entry.accuracy), 13,
                              {middle_rect.x + 94.0f, middle_rect.y + 31.0f, middle_rect.width - 94.0f, 14.0f},
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
