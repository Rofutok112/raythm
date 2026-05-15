#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "localization/localization.h"
#include "platform/windows_input_source.h"
#include "scene_common.h"
#include "tween.h"
#include "title/title_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace title_online_view {
namespace {

constexpr float kChartLevelWidth = 264.0f;
constexpr float kChartFilterGroupGap = 22.0f;
constexpr float kChartKeyGroupX = kChartLevelWidth + kChartFilterGroupGap;
constexpr float kChartKeyButtonWidth = 44.0f;
constexpr float kChartKeyButtonStep = 52.0f;
constexpr float kChartKeyGroupWidth = kChartKeyButtonWidth + kChartKeyButtonStep * 4.0f;
constexpr float kChartClearButtonX = kChartKeyGroupX + kChartKeyGroupWidth + kChartFilterGroupGap;
constexpr float kChartClearButtonWidth = 68.0f;

Rectangle song_card_jacket_rect(Rectangle card) {
    const float jacket_size = std::min(card.height - 42.0f, 96.0f);
    return {
        card.x + 16.0f,
        card.y + 16.0f,
        jacket_size,
        jacket_size,
    };
}

int selected_song_display_index(const state& state) {
    const auto indices = detail::filtered_indices(state);
    const int selected_index = detail::selected_song_index_ref(state);
    const auto it = std::find(indices.begin(), indices.end(), selected_index);
    if (it == indices.end()) {
        return -1;
    }
    return static_cast<int>(it - indices.begin());
}

std::string join_labels(const std::vector<std::string>& values, const char* separator) {
    std::string result;
    for (const std::string& value : values) {
        if (value.empty()) {
            continue;
        }
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

std::string genre_summary(const song_meta& meta) {
    const std::string joined = join_labels(meta.genres, " / ");
    if (!joined.empty()) {
        return joined;
    }
    return meta.genre;
}

std::vector<std::string> genre_labels(const song_meta& meta) {
    if (!meta.genres.empty()) {
        return meta.genres;
    }
    if (!meta.genre.empty()) {
        return {meta.genre};
    }
    return {};
}

std::string primary_genre_label(const song_meta& meta) {
    if (!meta.genres.empty()) {
        return meta.genres.front();
    }
    return meta.genre;
}

std::string shelf_display_title(const std::string& key) {
    if (key == "rising") {
        return "RISING THIS WEEK";
    }
    if (key == "hidden_gems") {
        return "HIDDEN GEMS";
    }
    if (key == "fresh_charts") {
        return "FRESH CHARTS";
    }
    if (key == "new") {
        return "NEW ARRIVALS";
    }
    if (key == "needs_charts") {
        return "NEEDS CHARTS";
    }
    if (key == "recommended") {
        return "RECOMMENDED";
    }
    return "DISCOVERY";
}

Color genre_color_for_label(const std::string& label) {
    static constexpr Color kPalette[] = {
        {147, 94, 226, 255},   // violet
        {38, 167, 216, 255},   // cyan
        {214, 143, 43, 255},   // amber
        {132, 204, 45, 255},   // lime
        {216, 78, 133, 255},   // rose
        {62, 126, 220, 255},   // blue
        {39, 181, 154, 255},   // mint
        {218, 91, 61, 255},    // coral
        {190, 181, 48, 255},   // yellow
        {162, 103, 231, 255},  // purple
        {65, 190, 96, 255},    // green
        {212, 94, 172, 255},   // pink
    };
    unsigned int hash = 2166136261u;
    for (unsigned char ch : label) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<unsigned char>(ch - 'A' + 'a');
        }
        hash ^= ch;
        hash *= 16777619u;
    }
    return kPalette[hash % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

Color keyword_color_for_label(const std::string& label) {
    return genre_color_for_label("keyword:" + label);
}

std::string song_subtitle(const song_meta& meta) {
    return meta.artist;
}

const char* format_count_label(const char* label, int count) {
    if (count >= 1000000) {
        return TextFormat("%s %.1fm", label, static_cast<float>(count) / 1000000.0f);
    }
    if (count >= 1000) {
        return TextFormat("%s %.1fk", label, static_cast<float>(count) / 1000.0f);
    }
    return TextFormat("%s %d", label, count);
}

bool view_matches_shelf(discovery_view view, const std::string& key) {
    switch (view) {
    case discovery_view::overview:
        return false;
    case discovery_view::new_arrivals:
        return key == "new";
    case discovery_view::rising:
        return key == "rising";
    case discovery_view::hidden_gems:
        return key == "hidden_gems";
    case discovery_view::recommended:
        return key == "recommended";
    case discovery_view::needs_charts:
        return key == "needs_charts";
    }
    return false;
}

const char* view_label(discovery_view view) {
    switch (view) {
    case discovery_view::overview:
        return "Overview";
    case discovery_view::new_arrivals:
        return "New";
    case discovery_view::rising:
        return "Rising";
    case discovery_view::hidden_gems:
        return "Hidden gems";
    case discovery_view::recommended:
        return "Recommended";
    case discovery_view::needs_charts:
        return "Needs charts";
    }
    return "";
}

const char* source_label(source_filter source) {
    switch (source) {
    case source_filter::all:
        return "All";
    case source_filter::official:
        return "Official";
    case source_filter::community:
        return "Community";
    }
    return "";
}

Rectangle preview_open_button_rect(Rectangle panel) {
    constexpr float kPreviewPanelInset = 24.0f;
    constexpr float kPreviewOpenButtonBottom = 28.0f;
    constexpr float kPreviewOpenButtonHeight = 58.0f;
    return {
        panel.x + kPreviewPanelInset,
        panel.y + panel.height - kPreviewOpenButtonBottom - kPreviewOpenButtonHeight,
        panel.width - kPreviewPanelInset * 2.0f,
        kPreviewOpenButtonHeight,
    };
}

Rectangle preview_play_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewPlayWidth = 116.0f;
    constexpr float kPreviewPlayHeight = 54.0f;
    return {
        panel.x + panel.width * 0.5f - kPreviewPlayWidth * 0.5f,
        panel.y + kPreviewPlayY,
        kPreviewPlayWidth,
        kPreviewPlayHeight,
    };
}

Rectangle preview_prev_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewButtonWidth = 90.0f;
    constexpr float kPreviewButtonHeight = 54.0f;
    constexpr float kPreviewButtonGap = 8.0f;
    return {
        panel.x + panel.width * 0.5f - 58.0f - kPreviewButtonGap - kPreviewButtonWidth,
        panel.y + kPreviewPlayY,
        kPreviewButtonWidth,
        kPreviewButtonHeight,
    };
}

Rectangle preview_next_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewButtonWidth = 90.0f;
    constexpr float kPreviewButtonHeight = 54.0f;
    constexpr float kPreviewButtonGap = 8.0f;
    return {
        panel.x + panel.width * 0.5f + 58.0f + kPreviewButtonGap,
        panel.y + kPreviewPlayY,
        kPreviewButtonWidth,
        kPreviewButtonHeight,
    };
}

Rectangle preview_progress_rect(Rectangle panel) {
    constexpr float kPreviewPanelInset = 24.0f;
    constexpr float kPreviewBarY = 468.0f;
    constexpr float kPreviewBarHeight = 12.0f;
    return {
        panel.x + kPreviewPanelInset,
        panel.y + kPreviewBarY,
        panel.width - kPreviewPanelInset * 2.0f,
        kPreviewBarHeight,
    };
}

Rectangle chart_filter_button_rect(Rectangle chart_list, int index) {
    return {
        chart_list.x + static_cast<float>(index) * 74.0f,
        chart_list.y - 78.0f,
        66.0f,
        28.0f,
    };
}

Rectangle chart_source_button_rect(Rectangle chart_list, int index) {
    return {
        chart_list.x + static_cast<float>(index) * ((chart_list.width - 18.0f) / 4.0f + 6.0f),
        chart_list.y - 242.0f,
        (chart_list.width - 18.0f) / 4.0f,
        46.0f,
    };
}

Rectangle chart_key_button_rect(Rectangle chart_list, int index) {
    return {
        chart_list.x + kChartKeyGroupX + static_cast<float>(index) * kChartKeyButtonStep,
        chart_list.y - 98.0f,
        kChartKeyButtonWidth,
        28.0f,
    };
}

Rectangle chart_status_button_rect(Rectangle chart_list, int index) {
    return {
        chart_list.x + static_cast<float>(index) * ((chart_list.width - 12.0f) / 3.0f + 6.0f),
        chart_list.y - 184.0f,
        (chart_list.width - 12.0f) / 3.0f,
        42.0f,
    };
}

Rectangle chart_clear_button_rect(Rectangle chart_list) {
    return {
        chart_list.x + kChartClearButtonX,
        chart_list.y - 98.0f,
        kChartClearButtonWidth,
        28.0f,
    };
}

Rectangle chart_level_min_input_rect(Rectangle chart_list) {
    return {chart_list.x, chart_list.y - 98.0f, 66.0f, 28.0f};
}

Rectangle chart_level_max_input_rect(Rectangle chart_list) {
    return {chart_list.x + 104.0f, chart_list.y - 98.0f, 66.0f, 28.0f};
}

