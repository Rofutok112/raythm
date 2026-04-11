#include "song_select/song_select_ranking_view.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui/ui_font.h"

namespace {

constexpr const char* kRankingSourceOptions[] = {
    "LOCAL",
    "ONLINE",
};

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
    switch (value) {
        case rank::ss: return g_theme->rank_ss;
        case rank::s: return g_theme->rank_s;
        case rank::aa: return g_theme->rank_aa;
        case rank::a: return g_theme->rank_a;
        case rank::b: return g_theme->rank_b;
        case rank::c: return g_theme->rank_c;
        case rank::f: return g_theme->rank_f;
    }
    return g_theme->text_secondary;
}

const char* source_label(ranking_service::source source) {
    return source == ranking_service::source::local ? "LOCAL" : "ONLINE";
}

int source_index(ranking_service::source source) {
    return source == ranking_service::source::local ? 0 : 1;
}

void draw_static_source_dropdown(ranking_service::source source) {
    const auto& theme = *g_theme;
    const ui::row_state row = ui::draw_row(song_select::layout::kRankingSourceDropdownRect,
                                           theme.row, theme.row_hover, theme.border);
    const Rectangle content = ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 12.0f));
    const Rectangle arrow_rect = ui::place(content, 18.0f, content.height,
                                           ui::anchor::center_right, ui::anchor::center_right);
    const Rectangle value_rect = {
        content.x,
        content.y,
        std::max(0.0f, arrow_rect.x - content.x - 8.0f),
        content.height
    };
    ui::draw_text_in_rect(source_label(source), 18, value_rect, theme.text_dim, ui::text_align::right);
    ui::draw_text_in_rect("v", 18, arrow_rect, theme.text_dim);
}

std::optional<std::chrono::system_clock::time_point> parse_recorded_at_utc(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    std::tm utc_tm{};
    std::istringstream input(value);
    input >> std::get_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    if (input.fail()) {
        return std::nullopt;
    }

#ifdef _WIN32
    const std::time_t raw_time = _mkgmtime(&utc_tm);
#else
    const std::time_t raw_time = timegm(&utc_tm);
#endif
    if (raw_time == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(raw_time);
}

std::string format_relative_recorded_at(const std::string& recorded_at) {
    const auto parsed_time = parse_recorded_at_utc(recorded_at);
    if (!parsed_time.has_value()) {
        return "-";
    }

    const auto now = std::chrono::system_clock::now();
    auto delta_sec = std::chrono::duration_cast<std::chrono::seconds>(now - *parsed_time).count();
    if (delta_sec < 0) {
        delta_sec = 0;
    }

    if (delta_sec < 60) {
        return std::to_string(delta_sec) + "s ago";
    }

    auto delta = delta_sec / 60;

    if (delta < 60) {
        return std::to_string(delta) + "m ago";
    }

    const long long hours = delta / 60;
    if (hours < 24) {
        return std::to_string(hours) + "h ago";
    }

    const long long days = hours / 24;
    if (days < 30) {
        return std::to_string(days) + "d ago";
    }

    const long long months = days / 30;
    if (months < 12) {
        return std::to_string(months) + "mo ago";
    }

    return std::to_string(months / 12) + "y ago";
}

std::string format_score(int value) {
    std::string digits = std::to_string(std::max(0, value));
    for (int insert_at = static_cast<int>(digits.size()) - 3; insert_at > 0; insert_at -= 3) {
        digits.insert(static_cast<size_t>(insert_at), ",");
    }
    return digits;
}

void draw_score_text(const std::string& text, Rectangle rect, Color color) {
    constexpr float font_size = 19.0f;
    constexpr float spacing = 5.0f;
    const Vector2 size = ui::measure_text_size(text.c_str(), font_size, spacing);
    const Vector2 pos = {
        rect.x + rect.width - size.x,
        rect.y + (rect.height - size.y) * 0.5f
    };
    ui::draw_text_auto(text.c_str(), pos, font_size, spacing, color);
}

void draw_ranking_row(const ranking_service::entry& entry, float y, float offset_x, unsigned char alpha) {
    const auto& theme = *g_theme;
    const Rectangle row_rect = {
        song_select::layout::kRankingPanelRect.x + 16.0f + offset_x,
        y + 3.0f,
        song_select::layout::kRankingPanelRect.width - 40.0f,
        song_select::layout::kRankingRowHeight - 8.0f
    };
    const ui::row_state row_state = ui::draw_row(row_rect, with_alpha(theme.row, alpha),
                                                 with_alpha(theme.row_hover, alpha),
                                                 with_alpha(theme.border, alpha), 1.0f);
    const Rectangle content = ui::inset(row_state.visual, ui::edge_insets::symmetric(4.0f, 10.0f));

    const Rectangle placement_rect = {content.x, content.y, 36.0f, content.height};
    const Rectangle rank_rect = {content.x + 52.0f, content.y, 48.0f, content.height};
    const Rectangle accuracy_rect = {content.x + 124.0f, content.y, 108.0f, content.height};
    const Rectangle combo_rect = {content.x + 254.0f, content.y, 104.0f, content.height};
    const Rectangle recorded_at_rect = {content.x + 378.0f, content.y, 90.0f, content.height};
    const Rectangle score_rect = {content.x + 476.0f, content.y, content.width - 476.0f, content.height};

    DrawRectangleRec(rank_rect, with_alpha(theme.section, alpha));
    DrawRectangleLinesEx(rank_rect, 1.5f, with_alpha(theme.border_light, alpha));

    ui::draw_text_in_rect(TextFormat("%02d", entry.placement), 18, placement_rect, with_alpha(theme.text, alpha), ui::text_align::center);
    ui::draw_text_in_rect(rank_label(entry.clear_rank()), 17, rank_rect, with_alpha(rank_color(entry.clear_rank()), alpha), ui::text_align::center);
    ui::draw_text_in_rect(TextFormat("%.2f%%", entry.accuracy), 17, accuracy_rect, with_alpha(theme.text_secondary, alpha), ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%d Combo", entry.max_combo), 14, combo_rect, with_alpha(theme.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect(format_relative_recorded_at(entry.recorded_at).c_str(), 14, recorded_at_rect,
                          with_alpha(theme.text_muted, alpha), ui::text_align::left);
    const std::string score_label = format_score(entry.score);
    draw_score_text(score_label, score_rect, with_alpha(theme.text, alpha));
}

}  // namespace