Rectangle chart_level_slider_rect(Rectangle chart_list) {
    return {chart_list.x, chart_list.y - 41.0f, kChartLevelWidth, 8.0f};
}

bool chart_filters_active(const state& state) {
    return !state.chart_search_input.value.empty() ||
        !state.min_level_input.value.empty() ||
        !state.max_level_input.value.empty() ||
        state.chart_source != chart_source_filter::all ||
        state.chart_key_filter != 0 ||
        state.chart_download_filter != 0;
}

const char* chart_source_label(content_status status) {
    switch (status) {
    case content_status::official:
        return "Official";
    case content_status::community:
        return "Community";
    default:
        return "Local";
    }
}

Color chart_source_status_color(const chart_entry_state& chart) {
    if (chart.installed || chart.update_available) {
        return g_theme->success;
    }
    return chart.chart.source_status == content_status::official ? g_theme->success : g_theme->slow;
}

float chart_level_value(const std::string& value, float fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed == value.size()) {
            return std::clamp(result, 1.0f, 10.0f);
        }
    } catch (...) {
    }
    return fallback;
}

void draw_level_range_slider(Rectangle chart_list, const state& state, unsigned char alpha) {
    float min_level = chart_level_value(state.min_level_input.value, 1.0f);
    float max_level = chart_level_value(state.max_level_input.value, 10.0f);
    if (min_level > max_level) {
        std::swap(min_level, max_level);
    }
    const Rectangle track = chart_level_slider_rect(chart_list);
    const float min_t = (min_level - 1.0f) / 9.0f;
    const float max_t = (max_level - 1.0f) / 9.0f;
    const float min_x = track.x + track.width * min_t;
    const float max_x = track.x + track.width * max_t;
    ui::draw_rect_f(track, with_alpha(g_theme->slider_track, alpha));
    ui::draw_rect_f({min_x, track.y, std::max(0.0f, max_x - min_x), track.height},
                    with_alpha(g_theme->accent, alpha));
    ui::draw_rect_f({min_x - 5.0f, track.y - 7.0f, 10.0f, 22.0f}, with_alpha(g_theme->accent, alpha));
    ui::draw_rect_f({max_x - 5.0f, track.y - 7.0f, 10.0f, 22.0f}, with_alpha(g_theme->accent, alpha));
}

void draw_browse_body_text_in_rect(const char* text,
                            int font_size,
                            Rectangle rect,
                            Color color,
                            ui::text_align align = ui::text_align::center) {
    ui::draw_body_text_in_rect(text, font_size, rect, color, align);
}

void draw_browse_body_text_f(const char* text, float x, float y, int font_size, Color color) {
    ui::draw_text_body(text, {x, y}, static_cast<float>(font_size), 0.0f, color);
}

void draw_body_marquee_text(const char* text,
                            Rectangle clip_rect,
                            int font_size,
                            Color color,
                            double time,
                            ui::text_align align = ui::text_align::left) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const float text_width = ui::measure_body_text_size(text, static_cast<float>(font_size), 0.0f).x;
    const float draw_y = clip_rect.y + (clip_rect.height - ui::text_layout_font_size(static_cast<float>(font_size))) * 0.5f;
    if (text_width <= clip_rect.width) {
        float draw_x = clip_rect.x;
        if (align == ui::text_align::center) {
            draw_x = clip_rect.x + (clip_rect.width - text_width) * 0.5f;
        } else if (align == ui::text_align::right) {
            draw_x = clip_rect.x + clip_rect.width - text_width;
        }
        ui::scoped_clip_rect clip_scope(clip_rect);
        draw_browse_body_text_f(text, draw_x, draw_y, font_size, color);
        return;
    }

    constexpr float kScrollSpeed = 42.0f;
    constexpr float kPauseSeconds = 1.0f;
    const float overflow = text_width - clip_rect.width;
    const float travel_time = overflow / kScrollSpeed;
    const float cycle = travel_time + kPauseSeconds * 2.0f;
    const float cycle_t = static_cast<float>(std::fmod(time, static_cast<double>(std::max(cycle, 0.001f))));
    float offset = 0.0f;
    if (cycle_t > kPauseSeconds && cycle_t < kPauseSeconds + travel_time) {
        offset = (cycle_t - kPauseSeconds) * kScrollSpeed;
    } else if (cycle_t >= kPauseSeconds + travel_time) {
        offset = overflow;
    }
    ui::scoped_clip_rect clip_scope(clip_rect);
    draw_browse_body_text_f(text, clip_rect.x - offset, draw_y, font_size, color);
}

void draw_body_marquee_text(const char* text,
                            float x,
                            float y,
                            int font_size,
                            Color color,
                            float max_width,
                            double time) {
    draw_body_marquee_text(text,
                           {x, y, max_width, ui::text_layout_font_size(static_cast<float>(font_size))},
                           font_size,
                           color,
                           time,
                           ui::text_align::left);
}

ui::text_input_result draw_song_search_input(Rectangle rect, ui::text_input_state& state,
                                             const char* label, const char* placeholder,
                                             int font_size, size_t max_length,
                                             Color button_base, Color button_hover, Color button_selected,
                                             unsigned char normal_row_alpha,
                                             unsigned char hover_row_alpha,
                                             unsigned char selected_row_alpha,
                                             unsigned char alpha) {
    ui::text_input_result result;
    ui::clamp_text_input_state(state);
    const auto& t = *g_theme;

    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const bool clicked = ui::is_clicked(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    const unsigned char row_alpha = state.active ? selected_row_alpha
        : hovered ? hover_row_alpha
                  : normal_row_alpha;
    ui::draw_rect_f(visual, with_alpha(state.active ? button_selected : button_base, row_alpha));
    const Rectangle border_rect = ui::inset(visual, 1.0f);
    ui::draw_rect_lines(border_rect, 1.2f,
                        with_alpha(state.active ? t.border_active : t.border_light, alpha));

    const Rectangle content_rect = ui::inset(visual, ui::edge_insets::symmetric(0.0f, 14.0f));
    constexpr float kLabelWidth = 108.0f;
    constexpr float kLabelGap = 14.0f;
    const bool show_label = label != nullptr && *label != '\0' && !state.active && state.value.empty();
    const Rectangle label_rect = {content_rect.x, content_rect.y, kLabelWidth, content_rect.height};
    const Rectangle text_rect = {
        show_label ? content_rect.x + kLabelWidth + kLabelGap : content_rect.x,
        content_rect.y,
        show_label ? std::max(0.0f, content_rect.width - kLabelWidth - kLabelGap) : content_rect.width,
        content_rect.height,
    };

    if (clicked) {
        result.clicked = true;
        if (!state.active) {
            result.activated = true;
        }
        state.active = true;

        if (CheckCollisionPointRec(GetMousePosition(), text_rect)) {
            const float local_x = GetMousePosition().x - text_rect.x + state.scroll_x;
            state.cursor = ui::text_input_cursor_from_mouse(state.value, local_x, font_size);
            ui::clear_text_input_selection(state);
            state.mouse_selecting = true;
        } else {
            state.cursor = ui::utf8_codepoint_count(state.value);
            ui::clear_text_input_selection(state);
        }
    } else if (state.active && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered) {
        state.active = false;
        state.mouse_selecting = false;
        ui::clear_text_input_selection(state);
        result.deactivated = true;
    }

    if (state.active && state.mouse_selecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        const float local_x = mouse.x - text_rect.x + state.scroll_x;
        const size_t mouse_cursor = ui::text_input_cursor_from_mouse(state.value, local_x, font_size);
        state.cursor = mouse_cursor;
        state.has_selection = state.cursor != state.selection_anchor;
    }

    if (state.mouse_selecting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state.mouse_selecting = false;
    }

    if (state.active) {
        windows_input_source::instance().request_text_input();

        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        if (ctrl && IsKeyPressed(KEY_A)) {
            state.selection_anchor = 0;
            state.cursor = ui::utf8_codepoint_count(state.value);
            state.has_selection = state.cursor > 0;
        }

        if (ctrl && IsKeyPressed(KEY_C) && state.has_selection) {
            SetClipboardText(ui::selected_text_input_text(state).c_str());
        }

        if (ctrl && IsKeyPressed(KEY_X) && state.has_selection) {
            SetClipboardText(ui::selected_text_input_text(state).c_str());
            result.changed = ui::delete_text_input_selection(state) || result.changed;
        }

        if (ctrl && IsKeyPressed(KEY_V)) {
            const char* clipboard = GetClipboardText();
            if (clipboard != nullptr) {
                result.changed =
                    ui::paste_text_input_at_cursor(state, clipboard, max_length, ui::default_text_input_filter) ||
                    result.changed;
            }
        }

        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (state.has_selection) {
                result.changed = ui::delete_text_input_selection(state) || result.changed;
            }
            if (ui::utf8_codepoint_count(state.value) < max_length &&
                ui::default_text_input_filter(codepoint, state.value)) {
                result.changed = ui::insert_codepoint_at_cursor(state, codepoint) || result.changed;
            }
            codepoint = GetCharPressed();
        }

        if (ui::text_input_key_action(KEY_BACKSPACE)) {
            if (state.has_selection) {
                result.changed = ui::delete_text_input_selection(state) || result.changed;
            } else if (state.cursor > 0) {
                const size_t end_byte = ui::utf8_codepoint_to_byte_index(state.value, state.cursor);
                const size_t start_byte = ui::utf8_codepoint_to_byte_index(state.value, state.cursor - 1);
                state.value.erase(start_byte, end_byte - start_byte);
                --state.cursor;
                ui::clear_text_input_selection(state);
                result.changed = true;
            }
        }

        if (ui::text_input_key_action(KEY_DELETE)) {
            if (state.has_selection) {
                result.changed = ui::delete_text_input_selection(state) || result.changed;
            } else if (state.cursor < ui::utf8_codepoint_count(state.value)) {
                const size_t start_byte = ui::utf8_codepoint_to_byte_index(state.value, state.cursor);
                const size_t end_byte = ui::utf8_codepoint_to_byte_index(state.value, state.cursor + 1);
                state.value.erase(start_byte, end_byte - start_byte);
                result.changed = true;
            }
        }

        if (ui::text_input_key_action(KEY_LEFT)) {
            if (state.has_selection && !shift) {
                ui::move_text_input_cursor(state, ui::text_input_selection_range(state).first, false);
            } else if (state.cursor > 0) {
                ui::move_text_input_cursor(state, state.cursor - 1, shift);
            }
        }

        if (ui::text_input_key_action(KEY_RIGHT)) {
            if (state.has_selection && !shift) {
                ui::move_text_input_cursor(state, ui::text_input_selection_range(state).second, false);
            } else if (state.cursor < ui::utf8_codepoint_count(state.value)) {
                ui::move_text_input_cursor(state, state.cursor + 1, shift);
            }
        }

        if (ui::text_input_key_action(KEY_HOME)) {
            ui::move_text_input_cursor(state, 0, shift);
        }

        if (ui::text_input_key_action(KEY_END)) {
            ui::move_text_input_cursor(state, ui::utf8_codepoint_count(state.value), shift);
        }

        if (IsKeyPressed(KEY_ENTER)) {
            result.submitted = true;
            state.active = false;
            state.mouse_selecting = false;
            ui::clear_text_input_selection(state);
            result.deactivated = true;
        }
    }

    ui::update_text_input_scroll(state, text_rect.width - 8.0f, font_size);

    if (show_label) {
        draw_browse_body_text_in_rect(label, font_size, label_rect,
                              with_alpha(t.text_secondary, alpha), ui::text_align::left);
    }

    std::string display_value = state.value;
    if (display_value.empty() && !state.active && placeholder != nullptr) {
        display_value = placeholder;
    }

    const Color text_color = with_alpha(state.value.empty() && !state.active ? t.text_hint : t.text, alpha);
    const float layout_font_size = ui::text_layout_font_size(static_cast<float>(font_size));
    const float text_y = text_rect.y + (text_rect.height - layout_font_size) * 0.5f + 2.0f;
    const float selection_y = text_rect.y + 7.0f;
    const float selection_height = text_rect.height - 14.0f;
    const float cursor_y = text_rect.y + 8.0f;
    const float cursor_height = text_rect.height - 16.0f;

    if (!state.active && !state.value.empty()) {
        draw_body_marquee_text(display_value.c_str(), text_rect.x, text_y, font_size, text_color,
                          text_rect.width, GetTime());
    } else if (!state.active) {
        draw_browse_body_text_f(display_value.c_str(), text_rect.x, text_y, font_size, text_color);
    } else {
        ui::scoped_clip_rect clip_scope(text_rect);

        if (state.has_selection) {
            const auto [selection_start, selection_end] = ui::text_input_selection_range(state);
            const float selection_x = text_rect.x +
                                      ui::text_input_prefix_width(state.value, selection_start, font_size) -
                                      state.scroll_x;
            const float selection_end_x = text_rect.x +
                                          ui::text_input_prefix_width(state.value, selection_end, font_size) -
                                          state.scroll_x;
            ui::draw_rect_span({selection_x, selection_y,
                                selection_end_x - selection_x, selection_height},
                               with_alpha(t.row_selected, alpha));
        }

        draw_browse_body_text_f(state.value.c_str(), text_rect.x - state.scroll_x, text_y, font_size, with_alpha(t.text, alpha));

        const double blink = GetTime() * 1.6;
        if (std::fmod(blink, 1.0) < 0.6) {
            const float cursor_x = text_rect.x +
                                   ui::text_input_prefix_width(state.value, state.cursor, font_size) -
                                   state.scroll_x;
            ui::draw_rect_span({cursor_x, cursor_y, 1.5f, cursor_height},
                               with_alpha(t.text, alpha));
        }

    }

    return result;
}

void draw_transport_toggle_button(Rectangle rect, bool playing, unsigned char alpha) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    const Color border = with_alpha(playing ? t.accent : t.border_light, alpha);
    const Color fill = with_alpha(playing ? lerp_color(t.section, t.accent, 0.34f) : t.section,
                                  static_cast<unsigned char>(hovered ? alpha : alpha * 0.72f));
    ui::draw_rect_f(visual, fill);
    ui::draw_rect_lines(visual, 1.3f, border);
    const Color icon = with_alpha(playing ? t.text : (hovered ? t.text : t.text_secondary), alpha);
    if (playing) {
        const float bar_width = 5.0f;
        const float bar_height = 18.0f;
        const float gap = 7.0f;
        const float total_width = bar_width * 2.0f + gap;
        const float x = visual.x + (visual.width - total_width) * 0.5f;
        const float y = visual.y + (visual.height - bar_height) * 0.5f;
        ui::draw_rect_f({x, y, bar_width, bar_height}, icon);
        ui::draw_rect_f({x + bar_width + gap, y, bar_width, bar_height}, icon);
    } else {
        const float tri_width = 18.0f;
        const float tri_height = 20.0f;
        const float x = visual.x + (visual.width - tri_width) * 0.5f + 2.0f;
        const float y = visual.y + (visual.height - tri_height) * 0.5f;
        DrawTriangle({x, y},
                     {x, y + tri_height},
                     {x + tri_width, y + tri_height * 0.5f},
                     icon);
    }
}

void draw_transport_skip_button(Rectangle rect, bool next, unsigned char alpha) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::draw_rect_f(visual, with_alpha(t.section, static_cast<unsigned char>(hovered ? alpha : alpha * 0.64f)));
    ui::draw_rect_lines(visual, 1.2f, with_alpha(t.border_light, alpha));

    const Color icon = with_alpha(hovered ? t.text : t.text_secondary, alpha);
    const float cx = visual.x + visual.width * 0.5f;
    const float cy = visual.y + visual.height * 0.5f;
    const float tri_w = 18.0f;
    const float tri_h = 22.0f;
    const float bar_h = 23.0f;
    const float bar_w = 3.5f;
    const float gap = 5.0f;
    if (next) {
        const float right = cx + 12.0f;
        const float left = right - tri_w;
        DrawTriangle({left, cy - tri_h * 0.5f},
                     {left, cy + tri_h * 0.5f},
                     {right, cy},
                     icon);
        ui::draw_rect_f({right + gap, cy - bar_h * 0.5f, bar_w, bar_h}, icon);
    } else {
        const float left = cx - 12.0f;
        const float right = left + tri_w;
        DrawTriangle({right, cy - tri_h * 0.5f},
                     {left, cy},
                     {right, cy + tri_h * 0.5f},
                     icon);
        ui::draw_rect_f({left - gap - bar_w, cy - bar_h * 0.5f, bar_w, bar_h}, icon);
    }
}

void draw_download_icon_button(Rectangle rect, bool update, unsigned char alpha) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.2f) : rect;
    const Color tone = update ? t.accent : t.success;
    const Color fill = with_alpha(lerp_color(t.section, tone, hovered ? 0.18f : 0.10f),
                                  static_cast<unsigned char>(hovered ? alpha : alpha * 0.82f));
    const Color stroke = with_alpha(tone, alpha);
    ui::draw_rect_f(visual, fill);
    ui::draw_rect_lines(visual, 1.2f, stroke);

    const float cx = visual.x + visual.width * 0.5f;
    const float top = visual.y + 7.0f;
    const float tip_y = visual.y + visual.height - 7.0f;
    const float wing_y = tip_y - 7.0f;
    DrawLineEx({cx, top}, {cx, tip_y}, 2.4f, stroke);
    DrawLineEx({cx - 6.0f, wing_y}, {cx, tip_y}, 2.4f, stroke);
    DrawLineEx({cx + 6.0f, wing_y}, {cx, tip_y}, 2.4f, stroke);
}

Color action_tone_for_state(bool update_available, bool installed, bool downloading) {
    const auto& t = *g_theme;
    if (downloading) {
        return t.text_muted;
    }
    if (update_available) {
        return t.accent;
    }
    if (!installed) {
        return t.success;
    }
    return t.fast;
}