namespace song_select {

float ranking_content_height(const state& state) {
    const size_t entry_count = state.ranking_panel.listing.entries.empty()
        ? 1
        : state.ranking_panel.listing.entries.size();
    return static_cast<float>(entry_count) * layout::kRankingRowHeight + 8.0f;
}

ranking_panel_result draw_ranking_panel(const state& state, bool source_dropdown_interactive) {
    const auto& theme = *g_theme;
    ranking_panel_result result;
    const float chart_anim = 1.0f - state.chart_change_anim_t;
    const float content_offset_x = 14.0f * state.chart_change_anim_t;
    const unsigned char content_alpha = static_cast<unsigned char>(120.0f + 135.0f * chart_anim);

    ui::draw_section(layout::kRankingPanelRect);
    ui::draw_text_in_rect("RANKING", 24, layout::kRankingTitleRect, theme.text, ui::text_align::left);

    if (source_dropdown_interactive) {
        const ui::dropdown_state source_dropdown = ui::enqueue_dropdown(
            layout::kRankingSourceDropdownRect,
            layout::ranking_source_dropdown_menu_rect(),
            "",
            source_label(state.ranking_panel.selected_source),
            kRankingSourceOptions,
            source_index(state.ranking_panel.selected_source),
            state.ranking_panel.source_dropdown_open,
            ui::draw_layer::base,
            ui::draw_layer::overlay,
            18,
            0.0f);
        result.source_dropdown_toggled = source_dropdown.trigger.clicked;
        result.source_clicked_index = source_dropdown.clicked_index;
        result.source_dropdown_close_requested =
            state.ranking_panel.source_dropdown_open &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            !ui::is_hovered(layout::kRankingSourceDropdownRect, ui::draw_layer::base) &&
            !ui::is_hovered(layout::ranking_source_dropdown_menu_rect(), ui::draw_layer::overlay);
    } else {
        draw_static_source_dropdown(state.ranking_panel.selected_source);
    }

    ui::scoped_clip_rect clip_scope(layout::kRankingListRect);
    const float base_y = layout::kRankingListRect.y - state.ranking_panel.scroll_y;

    if (!state.ranking_panel.listing.available || state.ranking_panel.listing.entries.empty()) {
        const std::string message = state.ranking_panel.listing.message.empty()
            ? std::string("No ") + source_label(state.ranking_panel.selected_source) + " ranking entries."
            : state.ranking_panel.listing.message;
        ui::draw_text_in_rect(message.c_str(), 20,
                              {layout::kRankingListRect.x + 16.0f + content_offset_x, base_y + 22.0f,
                               layout::kRankingListRect.width - 40.0f, 40.0f},
                              with_alpha(theme.text_muted, content_alpha), ui::text_align::left);
    } else {
        float row_y = base_y;
        for (const ranking_service::entry& entry : state.ranking_panel.listing.entries) {
            if (row_y + layout::kRankingRowHeight < layout::kRankingListRect.y) {
                row_y += layout::kRankingRowHeight;
                continue;
            }
            if (row_y > layout::kRankingListRect.y + layout::kRankingListRect.height) {
                break;
            }
            draw_ranking_row(entry, row_y, content_offset_x, content_alpha);
            row_y += layout::kRankingRowHeight;
        }
    }

    ui::draw_scrollbar(layout::kRankingScrollbarTrackRect,
                       ranking_content_height(state),
                       state.ranking_panel.scroll_y,
                       theme.scrollbar_track,
                       theme.scrollbar_thumb);

    return result;
}

}  // namespace song_select