void draw_toned_button(Rectangle rect,
                       const char* label,
                       int font_size,
                       Color tone,
                       unsigned char alpha,
                       unsigned char base_alpha,
                       unsigned char hover_alpha) {
    const auto& t = *g_theme;
    const Color base = with_alpha(lerp_color(t.section, tone, 0.14f), base_alpha);
    const Color hover = with_alpha(lerp_color(t.section, tone, 0.28f), hover_alpha);
    ui::draw_button_colored(rect, label, font_size, base, hover, with_alpha(t.text, alpha), 1.4f);
}

}  // namespace

void draw(state& state, float anim_t, Rectangle origin_rect) {
    const auto& t = *g_theme;
    const float play_t = std::clamp(anim_t, 0.0f, 1.0f);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_layout(anim_t, origin_rect);
    const float content_fade_t = std::clamp((play_t - 0.16f) / 0.66f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    state.jackets.poll();
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);

    const auto indices = detail::filtered_indices(state);
    const auto& songs = detail::active_songs(state);
    const bool loading = state.catalog_loading;
    const char* caption = detail::catalog_caption(state, songs);
    const float detail_t = std::clamp(state.detail_transition, 0.0f, 1.0f);
    const float jacket_t = state.detail_open
        ? tween::ease_out_cubic(std::pow(detail_t, 1.35f))
        : tween::ease_out_cubic(detail_t);
    const float detail_content_t = std::clamp((detail_t - 0.16f) / 0.84f, 0.0f, 1.0f);
    const float grid_fade_t = detail_t >= 0.96f ? 0.0f : std::clamp(1.0f - detail_t / 0.96f, 0.0f, 1.0f);
    const unsigned char grid_alpha =
        static_cast<unsigned char>(static_cast<float>(alpha) * std::clamp(grid_fade_t, 0.0f, 1.0f));
    const unsigned char detail_alpha =
        static_cast<unsigned char>(static_cast<float>(alpha) * detail_content_t);

    ui::draw_button_colored(current.back_rect, "HOME", 16,
                            with_alpha(button_base, normal_row_alpha),
                            with_alpha(button_hover, hover_row_alpha),
                            with_alpha(t.text, alpha), 1.5f);

    ui::draw_rect_f(current.sidebar_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * grid_fade_t / 2.0f)));
    ui::draw_rect_lines(current.sidebar_rect, 1.2f, with_alpha(t.border_light, grid_alpha));
    draw_song_search_input(current.search_rect, state.search_input, "", localization::tr_literal("Search"),
                           14, 64,
                           button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha,
                           grid_alpha);
    constexpr float kSidebarTitleY = 106.0f;
    constexpr float kSourceTitleY = 594.0f;
    constexpr float kSidebarDividerY = 562.0f;
    draw_browse_body_text_in_rect("DISCOVERY", 15,
                          {current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSidebarTitleY,
                           current.sidebar_rect.width - 48.0f, 20.0f},
                          with_alpha(t.accent, grid_alpha), ui::text_align::left);
    const discovery_view views[] = {
        discovery_view::overview,
        discovery_view::new_arrivals,
        discovery_view::rising,
        discovery_view::hidden_gems,
        discovery_view::recommended,
        discovery_view::needs_charts,
    };
    for (int index = 0; index < 6; ++index) {
        const bool active = state.view == views[index];
        ui::draw_button_colored(detail::sidebar_button_rect(current.sidebar_rect, index),
                                view_label(views[index]), 14,
                                with_alpha(active ? button_selected : button_base,
                                           static_cast<unsigned char>((active ? selected_row_alpha : normal_row_alpha) * grid_fade_t)),
                                with_alpha(active ? button_selected : button_hover,
                                           static_cast<unsigned char>((active ? selected_row_alpha : hover_row_alpha) * grid_fade_t)),
                                with_alpha(active ? t.text : t.text_secondary, grid_alpha), 1.2f);
    }
    ui::draw_line_ex({current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSidebarDividerY},
                     {current.sidebar_rect.x + current.sidebar_rect.width - 24.0f, current.sidebar_rect.y + kSidebarDividerY},
                     1.2f, with_alpha(t.border_active, grid_alpha));
    draw_browse_body_text_in_rect("SOURCE", 15,
                          {current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSourceTitleY,
                           current.sidebar_rect.width - 48.0f, 20.0f},
                          with_alpha(t.accent, grid_alpha), ui::text_align::left);
    const source_filter sources[] = {
        source_filter::all,
        source_filter::official,
        source_filter::community,
    };
    for (int index = 0; index < 3; ++index) {
        const bool active = state.source == sources[index];
        ui::draw_button_colored(detail::source_button_rect(current.sidebar_rect, index),
                                source_label(sources[index]), 14,
                                with_alpha(active ? button_selected : button_base,
                                           static_cast<unsigned char>((active ? selected_row_alpha : normal_row_alpha) * grid_fade_t)),
                                with_alpha(active ? button_selected : button_hover,
                                           static_cast<unsigned char>((active ? selected_row_alpha : hover_row_alpha) * grid_fade_t)),
                                with_alpha(active ? t.text : t.text_secondary, grid_alpha), 1.2f);
    }

    ui::draw_rect_f(current.content_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * grid_fade_t / 2.0f)));
    ui::draw_rect_lines(current.content_rect, 1.2f, with_alpha(t.border_light, grid_alpha));
    const std::string content_caption = state.view == discovery_view::overview
        ? "Overview"
        : view_label(state.view);
    draw_browse_body_text_in_rect(content_caption.c_str(), 17,
                          {current.content_rect.x + 12.0f, current.content_rect.y + 6.0f,
                           current.content_rect.width * 0.52f, 20.0f},
                          with_alpha(loading ? t.text : t.text_secondary, grid_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())),
                          14,
                          {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                           current.content_rect.width * 0.5f - 12.0f, 16.0f},
                          with_alpha(t.text_muted, grid_alpha), ui::text_align::right);
    draw_browse_body_text_in_rect("Press Esc to return to the grid",
                          14,
                          {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                           current.content_rect.width * 0.5f - 12.0f, 16.0f},
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);

    Rectangle source_jacket_rect = current.hero_jacket_rect;
    bool selected_card_drawn = false;
    {
        ui::scoped_clip_rect song_clip(current.song_grid_rect);
        if (indices.empty() && grid_alpha > 0) {
            const Rectangle placeholder = {
                current.song_grid_rect.x + 96.0f,
                current.song_grid_rect.y + current.song_grid_rect.height * 0.5f - 42.0f,
                current.song_grid_rect.width - 192.0f,
                84.0f,
            };
            ui::draw_rect_f(placeholder, with_alpha(button_base, static_cast<unsigned char>(selected_row_alpha * grid_fade_t)));
            ui::draw_rect_lines(placeholder, 1.5f, with_alpha(t.border_light, grid_alpha));
            const char* empty_title = loading
                ? "Loading..."
                : (state.mode == catalog_mode::owned && state.owned_loading)
                    ? "Syncing owned songs..."
                : state.catalog_maintenance
                    ? "Server maintenance"
                : (state.catalog_request_failed ? "Could not reach raythm-Server." : "No songs found.");
            draw_browse_body_text_in_rect(empty_title,
                                  26, {placeholder.x, placeholder.y + 8.0f, placeholder.width, 28.0f},
                                  with_alpha(t.text, grid_alpha), ui::text_align::center);
            if (!loading && state.catalog_request_failed) {
                const std::string detail = !state.catalog_status_message.empty()
                    ? state.catalog_status_message
                    : state.catalog_maintenance
                        ? "Online features are temporarily unavailable. Please try again later."
                        : "Check the server URL and confirm raythm-Server is running.";
                draw_browse_body_text_in_rect(detail.c_str(),
                                      14, {placeholder.x + 20.0f, placeholder.y + 42.0f, placeholder.width - 40.0f, 16.0f},
                                      with_alpha(t.text_muted, grid_alpha), ui::text_align::center);
                if (!state.catalog_server_url.empty()) {
                    const std::string server_label = "Tried: " + state.catalog_server_url;
                    draw_browse_body_text_in_rect(server_label.c_str(),
                                          12, {placeholder.x + 20.0f, placeholder.y + 58.0f, placeholder.width - 40.0f, 14.0f},
                                          with_alpha(t.text_hint, grid_alpha), ui::text_align::center);
                }
            }
        }

        const std::vector<detail::overview_shelf_row> overview_rows = detail::overview_shelf_rows(state);
        for (int shelf_row = 0; shelf_row < static_cast<int>(overview_rows.size()); ++shelf_row) {
            const detail::overview_shelf_row& row = overview_rows[static_cast<size_t>(shelf_row)];
            const Rectangle prev_arrow = detail::overview_shelf_prev_button_rect(
                current.song_grid_rect, shelf_row, state.song_scroll_y);
            const Rectangle arrow = detail::overview_shelf_next_button_rect(
                current.song_grid_rect, shelf_row, state.song_scroll_y);
            if (arrow.y + arrow.height < current.song_grid_rect.y - 4.0f ||
                arrow.y > current.song_grid_rect.y + current.song_grid_rect.height + 4.0f) {
                continue;
            }

            const float row_top = arrow.y - 34.0f;
            draw_browse_body_text_in_rect(shelf_display_title(row.key).c_str(),
                                  15,
                                  {current.song_grid_rect.x + 10.0f, row_top + 7.0f,
                                   current.song_grid_rect.width - 70.0f, 20.0f},
                                  with_alpha(t.text, grid_alpha), ui::text_align::left);
            if (row.total_count > detail::kSongGridColumns) {
                const auto target_it = state.overview_shelf_scroll_x_target.find(row.key);
                const float target_scroll = target_it == state.overview_shelf_scroll_x_target.end()
                    ? row.scroll_x
                    : target_it->second;
                const bool can_prev = target_scroll > 0.001f || row.scroll_x > 0.001f;
                const bool can_next = target_scroll < static_cast<float>(row.total_count - detail::kSongGridColumns) - 0.001f ||
                    row.scroll_x < static_cast<float>(row.total_count - detail::kSongGridColumns) - 0.001f;
                const auto draw_shelf_arrow = [&](Rectangle rect, const char* label, bool enabled) {
                    const bool hovered = enabled && ui::is_hovered(rect);
                    const unsigned char arrow_alpha = static_cast<unsigned char>(
                        (enabled ? (hovered ? hover_row_alpha : normal_row_alpha) : normal_row_alpha / 3) * grid_fade_t);
                    ui::draw_rect_f(rect, with_alpha(hovered ? button_hover : button_base, arrow_alpha));
                    ui::draw_rect_lines(rect, 1.15f, with_alpha(hovered ? t.border_active : t.border_light,
                                                               enabled ? grid_alpha : static_cast<unsigned char>(grid_alpha / 3)));
                    draw_browse_body_text_in_rect(label,
                                          31,
                                          {rect.x, rect.y + rect.height * 0.5f - 22.0f, rect.width, 44.0f},
                                          with_alpha(enabled ? t.text : t.text_muted,
                                                     enabled ? grid_alpha : static_cast<unsigned char>(grid_alpha / 3)),
                                          ui::text_align::center);
                };
                draw_shelf_arrow(prev_arrow, "<", can_prev);
                draw_shelf_arrow(arrow, ">", can_next);
            }
        }

        for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
            const int song_index = indices[static_cast<size_t>(display_index)];
            const song_entry_state& song = songs[static_cast<size_t>(song_index)];
            Rectangle card = detail::song_row_rect(state, current.song_grid_rect, display_index, state.song_scroll_y);
            if (card.y + card.height < current.song_grid_rect.y - 4.0f ||
                card.y > current.song_grid_rect.y + current.song_grid_rect.height + 4.0f) {
                continue;
            }
            const Rectangle track = detail::uses_overview_shelves(state)
                ? detail::overview_shelf_track_rect(current.song_grid_rect)
                : current.song_grid_rect;
            if (card.x + card.width < track.x - 4.0f || card.x > track.x + track.width + 4.0f) {
                continue;
            }
            const Rectangle card_clip = detail::uses_overview_shelves(state)
                ? Rectangle{track.x, card.y, track.width, card.height}
                : current.song_grid_rect;
            ui::scoped_clip_rect card_clip_scope(card_clip);

            const bool selected = song_index == detail::selected_song_index_ref(state);
            const bool hovered = ui::is_hovered(card);
            const unsigned char row_alpha = static_cast<unsigned char>((selected ? selected_row_alpha
                : hovered ? hover_row_alpha
                          : normal_row_alpha) * grid_fade_t);
            ui::draw_rect_f(card, with_alpha(selected ? button_selected : button_base, row_alpha));
            ui::draw_rect_lines(card, 1.15f,
                                with_alpha(selected ? t.border_active : t.border_light, grid_alpha));

            const Rectangle jacket_rect = song_card_jacket_rect(card);
            if (selected) {
                source_jacket_rect = jacket_rect;
                selected_card_drawn = true;
            }
            const bool hide_selected_jacket = selected && detail_t > 0.001f;
            if (!hide_selected_jacket) {
                if (const Texture2D* jacket = state.jackets.get(song.song.song)) {
                    DrawTexturePro(*jacket,
                                   {0.0f, 0.0f, static_cast<float>(jacket->width), static_cast<float>(jacket->height)},
                                   jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, grid_alpha));
                } else {
                    const float selected_placeholder_t = selected
                        ? tween::smoothstep(tween::remap_clamped(detail_t, 0.12f, 0.0f))
                        : 1.0f;
                    const unsigned char placeholder_alpha =
                        static_cast<unsigned char>(static_cast<float>(grid_alpha) * selected_placeholder_t);
                    ui::draw_rect_f(jacket_rect, with_alpha(t.bg_alt, row_alpha));
                    draw_browse_body_text_in_rect("JACKET", 18, jacket_rect, with_alpha(t.text_muted, placeholder_alpha),
                                          ui::text_align::center);
                }
                ui::draw_rect_lines(jacket_rect, 1.0f, with_alpha(t.border_image, grid_alpha));
            }

            const std::string badge_label = detail::song_status_label(song);
            if (!badge_label.empty()) {
                const Rectangle badge_rect = {card.x + card.width - 90.0f, card.y + 12.0f, 72.0f, 18.0f};
                draw_browse_body_text_in_rect(badge_label.c_str(), 12, badge_rect,
                                      with_alpha(detail::song_status_color(song), grid_alpha), ui::text_align::right);
            }

            draw_body_marquee_text(song.song.song.meta.title.c_str(),
                              {jacket_rect.x + jacket_rect.width + 16.0f, jacket_rect.y,
                               card.width - jacket_rect.width - 48.0f, 28.0f},
                              18, with_alpha(t.text, grid_alpha), now);
            const std::string card_subtitle = song_subtitle(song.song.song.meta);
            draw_body_marquee_text(card_subtitle.c_str(),
                              {jacket_rect.x + jacket_rect.width + 16.0f, jacket_rect.y + 28.0f,
                               card.width - jacket_rect.width - 48.0f, 22.0f},
                              13, with_alpha(t.text_muted, grid_alpha), now);
            const auto draw_card_tag_row = [&](const std::vector<std::string>& labels,
                                               float y,
                                               bool keyword) {
                float tag_x = jacket_rect.x + jacket_rect.width + 16.0f;
                const float max_x = card.x + card.width - 18.0f;
                for (const std::string& label : labels) {
                    if (label.empty()) {
                        continue;
                    }
                    const Color tag_color = keyword ? keyword_color_for_label(label) : genre_color_for_label(label);
                    const float width = std::clamp(ui::measure_body_text_size(label.c_str(), 11.0f).x + 16.0f,
                                                   58.0f, 118.0f);
                    if (tag_x + width > max_x) {
                        break;
                    }
                    const Rectangle tag_rect = {tag_x, y, width, 20.0f};
                    ui::draw_rect_f(tag_rect, with_alpha(button_base, row_alpha));
                    ui::draw_rect_lines(tag_rect, 1.0f, with_alpha(tag_color, grid_alpha));
                    draw_browse_body_text_in_rect(label.c_str(), 11, tag_rect,
                                          with_alpha(tag_color, grid_alpha), ui::text_align::center);
                    tag_x += width + 7.0f;
                }
            };
            draw_card_tag_row(genre_labels(song.song.song.meta), jacket_rect.y + 54.0f, false);
            draw_card_tag_row(song.song.song.meta.keywords, jacket_rect.y + 78.0f, true);
            const float footer_x = card.x + 16.0f;
            const float footer_y = card.y + card.height - 28.0f;
            if (song.song.song.meta.has_play_count) {
                draw_browse_body_text_in_rect(format_count_label("plays", song.song.song.meta.play_count),
                                      12,
                                      {footer_x, footer_y, 88.0f, 16.0f},
                                      with_alpha(t.text_muted, grid_alpha), ui::text_align::left);
            }
            draw_browse_body_text_in_rect(TextFormat("charts %d", std::max(song.song.song.meta.chart_count,
                                                                    static_cast<int>(song.charts.size()))),
                                  12,
                                  {footer_x + (song.song.song.meta.has_play_count ? 96.0f : 0.0f),
                                   footer_y, 120.0f, 16.0f},
                                  with_alpha(t.text_muted, grid_alpha), ui::text_align::left);
        }
    }

    const song_entry_state* song = selected_song(state);
    const chart_entry_state* chart = selected_chart(state);
    if (song == nullptr) {
        return;
    }

    if (!selected_card_drawn) {
        const int display_index = selected_song_display_index(state);
        if (display_index >= 0) {
            source_jacket_rect =
                song_card_jacket_rect(detail::song_row_rect(state, current.song_grid_rect, display_index, state.song_scroll_y));
        }
    }

    if (!state.detail_open) {
        ui::draw_rect_f(current.preview_panel_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
        ui::draw_rect_lines(current.preview_panel_rect, 1.2f, with_alpha(t.border_light, alpha));

        const Rectangle jacket_rect = {
            current.preview_panel_rect.x + (current.preview_panel_rect.width - 260.0f) * 0.5f,
            current.preview_panel_rect.y + 38.0f,
            260.0f,
            260.0f,
        };
        if (const Texture2D* jacket = state.jackets.get(song->song.song)) {
            DrawTexturePro(*jacket,
                           {0.0f, 0.0f, static_cast<float>(jacket->width), static_cast<float>(jacket->height)},
                           jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, alpha));
        } else {
            ui::draw_rect_f(jacket_rect, with_alpha(t.bg_alt, selected_row_alpha));
            draw_browse_body_text_in_rect("JACKET", 24, jacket_rect, with_alpha(t.text_muted, alpha), ui::text_align::center);
        }
        ui::draw_rect_lines(jacket_rect, 1.4f, with_alpha(t.border_image, alpha));

        draw_body_marquee_text(song->song.song.meta.title.c_str(),
                          {current.preview_panel_rect.x + 26.0f, jacket_rect.y + jacket_rect.height + 24.0f,
                           current.preview_panel_rect.width - 52.0f, 38.0f},
                          25, with_alpha(t.text, alpha), now, ui::text_align::center);
        draw_body_marquee_text(song->song.song.meta.artist.c_str(),
                          {current.preview_panel_rect.x + 26.0f, jacket_rect.y + jacket_rect.height + 62.0f,
                           current.preview_panel_rect.width - 52.0f, 26.0f},
                          17, with_alpha(t.text_secondary, alpha), now, ui::text_align::center);

        draw_transport_skip_button(preview_prev_button_rect(current.preview_panel_rect), false, alpha);
        draw_transport_toggle_button(preview_play_button_rect(current.preview_panel_rect),
                                     audio_manager::instance().is_preview_playing(), alpha);
        draw_transport_skip_button(preview_next_button_rect(current.preview_panel_rect), true, alpha);
        const Rectangle bar = preview_progress_rect(current.preview_panel_rect);
        const double preview_length = detail::preview_display_length_seconds(*song);
        const double preview_position = state.preview_bar_dragging
            ? state.preview_bar_drag_position_seconds
            : audio_manager::instance().get_preview_position_seconds();
        const float preview_ratio =
            preview_length > 0.0 ? std::clamp(static_cast<float>(preview_position / preview_length), 0.0f, 1.0f) : 0.0f;
        ui::draw_rect_f(bar, with_alpha(t.bg_alt, normal_row_alpha));
        ui::draw_rect_f({bar.x, bar.y, bar.width * preview_ratio, bar.height}, with_alpha(t.accent, alpha));
        ui::draw_rect_lines(bar, 1.0f, with_alpha(t.border_light, alpha));
        draw_browse_body_text_in_rect(
            TextFormat("%s / %s",
                       detail::format_time_label(preview_position).c_str(),
                       preview_length > 0.0 ? detail::format_time_label(preview_length).c_str() : "--:--"),
            12,
            {bar.x, bar.y + 16.0f, bar.width, 18.0f},
            with_alpha(t.text_muted, alpha), ui::text_align::right);

        draw_browse_body_text_in_rect("GENRES", 13,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 42.0f, 140.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
        const auto draw_preview_tag_row = [&](const std::vector<std::string>& labels,
                                              float y,
                                              Color fallback_color,
                                              int color_mode,
                                              int max_rows) {
            float x = current.preview_panel_rect.x + 24.0f;
            float row_y = y;
            int row = 0;
            const float max_x = current.preview_panel_rect.x + current.preview_panel_rect.width - 24.0f;
            for (const std::string& label : labels) {
                if (label.empty()) {
                    continue;
                }
                const Color color = color_mode == 1 ? genre_color_for_label(label)
                    : color_mode == 2             ? keyword_color_for_label(label)
                                                  : fallback_color;
                const float width = std::clamp(ui::measure_body_text_size(label.c_str(), 13.0f).x + 22.0f, 70.0f, 150.0f);
                if (x + width > max_x) {
                    ++row;
                    if (row >= max_rows) {
                        break;
                    }
                    x = current.preview_panel_rect.x + 24.0f;
                    row_y += 38.0f;
                }
                const Rectangle tag_rect = {x, row_y, width, 30.0f};
                ui::draw_rect_f(tag_rect, with_alpha(button_base, normal_row_alpha));
                ui::draw_rect_lines(tag_rect, 1.0f, with_alpha(color, alpha));
                draw_browse_body_text_in_rect(label.c_str(), 13, tag_rect,
                                      with_alpha(color, alpha), ui::text_align::center);
                x += width + 8.0f;
            }
        };
        draw_preview_tag_row(genre_labels(song->song.song.meta), bar.y + 64.0f, t.accent, 1, 2);
        draw_browse_body_text_in_rect("KEYWORDS", 13,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 152.0f, 140.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
        draw_preview_tag_row(song->song.song.meta.keywords, bar.y + 174.0f, t.fast, 2, 2);
        const auto draw_preview_stat = [&](Rectangle rect, const char* label, const char* value, Color accent) {
            ui::draw_rect_f(rect, with_alpha(button_base, normal_row_alpha));
            ui::draw_rect_lines(rect, 1.0f, with_alpha(accent, alpha));
            draw_browse_body_text_in_rect(label, 11,
                                  {rect.x + 10.0f, rect.y + 7.0f, rect.width - 20.0f, 14.0f},
                                  with_alpha(t.text_muted, alpha), ui::text_align::left);
            draw_browse_body_text_in_rect(value, 17,
                                  {rect.x + 10.0f, rect.y + 22.0f, rect.width - 20.0f, 20.0f},
                                  with_alpha(t.text, alpha), ui::text_align::right);
        };
        const float stat_gap = 10.0f;
        const float stat_width = (current.preview_panel_rect.width - 48.0f - stat_gap) * 0.5f;
        const Rectangle bpm_stat = {current.preview_panel_rect.x + 24.0f, bar.y + 262.0f, stat_width, 48.0f};
        const Rectangle charts_stat = {bpm_stat.x + stat_width + stat_gap, bpm_stat.y, stat_width, bpm_stat.height};
        draw_preview_stat(bpm_stat, "BPM", TextFormat("%.0f", song->song.song.meta.base_bpm), t.accent);
        draw_preview_stat(charts_stat, "CHARTS",
                          TextFormat("%d", std::max(song->song.song.meta.chart_count,
                                                    static_cast<int>(song->charts.size()))),
                          t.fast);
        draw_toned_button(preview_open_button_rect(current.preview_panel_rect), "OPEN SONG", 16,
                          t.accent, alpha, selected_row_alpha, hover_row_alpha);
        return;
    }

    const Rectangle animated_jacket_rect = tween::lerp(source_jacket_rect, current.hero_jacket_rect, jacket_t);

    ui::draw_rect_f(current.detail_left_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t / 2.0f)));
    ui::draw_rect_lines(current.detail_left_rect, 1.2f, with_alpha(t.border_light, detail_alpha));
    ui::draw_rect_f(current.detail_right_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t / 2.0f)));
    ui::draw_rect_lines(current.detail_right_rect, 1.2f, with_alpha(t.border_light, detail_alpha));

    draw_browse_body_text_in_rect("SONG", 16,
                          {current.detail_left_rect.x + 28.0f, current.detail_left_rect.y + 22.0f,
                           120.0f, 22.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::left);

    if (detail_t > 0.001f) {
        if (const Texture2D* hero_jacket = state.jackets.get(song->song.song)) {
            DrawTexturePro(*hero_jacket,
                           {0.0f, 0.0f, static_cast<float>(hero_jacket->width), static_cast<float>(hero_jacket->height)},
                           animated_jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, alpha));
        } else {
            const float hero_placeholder_t = state.detail_open
                ? tween::smoothstep(tween::remap_clamped(detail_t, 0.84f, 1.0f))
                : tween::smoothstep(tween::remap_clamped(detail_t, 0.90f, 1.0f));
            const unsigned char placeholder_alpha =
                static_cast<unsigned char>(static_cast<float>(alpha) * hero_placeholder_t);
            ui::draw_rect_f(animated_jacket_rect, with_alpha(t.bg_alt, selected_row_alpha));
            if (placeholder_alpha > 0) {
                draw_browse_body_text_in_rect("JACKET", 26, animated_jacket_rect,
                                      with_alpha(t.text_muted, placeholder_alpha), ui::text_align::center);
            }
        }
        ui::draw_rect_lines(animated_jacket_rect, 1.5f, with_alpha(t.border_image, alpha));
    }

    if (detail_alpha == 0) {
        return;
    }

    const Rectangle song_info_rect = {
        current.hero_jacket_rect.x + current.hero_jacket_rect.width + 22.0f,
        current.hero_jacket_rect.y,
        current.detail_left_rect.x + current.detail_left_rect.width -
            (current.hero_jacket_rect.x + current.hero_jacket_rect.width + 62.0f),
        280.0f,
    };
    const Rectangle title_rect = {
        song_info_rect.x,
        song_info_rect.y + 2.0f,
        song_info_rect.width,
        38.0f,
    };
    const Rectangle artist_rect = {
        title_rect.x,
        title_rect.y + 38.0f,
        title_rect.width,
        27.0f
    };
    draw_body_marquee_text(song->song.song.meta.title.c_str(), title_rect, 27, with_alpha(t.text, detail_alpha), now);
    const std::string detail_subtitle = song_subtitle(song->song.song.meta);
    draw_body_marquee_text(detail_subtitle.c_str(), artist_rect, 17,
                      with_alpha(t.text_secondary, detail_alpha), now);

    const auto draw_source_badge = [&](content_status status, Rectangle rect) {
        if (status == content_status::local) {
            return;
        }
        const char* label = chart_source_label(status);
        const Color color = status == content_status::official ? t.success : t.slow;
        ui::draw_rect_f(rect, with_alpha(button_base, normal_row_alpha));
        ui::draw_rect_lines(rect, 1.0f, with_alpha(color, detail_alpha));
        draw_browse_body_text_in_rect(label, 13, rect, with_alpha(color, detail_alpha), ui::text_align::center);
    };
    draw_source_badge(song->song.source_status, {song_info_rect.x, artist_rect.y + 42.0f, 84.0f, 28.0f});

    const auto draw_tag_row = [&](const std::vector<std::string>& labels,
                                  float start_y,
                                  Color fallback_color,
                                  int color_mode) {
        float tag_x = song_info_rect.x;
        float tag_y = start_y;
        bool drew_any = false;
        const auto draw_tag = [&](const std::string& label) {
            if (label.empty()) {
                return;
            }
            const Color color = color_mode == 1 ? genre_color_for_label(label)
                : color_mode == 2             ? keyword_color_for_label(label)
                                              : fallback_color;
            const float width = std::clamp(ui::measure_body_text_size(label.c_str(), 13.0f).x + 22.0f, 70.0f, 150.0f);
            if (tag_x + width > song_info_rect.x + song_info_rect.width) {
                tag_x = song_info_rect.x;
                tag_y += 40.0f;
            }
            const Rectangle tag_rect = {tag_x, tag_y, width, 32.0f};
            ui::draw_rect_f(tag_rect, with_alpha(button_base, normal_row_alpha));
            ui::draw_rect_lines(tag_rect, 1.0f, with_alpha(color, detail_alpha));
            draw_browse_body_text_in_rect(label.c_str(), 13, tag_rect,
                                  with_alpha(color, detail_alpha), ui::text_align::center);
            tag_x += width + 8.0f;
            drew_any = true;
        };
        for (const std::string& label : labels) {
            draw_tag(label);
        }
        return drew_any ? tag_y + 40.0f : start_y;
    };
    const float genre_y = artist_rect.y + 80.0f;
    const float keyword_y = draw_tag_row(genre_labels(song->song.song.meta), genre_y, t.accent, 1);
    draw_tag_row(song->song.song.meta.keywords, keyword_y, t.fast, 2);
    const float stats_y = song_info_rect.y + 276.0f;
    draw_browse_body_text_in_rect(TextFormat("BPM %.0f", song->song.song.meta.base_bpm), 14,
                          {song_info_rect.x, stats_y, 96.0f, 28.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::center);
    ui::draw_rect_lines({song_info_rect.x, stats_y, 96.0f, 28.0f}, 1.0f, with_alpha(t.border_light, detail_alpha));
    draw_browse_body_text_in_rect(detail::format_time_label(detail::preview_display_length_seconds(*song)).c_str(), 14,
                          {song_info_rect.x + 110.0f, stats_y, 84.0f, 28.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::center);
    ui::draw_rect_lines({song_info_rect.x + 110.0f, stats_y, 84.0f, 28.0f}, 1.0f, with_alpha(t.border_light, detail_alpha));
    draw_browse_body_text_in_rect(TextFormat("charts %d", std::max(song->song.song.meta.chart_count,
                                                           static_cast<int>(song->charts.size()))),
                          14,
                          {song_info_rect.x + 208.0f, stats_y, 110.0f, 28.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::center);
    ui::draw_rect_lines({song_info_rect.x + 208.0f, stats_y, 110.0f, 28.0f}, 1.0f, with_alpha(t.border_light, detail_alpha));

    const audio_manager& audio = audio_manager::instance();
    const double preview_length = detail::preview_display_length_seconds(*song);
    const double preview_position = state.preview_bar_dragging
        ? state.preview_bar_drag_position_seconds
        : audio.get_preview_position_seconds();
    const float preview_ratio =
        preview_length > 0.0 ? std::clamp(static_cast<float>(preview_position / preview_length), 0.0f, 1.0f) : 0.0f;
    ui::draw_rect_f(current.preview_bar_rect, with_alpha(t.bg_alt, static_cast<unsigned char>(normal_row_alpha * detail_content_t)));
    ui::draw_rect_f({current.preview_bar_rect.x, current.preview_bar_rect.y,
                     current.preview_bar_rect.width * preview_ratio, current.preview_bar_rect.height},
                    with_alpha(t.accent, detail_alpha));
    ui::draw_rect_lines(current.preview_bar_rect, 1.0f, with_alpha(t.border_light, detail_alpha));
    draw_browse_body_text_in_rect(
        TextFormat("%s / %s",
                   detail::format_time_label(preview_position).c_str(),
                   preview_length > 0.0 ? detail::format_time_label(preview_length).c_str() : "--:--"),
        13,
        {current.preview_bar_rect.x, current.preview_bar_rect.y - 18.0f, current.preview_bar_rect.width, 14.0f},
        with_alpha(t.text_muted, detail_alpha), ui::text_align::left);

    draw_transport_toggle_button(current.preview_play_rect, audio.is_preview_playing(), detail_alpha);

    const bool selected_chart_update =
        chart != nullptr && chart->installed && chart->update_available;
    const char* primary_label = state.download_in_progress ? "DOWNLOADING..."
        : (needs_download(*song) ? (song->update_available ? "UPDATE SONG" : "DOWNLOAD SONG")
           : (selected_chart_update ? "UPDATE CHART" : "OPEN LOCAL"));
    if (state.download_in_progress && state.download_progress) {
        const int total_steps = std::max(1, state.download_progress->total_steps.load());
        const int completed_steps = std::clamp(state.download_progress->completed_steps.load(), 0, total_steps);
        const size_t current_bytes = state.download_progress->current_bytes.load();
        const size_t current_total_bytes = state.download_progress->current_total_bytes.load();
        const float current_ratio = current_total_bytes > 0
            ? std::clamp(static_cast<float>(static_cast<double>(current_bytes) /
                                            static_cast<double>(current_total_bytes)), 0.0f, 1.0f)
            : 0.0f;
        const float progress_ratio =
            std::clamp((static_cast<float>(completed_steps) + current_ratio) /
                           static_cast<float>(total_steps),
                       0.0f, 1.0f);
        const Rectangle progress_rect = {
            current.primary_action_rect.x,
            current.primary_action_rect.y - 18.0f,
            current.primary_action_rect.width,
            8.0f,
        };
        ui::draw_rect_f(progress_rect,
                        with_alpha(t.bg_alt, static_cast<unsigned char>(normal_row_alpha * detail_content_t)));
        ui::draw_rect_f({progress_rect.x, progress_rect.y, progress_rect.width * progress_ratio, progress_rect.height},
                        with_alpha(t.accent, detail_alpha));
        ui::draw_rect_lines(progress_rect, 1.0f, with_alpha(t.border_light, detail_alpha));
        draw_browse_body_text_in_rect(TextFormat("%d%%", static_cast<int>(std::round(progress_ratio * 100.0f))),
                              12,
                              {progress_rect.x, progress_rect.y - 16.0f, progress_rect.width, 14.0f},
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
    }
    draw_toned_button(current.primary_action_rect,
                      primary_label,
                      15,
                      action_tone_for_state(song->update_available || selected_chart_update,
                                            song->installed,
                                            state.download_in_progress),
                      detail_alpha,
                      selected_row_alpha,
                      hover_row_alpha);

    const Rectangle chart_search_rect = {
        current.chart_list_rect.x + current.chart_list_rect.width * 0.36f,
        current.chart_list_rect.y - 298.0f,
        current.chart_list_rect.width * 0.64f,
        42.0f,
    };
    draw_browse_body_text_in_rect("CHARTS", 15,
                          {current.chart_list_rect.x, current.chart_list_rect.y - 298.0f,
                           110.0f, 24.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect("Find charts to download", 12,
                          {current.chart_list_rect.x, current.chart_list_rect.y - 274.0f,
                           180.0f, 18.0f},
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
    const char* source_labels[] = {"ALL", "OFFICIAL", "COMMUNITY", "MINE"};
    const chart_source_filter source_values[] = {
        chart_source_filter::all,
        chart_source_filter::official,
        chart_source_filter::community,
        chart_source_filter::mine,
    };
    for (int index = 0; index < 4; ++index) {
        const bool active = state.chart_source == source_values[index];
        ui::draw_button_colored(chart_source_button_rect(current.chart_list_rect, index),
                                source_labels[index], 11,
                                with_alpha(active ? button_selected : button_base,
                                           active ? selected_row_alpha : normal_row_alpha),
                                with_alpha(active ? button_selected : button_hover,
                                           active ? selected_row_alpha : hover_row_alpha),
                                with_alpha(active ? t.text : t.text_secondary, detail_alpha), 1.0f);
    }
    ui::draw_button_colored(chart_clear_button_rect(current.chart_list_rect), "CLEAR", 10,
                            with_alpha(chart_filters_active(state) ? button_base : t.section, normal_row_alpha),
                            with_alpha(button_hover, hover_row_alpha),
                            with_alpha(chart_filters_active(state) ? t.text_secondary : t.text_muted, detail_alpha),
                            1.0f);

    draw_browse_body_text_in_rect("LEVEL", 12,
                          {current.chart_list_rect.x, current.chart_list_rect.y - 120.0f, 86.0f, 18.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::left);
    draw_song_search_input(chart_level_min_input_rect(current.chart_list_rect),
                           state.min_level_input, "", "1",
                           12, 4, button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha, detail_alpha);
    draw_browse_body_text_in_rect("-", 13,
                          {current.chart_list_rect.x + 76.0f, current.chart_list_rect.y - 98.0f, 14.0f, 28.0f},
                          with_alpha(t.text_muted, detail_alpha));
    draw_song_search_input(chart_level_max_input_rect(current.chart_list_rect),
                           state.max_level_input, "", "10",
                           12, 4, button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha, detail_alpha);
    draw_level_range_slider(current.chart_list_rect, state, detail_alpha);

    draw_browse_body_text_in_rect("KEYS", 12,
                          {current.chart_list_rect.x + kChartKeyGroupX, current.chart_list_rect.y - 120.0f, 86.0f, 18.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::left);
    const char* key_labels[] = {"ALL", "4K", "5K", "6K", "7K"};
    const int key_values[] = {0, 4, 5, 6, 7};
    for (int index = 0; index < 5; ++index) {
        const bool active = state.chart_key_filter == key_values[index];
        ui::draw_button_colored(chart_key_button_rect(current.chart_list_rect, index),
                                key_labels[index], 11,
                                with_alpha(active ? button_selected : button_base,
                                           active ? selected_row_alpha : normal_row_alpha),
                                with_alpha(active ? button_selected : button_hover,
                                           active ? selected_row_alpha : hover_row_alpha),
                                with_alpha(active ? t.text : t.text_secondary, detail_alpha), 1.0f);
    }

    const char* status_labels[] = {"ANY", "LOCAL", "GET"};
    for (int index = 0; index < 3; ++index) {
        const bool active = state.chart_download_filter == index;
        ui::draw_button_colored(chart_status_button_rect(current.chart_list_rect, index),
                                status_labels[index], 11,
                                with_alpha(active ? button_selected : button_base,
                                           active ? selected_row_alpha : normal_row_alpha),
                                with_alpha(active ? button_selected : button_hover,
                                           active ? selected_row_alpha : hover_row_alpha),
                                with_alpha(active ? t.text : t.text_secondary, detail_alpha), 1.0f);
    }
    draw_song_search_input(chart_search_rect, state.chart_search_input,
                           "CHART", "difficulty / author / level",
                           13, 80,
                           button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha,
                           detail_alpha);
    const auto chart_indices = detail::filtered_chart_indices(state);
    const int visible_chart_count = song->song.song.meta.chart_count > 0
        ? std::max(song->song.song.meta.chart_count, static_cast<int>(song->charts.size()))
        : static_cast<int>(song->charts.size());
    const int filtered_chart_count = static_cast<int>(chart_indices.size());
    draw_browse_body_text_in_rect(
        !chart_filters_active(state)
            ? TextFormat("%d items", visible_chart_count)
            : TextFormat("%d / %d items", filtered_chart_count, visible_chart_count),
        14,
                          {current.chart_list_rect.x + 96.0f, current.chart_list_rect.y - 274.0f,
                           current.chart_list_rect.width * 0.24f, 16.0f},
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);

    ui::scoped_clip_rect chart_clip(current.chart_list_rect);
    if (song->charts.empty() || chart_indices.empty()) {
        const Rectangle placeholder = {
            current.chart_list_rect.x + 64.0f,
            current.chart_list_rect.y + current.chart_list_rect.height * 0.5f - 36.0f,
            current.chart_list_rect.width - 128.0f,
            72.0f,
        };
        ui::draw_rect_f(placeholder, with_alpha(button_base, static_cast<unsigned char>(selected_row_alpha * detail_content_t)));
        ui::draw_rect_lines(placeholder, 1.5f, with_alpha(t.border_light, detail_alpha));
        const char* chart_empty = song->charts_loading ? "Loading charts..."
            : (song->charts_failed ? "Could not load charts."
                                   : (song->charts.empty() ? "No charts found."
                                                           : "No charts match."));
        draw_browse_body_text_in_rect(chart_empty, 28, placeholder, with_alpha(t.text, detail_alpha), ui::text_align::center);
    }

    for (int display_index = 0; display_index < static_cast<int>(chart_indices.size()); ++display_index) {
        const int index = chart_indices[static_cast<size_t>(display_index)];
        const chart_entry_state& item = song->charts[static_cast<size_t>(index)];
        const Rectangle card = detail::chart_row_rect(current.chart_list_rect, display_index, state.chart_scroll_y);
        if (card.y + card.height < current.chart_list_rect.y - 4.0f ||
            card.y > current.chart_list_rect.y + current.chart_list_rect.height + 4.0f) {
            continue;
        }

        const bool selected = chart != nullptr && index == detail::selected_chart_index_ref(state);
        const bool hovered = ui::is_hovered(card);
        const unsigned char row_alpha = static_cast<unsigned char>((selected ? selected_row_alpha
            : hovered ? hover_row_alpha
                      : normal_row_alpha) * detail_content_t);
        ui::draw_rect_f(card, with_alpha(selected ? button_selected : button_base, row_alpha));
        ui::draw_rect_lines(card, 1.0f,
                            with_alpha(selected ? t.border_active : t.border_light, detail_alpha));

        const float row_mid_y = card.y + (card.height - 18.0f) * 0.5f;
        draw_browse_body_text_in_rect(detail::key_mode_label(item.chart.meta.key_count).c_str(),
                              15,
                              {card.x + 24.0f, row_mid_y, 44.0f, 18.0f},
                              with_alpha(detail::key_mode_color(item.chart.meta.key_count), detail_alpha),
                              ui::text_align::left);
        draw_browse_body_text_in_rect(item.chart.meta.difficulty.c_str(),
                              15,
                              {card.x + 72.0f, row_mid_y, 180.0f, 18.0f},
                              with_alpha(detail::key_mode_color(item.chart.meta.key_count), detail_alpha),
                              ui::text_align::left);
        const std::string chart_badge = detail::chart_status_label(item);
        const bool can_download_chart = !state.download_in_progress && detail::can_download_chart(*song, item);
        if (!chart_badge.empty() && !can_download_chart) {
            draw_browse_body_text_in_rect(chart_badge.c_str(), 12,
                                  {card.x + card.width - 78.0f, card.y + 14.0f, 62.0f, 14.0f},
                                  with_alpha(item.update_available ? t.accent : t.text_muted, detail_alpha),
                                  ui::text_align::right);
        }
        if (can_download_chart) {
            draw_download_icon_button(detail::chart_download_icon_rect(card),
                                      item.update_available,
                                      detail_alpha);
        }
        draw_browse_body_text_in_rect(TextFormat("Lv.%.1f", item.chart.meta.level), 15,
                              {card.x + 268.0f, row_mid_y, 90.0f, 18.0f},
                              with_alpha(t.text, detail_alpha), ui::text_align::left);
        draw_browse_body_text_in_rect(TextFormat("%d notes", item.chart.note_count), 13,
                              {card.x + 408.0f, row_mid_y + 1.0f, 130.0f, 16.0f},
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
        draw_browse_body_text_in_rect(TextFormat("by %s", item.chart.meta.chart_author.empty()
                                                   ? "Unknown"
                                                   : item.chart.meta.chart_author.c_str()),
                              13,
                              {card.x + 570.0f, row_mid_y + 1.0f, 150.0f, 16.0f},
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
        const Rectangle download_icon_rect = detail::chart_download_icon_rect(card);
        const char* source_label = chart_source_label(item.chart.source_status);
        const float source_badge_width = item.chart.source_status == content_status::community ? 104.0f
            : item.chart.source_status == content_status::official ? 84.0f
                                                                   : 58.0f;
        const float source_badge_right = can_download_chart ? download_icon_rect.x - 12.0f
                                                            : card.x + card.width - 20.0f;
        const Rectangle source_badge = {
            source_badge_right - source_badge_width,
            row_mid_y - 4.0f,
            source_badge_width,
            24.0f,
        };
        const Color source_color = chart_source_status_color(item);
        ui::draw_rect_f(source_badge, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t)));
        ui::draw_rect_lines(source_badge, 1.0f, with_alpha(source_color, detail_alpha));
        draw_browse_body_text_in_rect(source_label, 13, source_badge, with_alpha(source_color, detail_alpha),
                              ui::text_align::center);
    }
}

}  // namespace title_online_view
