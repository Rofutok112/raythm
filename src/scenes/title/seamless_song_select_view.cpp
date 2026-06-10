#include "title/seamless_song_select_view.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "chart_difficulty.h"
#include "localization/localization.h"
#include "platform/windows_input_source.h"
#include "ranking_service.h"
#include "raylib.h"
#include "scene_common.h"
#include "shared/content_status_badge.h"
#include "song_loader.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "title/center_panel_view.h"
#include "title/create_tools_view.h"
#include "title/ranking_panel_view.h"
#include "title/song_list_view.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui/icons/raythm_icons.h"
#include "virtual_screen.h"

namespace title_play_view {
namespace {

constexpr Rectangle kPlayBackButtonRect = {39.0f, 983.0f, 480.0f, 58.0f};
constexpr Rectangle kPlaySongColumnRect = {39.0f, 109.0f, 480.0f, 854.0f};
constexpr Rectangle kPlayMainColumnRect = {541.0f, 109.0f, 665.0f, 932.0f};
constexpr Rectangle kPlayRankingColumnRect = {1228.0f, 109.0f, 650.0f, 932.0f};
constexpr Rectangle kPlayJacketRect = {1258.0f, 141.0f, 212.0f, 212.0f};
constexpr Rectangle kPlayChartDetailRect = {1494.0f, 145.0f, 350.0f, 224.0f};
constexpr Rectangle kPlayMetaRect = {860.0f, 347.0f, 286.0f, 10.0f};
constexpr float kChartFilterMinLevel = 0.0f;
constexpr float kChartFilterUsefulMaxLevel = 15.0f;
constexpr float kChartFilterMaxLevel = 99.0f;
constexpr float kChartFilterUsefulTrack = 0.97f;
constexpr Rectangle kPlayChartButtonsRect = {565.0f, 542.0f, 617.0f, 444.0f};
constexpr Rectangle kPlayRankingHeaderRect = {1256.0f, 466.0f, 596.0f, 38.0f};
constexpr Rectangle kPlayRankingSourceLocalRect = {1708.0f, 466.0f, 144.0f, 38.0f};
constexpr Rectangle kPlayRankingSourceOnlineRect = {1556.0f, 466.0f, 144.0f, 38.0f};
constexpr Rectangle kPlayRankingListRect = {1256.0f, 512.0f, 596.0f, 431.0f};
constexpr Rectangle kCreateSongColumnRect = {99.0f, 162.0f, 507.0f, 756.0f};
constexpr Rectangle kCreateMainColumnRect = {657.0f, 150.0f, 603.0f, 780.0f};
constexpr Rectangle kCreateRankingColumnRect = {1317.0f, 153.0f, 507.0f, 774.0f};
constexpr Rectangle kCreateJacketRect = {684.0f, 273.0f, 282.0f, 282.0f};
constexpr Rectangle kCreateChartDetailRect = {1002.0f, 273.0f, 258.0f, 135.0f};
constexpr Rectangle kCreateMetaRect = {684.0f, 582.0f, 576.0f, 90.0f};
constexpr Rectangle kCreateChartButtonsRect = {684.0f, 600.0f, 576.0f, 327.0f};
constexpr Rectangle kCreateRankingHeaderRect = {1317.0f, 153.0f, 507.0f, 54.0f};
constexpr Rectangle kCreateRankingSourceLocalRect = {1551.0f, 147.0f, 129.0f, 51.0f};
constexpr Rectangle kCreateRankingSourceOnlineRect = {1689.0f, 147.0f, 135.0f, 51.0f};
constexpr Rectangle kCreateRankingListRect = {1317.0f, 225.0f, 507.0f, 702.0f};
constexpr float kContextMenuInnerPadding = 6.0f;
constexpr Rectangle kFallbackOriginRect = {840.0f, 564.0f, 240.0f, 90.0f};
constexpr Vector2 kSeedSongOffset = {-495.0f, 33.0f};
constexpr Vector2 kSeedMainOffset = {0.0f, 9.0f};
constexpr Vector2 kSeedRankingOffset = {495.0f, 39.0f};
constexpr Vector2 kSeedBackOffset = {-642.0f, -291.0f};
constexpr Vector2 kSeedJacketOffset = {-102.0f, -39.0f};
constexpr Vector2 kSeedMetaOffset = {15.0f, 138.0f};
constexpr Vector2 kSeedChartDetailOffset = {228.0f, -15.0f};

Rectangle centered_icon_rect(Rectangle rect, float inset) {
    const float size = std::max(1.0f, std::min(rect.width, rect.height) - inset * 2.0f);
    return {
        rect.x + (rect.width - size) * 0.5f,
        rect.y + (rect.height - size) * 0.5f,
        size,
        size
    };
}

float level_filter_t(float level) {
    const float clamped = std::clamp(level, kChartFilterMinLevel, kChartFilterMaxLevel);
    if (clamped <= kChartFilterUsefulMaxLevel) {
        return ((clamped - kChartFilterMinLevel) / (kChartFilterUsefulMaxLevel - kChartFilterMinLevel)) *
               kChartFilterUsefulTrack;
    }
    return 1.0f;
}

float level_from_filter_t(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    if (clamped > kChartFilterUsefulTrack) {
        return kChartFilterMaxLevel;
    }
    const float level = kChartFilterMinLevel +
                        (clamped / kChartFilterUsefulTrack) *
                            (kChartFilterUsefulMaxLevel - kChartFilterMinLevel);
    const float rounded = std::round(level * 10.0f) / 10.0f;
    return rounded >= kChartFilterMaxLevel - 0.5f ? kChartFilterMaxLevel : rounded;
}

Rectangle level_filter_chip_rect(Rectangle range, float level) {
    const float t = level_filter_t(level);
    const float x = range.x + range.width * t;
    return {x - 24.0f, range.y - 4.0f, 48.0f, 28.0f};
}

void draw_level_filter_gradient(Rectangle rect, unsigned char alpha) {
    constexpr int kSegments = 48;
    for (int i = 0; i < kSegments; ++i) {
        const float from_level = kChartFilterUsefulMaxLevel * (static_cast<float>(i) / kSegments);
        const float to_level = kChartFilterUsefulMaxLevel * (static_cast<float>(i + 1) / kSegments);
        const float from_t = level_filter_t(from_level);
        const float to_t = level_filter_t(to_level);
        const Rectangle segment = {
            rect.x + rect.width * from_t,
            rect.y,
            std::max(1.0f, rect.width * (to_t - from_t)),
            rect.height,
        };
        DrawRectangleGradientH(static_cast<int>(segment.x), static_cast<int>(segment.y),
                               static_cast<int>(std::ceil(segment.width)), static_cast<int>(segment.height),
                               with_alpha(difficulty_level_color(from_level), alpha),
                               with_alpha(difficulty_level_color(to_level), alpha));
    }
    const float useful_end_x = rect.x + rect.width * level_filter_t(kChartFilterUsefulMaxLevel);
    ui::draw_rect_f({useful_end_x, rect.y, rect.x + rect.width - useful_end_x, rect.height},
                    with_alpha({34, 38, 46, 255}, alpha));
}
constexpr Vector2 kSeedChartButtonsOffset = {81.0f, 240.0f};
constexpr Vector2 kSeedRankingHeaderOffset = {522.0f, -276.0f};
constexpr Vector2 kSeedRankingSourceLocalOffset = {627.0f, -285.0f};
constexpr Vector2 kSeedRankingSourceOnlineOffset = {768.0f, -285.0f};
constexpr Vector2 kSeedRankingListOffset = {522.0f, 30.0f};
constexpr float kWheelScrollStep = 63.0f;
constexpr int kPlaySongContextMenuItemCount = 1;
constexpr int kPlayChartContextMenuItemCount = 1;

Rectangle context_menu_item_rect(Rectangle menu_rect, int index = 0) {
    return {
        menu_rect.x + kContextMenuInnerPadding,
        menu_rect.y + kContextMenuInnerPadding +
            static_cast<float>(index) *
                (song_select::layout::kContextMenuItemHeight + song_select::layout::kContextMenuItemSpacing),
        menu_rect.width - kContextMenuInnerPadding * 2.0f,
        song_select::layout::kContextMenuItemHeight
    };
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

Color tag_color_for_label(const std::string& label) {
    constexpr Color kPalette[] = {
        {147, 94, 226, 255},
        {38, 167, 216, 255},
        {214, 143, 43, 255},
        {132, 204, 45, 255},
        {216, 78, 133, 255},
        {62, 126, 220, 255},
        {39, 181, 154, 255},
        {218, 91, 61, 255},
        {190, 181, 48, 255},
        {162, 103, 231, 255},
        {65, 190, 96, 255},
        {212, 94, 172, 255},
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

std::string format_duration_label(float seconds) {
    if (seconds <= 0.0f) {
        return "-";
    }
    const int total_seconds = static_cast<int>(std::round(seconds));
    return TextFormat("%d:%02d", total_seconds / 60, total_seconds % 60);
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
    const auto& t = *g_theme;
    switch (value) {
        case rank::ss: return t.rank_ss;
        case rank::s: return t.rank_s;
        case rank::aa: return t.rank_aa;
        case rank::a: return t.rank_a;
        case rank::b: return t.rank_b;
        case rank::c: return t.rank_c;
        case rank::f: return t.rank_f;
    }
    return t.text_secondary;
}

std::string format_score(int value) {
    std::string digits = std::to_string(std::max(0, value));
    for (int insert_at = static_cast<int>(digits.size()) - 3; insert_at > 0; insert_at -= 3) {
        digits.insert(static_cast<size_t>(insert_at), ",");
    }
    return digits;
}

std::string mod_summary(const play_mods& mods) {
    std::vector<std::string> labels;
    if (mods.auto_play) {
        labels.push_back("AUTO");
    }
    if (mods.no_fail) {
        labels.push_back("NOFAIL");
    }
    if (labels.empty()) {
        return "No Mod";
    }
    std::string summary = labels.front();
    for (size_t i = 1; i < labels.size(); ++i) {
        summary += " + " + labels[i];
    }
    return summary;
}

std::string format_bpm_range(float min_bpm, float max_bpm, float fallback_bpm) {
    if (min_bpm <= 0.0f && max_bpm <= 0.0f) {
        return TextFormat("%.0f", fallback_bpm);
    }
    if (std::fabs(max_bpm - min_bpm) < 0.05f) {
        return TextFormat("%.0f", min_bpm);
    }
    return TextFormat("%.0f-%.0f", min_bpm, max_bpm);
}

Rectangle start_button_rect(Rectangle ranking_column) {
    return {
        ranking_column.x + 320.0f,
        ranking_column.y + ranking_column.height - 78.0f,
        ranking_column.width - 344.0f,
        58.0f,
    };
}

Rectangle best_score_rect(Rectangle ranking_column) {
    using namespace title_play_view::mod_layout;
    return {
        ranking_column.x + kButtonLeftInset,
        ranking_column.y + ranking_column.height - kButtonBottomInset,
        kButtonWidth,
        kButtonHeight,
    };
}

Rectangle mod_modal_rect(Rectangle ranking_column) {
    using namespace title_play_view::mod_layout;
    const Rectangle button = best_score_rect(ranking_column);
    return {
        button.x,
        button.y - kModalGapFromButton - kModalHeight,
        kModalWidth,
        kModalHeight,
    };
}

Rectangle auto_mod_toggle_rect(Rectangle modal) {
    using namespace title_play_view::mod_layout;
    const float row_y = modal.y + kModalTopPadding + kHeaderHeight + kHeaderToDescriptionGap +
                        kDescriptionHeight + kDescriptionToRowsGap;
    return {
        modal.x + kModalSidePadding,
        row_y,
        modal.width - kModalSidePadding * 2.0f,
        kRowHeight,
    };
}

Rectangle no_fail_mod_toggle_rect(Rectangle modal) {
    using namespace title_play_view::mod_layout;
    const Rectangle auto_row = auto_mod_toggle_rect(modal);
    return {
        auto_row.x,
        auto_row.y + kRowHeight + kRowGap,
        auto_row.width,
        kRowHeight,
    };
}

Rectangle play_info_panel_rect(Rectangle ranking_column) {
    return {ranking_column.x, ranking_column.y, ranking_column.width, 306.0f};
}

Rectangle play_ranking_panel_rect(Rectangle ranking_column) {
    return {
        ranking_column.x,
        ranking_column.y + 337.0f,
        ranking_column.width,
        ranking_column.height - 337.0f,
    };
}

Rectangle preview_prev_button_rect(const title_play_view::layout& current) {
    return {
        current.meta_rect.x,
        current.meta_rect.y + 26.0f,
        86.0f,
        42.0f,
    };
}

Rectangle preview_play_button_rect(const title_play_view::layout& current) {
    return {
        current.meta_rect.x + 100.0f,
        current.meta_rect.y + 26.0f,
        86.0f,
        42.0f,
    };
}

Rectangle preview_next_button_rect(const title_play_view::layout& current) {
    return {
        current.meta_rect.x + 200.0f,
        current.meta_rect.y + 26.0f,
        86.0f,
        42.0f,
    };
}

void draw_play_search_input(Rectangle rect,
                            ui::text_input_state& input,
                            unsigned char alpha,
                            Color button_base,
                            Color button_hover,
                            Color button_selected,
                            unsigned char normal_row_alpha,
                            unsigned char hover_row_alpha,
                            unsigned char selected_row_alpha) {
    constexpr int kFontSize = 13;
    constexpr size_t kMaxLength = 80;
    const auto& t = *g_theme;
    ui::clamp_text_input_state(input);
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const bool clicked = ui::is_clicked(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    const Rectangle text_rect = {
        visual.x + 44.0f,
        visual.y,
        std::max(0.0f, visual.width - 56.0f),
        visual.height,
    };

    if (clicked) {
        input.active = true;
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const float local_x = mouse.x - text_rect.x + input.scroll_x;
        input.cursor = ui::text_input_cursor_from_mouse(input.value, local_x, kFontSize);
        ui::clear_text_input_selection(input);
        input.mouse_selecting = true;
    } else if (input.active && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered) {
        input.active = false;
        input.mouse_selecting = false;
        ui::clear_text_input_selection(input);
    }

    if (input.active && input.mouse_selecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const float local_x = mouse.x - text_rect.x + input.scroll_x;
        input.cursor = ui::text_input_cursor_from_mouse(input.value, local_x, kFontSize);
        input.has_selection = input.cursor != input.selection_anchor;
    }
    if (input.mouse_selecting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.mouse_selecting = false;
    }

    if (input.active) {
        windows_input_source::instance().request_text_input();
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        if (ctrl && IsKeyPressed(KEY_A)) {
            input.selection_anchor = 0;
            input.cursor = ui::utf8_codepoint_count(input.value);
            input.has_selection = input.cursor > 0;
        }
        if (ctrl && IsKeyPressed(KEY_C) && input.has_selection) {
            SetClipboardText(ui::selected_text_input_text(input).c_str());
        }
        if (ctrl && IsKeyPressed(KEY_X) && input.has_selection) {
            SetClipboardText(ui::selected_text_input_text(input).c_str());
            ui::delete_text_input_selection(input);
        }
        if (ctrl && IsKeyPressed(KEY_V)) {
            const char* clipboard = GetClipboardText();
            if (clipboard != nullptr) {
                ui::paste_text_input_at_cursor(input, clipboard, kMaxLength, ui::default_text_input_filter);
            }
        }

        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (input.has_selection) {
                ui::delete_text_input_selection(input);
            }
            if (ui::utf8_codepoint_count(input.value) < kMaxLength &&
                ui::default_text_input_filter(codepoint, input.value)) {
                ui::insert_codepoint_at_cursor(input, codepoint);
            }
            codepoint = GetCharPressed();
        }

        if (ui::text_input_key_action(KEY_BACKSPACE)) {
            if (input.has_selection) {
                ui::delete_text_input_selection(input);
            } else if (input.cursor > 0) {
                const size_t end_byte = ui::utf8_codepoint_to_byte_index(input.value, input.cursor);
                const size_t start_byte = ui::utf8_codepoint_to_byte_index(input.value, input.cursor - 1);
                input.value.erase(start_byte, end_byte - start_byte);
                --input.cursor;
                ui::clear_text_input_selection(input);
            }
        }
        if (ui::text_input_key_action(KEY_DELETE)) {
            if (input.has_selection) {
                ui::delete_text_input_selection(input);
            } else if (input.cursor < ui::utf8_codepoint_count(input.value)) {
                const size_t start_byte = ui::utf8_codepoint_to_byte_index(input.value, input.cursor);
                const size_t end_byte = ui::utf8_codepoint_to_byte_index(input.value, input.cursor + 1);
                input.value.erase(start_byte, end_byte - start_byte);
            }
        }
        if (ui::text_input_key_action(KEY_LEFT)) {
            if (input.has_selection && !shift) {
                ui::move_text_input_cursor(input, ui::text_input_selection_range(input).first, false);
            } else if (input.cursor > 0) {
                ui::move_text_input_cursor(input, input.cursor - 1, shift);
            }
        }
        if (ui::text_input_key_action(KEY_RIGHT)) {
            if (input.has_selection && !shift) {
                ui::move_text_input_cursor(input, ui::text_input_selection_range(input).second, false);
            } else if (input.cursor < ui::utf8_codepoint_count(input.value)) {
                ui::move_text_input_cursor(input, input.cursor + 1, shift);
            }
        }
        if (ui::text_input_key_action(KEY_HOME)) {
            ui::move_text_input_cursor(input, 0, shift);
        }
        if (ui::text_input_key_action(KEY_END)) {
            ui::move_text_input_cursor(input, ui::utf8_codepoint_count(input.value), shift);
        }
        if (IsKeyPressed(KEY_ENTER)) {
            input.active = false;
            input.mouse_selecting = false;
            ui::clear_text_input_selection(input);
        }
    }

    const unsigned char row_alpha = input.active ? selected_row_alpha
        : hovered ? hover_row_alpha
                  : normal_row_alpha;
    ui::draw_rect_f(visual, with_alpha(input.active ? button_selected : button_base, row_alpha));
    ui::draw_rect_lines(ui::inset(visual, 1.0f), 1.2f,
                        with_alpha(input.active ? t.border_active : t.border_light, alpha));

    const Rectangle icon_rect = {visual.x + 12.0f, visual.y, 28.0f, visual.height};
    ui::draw_text_in_rect("Q", 18, icon_rect, with_alpha(t.text_secondary, alpha), ui::text_align::center);

    ui::update_text_input_scroll(input, text_rect.width - 8.0f, kFontSize);

    const bool placeholder = input.value.empty() && !input.active;
    const char* display = placeholder ? "Search" : input.value.c_str();
    const Color text_color = with_alpha(placeholder ? t.text_hint : t.text, alpha);
    if (!input.active && !input.value.empty()) {
        draw_marquee_text(display, text_rect, kFontSize, text_color, GetTime());
    } else {
        ui::scoped_clip_rect clip(text_rect);
        ui::draw_text_in_rect(display,
                              kFontSize,
                              {text_rect.x - (input.active ? input.scroll_x : 0.0f),
                               text_rect.y, text_rect.width + (input.active ? input.scroll_x : 0.0f),
                               text_rect.height},
                              text_color,
                              ui::text_align::left);

        if (input.active && std::fmod(GetTime() * 1.6, 1.0) < 0.6) {
            const float cursor_x = text_rect.x +
                ui::text_input_prefix_width(input.value, input.cursor, kFontSize) - input.scroll_x;
            ui::draw_rect_span({cursor_x, text_rect.y + 12.0f, 1.5f, text_rect.height - 24.0f},
                               with_alpha(t.text, alpha));
        }
    }
}

void draw_transport_toggle_button(Rectangle rect, bool playing, unsigned char alpha) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    const Color border = with_alpha(playing || hovered ? t.accent : t.border_light, alpha);
    const Color fill = with_alpha(playing ? lerp_color(t.section, t.accent, 0.34f) : t.section,
                                  static_cast<unsigned char>(hovered ? alpha : alpha * 0.72f));
    ui::draw_rect_f(visual, fill);
    ui::draw_rect_lines(visual, 1.3f, border);
    const Color icon = with_alpha(playing ? t.text : (hovered ? t.text : t.text_secondary), alpha);
    if (playing) {
        raythm_icons::draw_pause(centered_icon_rect(visual, 13.0f), icon, 3.0f);
    } else {
        raythm_icons::draw_play(centered_icon_rect(visual, 13.0f), icon, 3.0f);
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
    if (next) {
        raythm_icons::draw_skip_forward(centered_icon_rect(visual, 13.0f), icon, 3.0f);
    } else {
        raythm_icons::draw_skip_back(centered_icon_rect(visual, 13.0f), icon, 3.0f);
    }
}

void draw_play_filter_panel(const title_play_view::layout& current,
                            song_select::state& state,
                            unsigned char alpha,
                            Color button_base,
                            Color button_selected,
                            unsigned char normal_row_alpha,
                            unsigned char selected_row_alpha) {
    const auto& t = *g_theme;
    const Rectangle panel = current.song_column;
    ui::draw_rect_f(panel, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
    ui::draw_rect_lines(panel, 1.2f, with_alpha(t.border_light, alpha));

    ui::draw_text_in_rect("ALL SONGS", 14,
                          {panel.x + 18.0f, panel.y + 18.0f, panel.width * 0.5f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    const std::vector<int> indices = song_select::filtered_song_indices(state);
    ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())), 12,
                          {panel.x + panel.width * 0.5f, panel.y + 18.0f, panel.width * 0.5f - 18.0f, 24.0f},
                          with_alpha(t.text_muted, alpha), ui::text_align::right);

    const Rectangle search = {panel.x + 18.0f, panel.y + 62.0f, panel.width - 94.0f, 46.0f};
    draw_play_search_input(search, state.play_search_input, alpha, button_base, t.row_soft_hover,
                           button_selected, normal_row_alpha, normal_row_alpha, selected_row_alpha);

    const Rectangle filter_button = {panel.x + panel.width - 58.0f, search.y, 40.0f, search.height};
    const bool filters_active = state.chart_source != song_select::chart_source_filter::all ||
                                !state.play_search_input.value.empty() ||
                                state.chart_key_filter != 0 ||
                                std::fabs(state.chart_min_level - kChartFilterMinLevel) > 0.001f ||
                                std::fabs(state.chart_max_level - kChartFilterMaxLevel) > 0.001f;
    const bool hovered = ui::is_hovered(filter_button);
    ui::draw_rect_f(filter_button,
                    with_alpha(filters_active || state.play_filter_modal_open ? button_selected : button_base,
                               filters_active || state.play_filter_modal_open ? selected_row_alpha : normal_row_alpha));
    ui::draw_rect_lines(filter_button, 1.2f,
                        with_alpha(filters_active || hovered ? t.accent : t.border_light, alpha));
    const Color icon_color = with_alpha(filters_active || hovered ? t.accent : t.text_secondary, alpha);
    const float cx = filter_button.x + filter_button.width * 0.5f;
    for (int i = 0; i < 3; ++i) {
        const float y = filter_button.y + 13.0f + static_cast<float>(i) * 10.0f;
        ui::draw_line_ex({cx - 11.0f, y}, {cx + 11.0f, y}, 1.6f, icon_color);
        const float knob_x = cx + (i == 0 ? -4.0f : (i == 1 ? 6.0f : -8.0f));
        ui::draw_rect_f({knob_x - 2.0f, y - 2.0f, 4.0f, 4.0f}, icon_color);
    }
}

Rectangle play_filter_button_rect(Rectangle panel) {
    return {panel.x + panel.width - 58.0f, panel.y + 62.0f, 40.0f, 46.0f};
}

Rectangle play_filter_modal_rect(const title_play_view::layout& current) {
    return {current.song_column.x + 70.0f, current.song_column.y + 146.0f, 360.0f, 438.0f};
}

void draw_play_filter_modal(const title_play_view::layout& current,
                            const song_select::state& state,
                            unsigned char alpha,
                            Color button_base,
                            Color button_selected,
                            unsigned char normal_row_alpha,
                            unsigned char selected_row_alpha) {
    if (!state.play_filter_modal_open) {
        return;
    }
    const auto& t = *g_theme;
    const Rectangle panel = play_filter_modal_rect(current);
    ui::draw_rect_f({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                    with_alpha(BLACK, static_cast<unsigned char>(80.0f * (static_cast<float>(alpha) / 255.0f))));
    ui::draw_rect_f(panel, with_alpha(t.panel, static_cast<unsigned char>(235.0f * (static_cast<float>(alpha) / 255.0f))));
    ui::draw_rect_lines(panel, 1.4f, with_alpha(t.border_active, alpha));

    const auto draw_heading = [&](const char* label, float y) {
        ui::draw_text_in_rect(label, 12, {panel.x + 18.0f, y, panel.width - 36.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
    };
    const auto draw_button = [&](Rectangle rect, const char* label, bool selected) {
        ui::draw_button_colored(rect, label, 11,
                                with_alpha(selected ? button_selected : button_base,
                                           selected ? selected_row_alpha : normal_row_alpha),
                                with_alpha(t.row_soft_hover, normal_row_alpha),
                                with_alpha(t.text, alpha), 1.0f);
    };
    const auto level_label = [](float level) {
        if (level >= kChartFilterMaxLevel - 0.05f) {
            return std::string("\xE2\x88\x9E");
        }
        return std::string(TextFormat("%.1f", level));
    };
    const auto draw_level_chip = [&](Rectangle rect, float level, bool max_chip) {
        const std::string label = level_label(level);
        const Color tone = max_chip && level >= kChartFilterMaxLevel - 0.05f
                               ? t.text_muted
                               : difficulty_level_color(level);
        ui::draw_rect_f(rect, with_alpha(lerp_color(t.bg_alt, tone, 0.18f), alpha));
        ui::draw_rect_lines(rect, 1.1f, with_alpha(tone, alpha));
        ui::draw_text_in_rect(label.c_str(), 11, rect, with_alpha(tone, alpha), ui::text_align::center);
    };

    ui::draw_text_in_rect("FILTER", 16, {panel.x + 18.0f, panel.y + 16.0f, panel.width - 36.0f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    draw_heading("SOURCE", panel.y + 60.0f);
    const float source_y = panel.y + 90.0f;
    const float source_w = (panel.width - 48.0f) / 3.0f;
    draw_button({panel.x + 18.0f, source_y, source_w, 36.0f}, "ALL",
                state.chart_source == song_select::chart_source_filter::all);
    draw_button({panel.x + 24.0f + source_w, source_y, source_w, 36.0f}, "OFFICIAL",
                state.chart_source == song_select::chart_source_filter::official);
    draw_button({panel.x + 30.0f + source_w * 2.0f, source_y, source_w, 36.0f}, "COMMUNITY",
                state.chart_source == song_select::chart_source_filter::community);

    draw_heading("LEVEL", panel.y + 164.0f);
    const Rectangle range = {panel.x + 52.0f, panel.y + 206.0f, panel.width - 104.0f, 24.0f};
    const Rectangle track = {range.x, range.y + 5.0f, range.width, 14.0f};
    ui::draw_rect_f(track, with_alpha(t.slider_track, alpha));
    draw_level_filter_gradient(track, static_cast<unsigned char>(alpha / 2));
    draw_level_chip(level_filter_chip_rect(range, state.chart_min_level), state.chart_min_level, false);
    draw_level_chip(level_filter_chip_rect(range, state.chart_max_level), state.chart_max_level, true);

    draw_heading("KEYS", panel.y + 292.0f);
    const char* key_labels[] = {"ALL", "4K", "5K", "6K", "7K"};
    const float key_w = 46.0f;
    const float key_gap = 7.0f;
    const float keys_x = panel.x + (panel.width - (key_w * 5.0f + key_gap * 4.0f)) * 0.5f;
    for (int i = 0; i < 5; ++i) {
        draw_button({keys_x + static_cast<float>(i) * (key_w + key_gap), panel.y + 318.0f, key_w, 30.0f},
                    key_labels[i], state.chart_key_filter == (i == 0 ? 0 : i + 3));
    }

    const bool filters_active = state.chart_source != song_select::chart_source_filter::all ||
                                !state.play_search_input.value.empty() ||
                                state.chart_key_filter != 0 ||
                                std::fabs(state.chart_min_level - kChartFilterMinLevel) > 0.001f ||
                                std::fabs(state.chart_max_level - kChartFilterMaxLevel) > 0.001f;
    draw_button({panel.x + 18.0f, panel.y + 378.0f, panel.width - 36.0f, 42.0f}, "CLEAR FILTERS", filters_active);
}

Rectangle play_filter_source_button_rect(Rectangle panel, int index) {
    const float source_y = panel.y + 90.0f;
    const float source_w = (panel.width - 48.0f) / 3.0f;
    return {
        panel.x + 18.0f + static_cast<float>(index) * (source_w + 6.0f),
        source_y,
        source_w,
        36.0f,
    };
}

Rectangle play_filter_key_button_rect(Rectangle panel, int index) {
    constexpr float key_w = 46.0f;
    constexpr float key_gap = 7.0f;
    const float keys_x = panel.x + (panel.width - (key_w * 5.0f + key_gap * 4.0f)) * 0.5f;
    return {keys_x + static_cast<float>(index) * (key_w + key_gap), panel.y + 318.0f, key_w, 30.0f};
}

Rectangle play_filter_clear_button_rect(Rectangle panel) {
    return {panel.x + 18.0f, panel.y + 378.0f, panel.width - 36.0f, 42.0f};
}

Rectangle play_filter_level_slider_rect(Rectangle panel) {
    return {panel.x + 34.0f, panel.y + 208.0f, panel.width - 68.0f, 18.0f};
}

const char* difficulty_factor_label(const std::string& name) {
    if (name == "density") {
        return "Density";
    }
    if (name == "stream") {
        return "Stream";
    }
    if (name == "pattern") {
        return "Pattern";
    }
    if (name == "read") {
        return "Read";
    }
    if (name == "rhythm") {
        return "Rhythm";
    }
    if (name == "tempo") {
        return "Tempo";
    }
    if (name == "rest") {
        return "No Rest";
    }
    if (name == "overlap") {
        return "LN Mix";
    }
    if (name == "hold_conflict") {
        return "LN Hand";
    }
    if (name == "hand") {
        return "Hand";
    }
    if (name == "stamina") {
        return "Stamina";
    }
    if (name == "release") {
        return "Release";
    }
    if (name == "jump") {
        return "Motion";
    }
    if (name == "chord") {
        return "Chord";
    }
    if (name == "hold") {
        return "Hold";
    }
    if (name == "balance") {
        return "Balance";
    }
    return name.c_str();
}

Color difficulty_factor_color(const std::string& name) {
    if (name == "density") {
        return {240, 186, 70, 255};
    }
    if (name == "stream" || name == "stamina") {
        return {70, 190, 230, 255};
    }
    if (name == "tempo") {
        return {82, 210, 214, 255};
    }
    if (name == "pattern" || name == "rhythm") {
        return {168, 106, 245, 255};
    }
    if (name == "release" || name == "hold" || name == "hold_conflict") {
        return {236, 105, 142, 255};
    }
    if (name == "overlap" || name == "read") {
        return {74, 205, 158, 255};
    }
    if (name == "hand" || name == "balance") {
        return {245, 132, 78, 255};
    }
    if (name == "rest") {
        return {225, 210, 90, 255};
    }
    if (name == "jump" || name == "chord") {
        return {96, 145, 238, 255};
    }
    return g_theme->accent;
}

unsigned char scaled_alpha(unsigned char alpha, float scale) {
    return static_cast<unsigned char>(
        std::clamp(static_cast<float>(alpha) * scale, 0.0f, 255.0f));
}

float status_badge_width(content_status status) {
    switch (status) {
        case content_status::community: return 92.0f;
        case content_status::official: return 78.0f;
        case content_status::modified: return 76.0f;
        case content_status::checking: return 76.0f;
        case content_status::update: return 64.0f;
        case content_status::local: return 54.0f;
    }
    return 54.0f;
}

void draw_square_status_badge(Rectangle rect, content_status status, unsigned char alpha, int font_size = 10) {
    const Color color = content_status_badge::color(status);
    ui::draw_rect_f(rect, with_alpha(g_theme->row_soft, scaled_alpha(alpha, 0.27f)));
    ui::draw_rect_lines(rect, 1.0f, with_alpha(color, alpha));
    ui::draw_text_in_rect(content_status_badge::label(status), font_size, rect,
                          with_alpha(color, alpha), ui::text_align::center);
}

std::optional<chart_difficulty::difficulty_breakdown> cached_difficulty_breakdown(const song_select::chart_option* chart) {
    static std::unordered_map<std::string, std::optional<chart_difficulty::difficulty_breakdown>> cache;
    if (chart == nullptr || chart->path.empty()) {
        return std::nullopt;
    }

    const auto found = cache.find(chart->path);
    if (found != cache.end()) {
        return found->second;
    }

    const chart_parse_result parsed = song_loader::load_chart(chart->path);
    if (!parsed.success || !parsed.data.has_value()) {
        cache.emplace(chart->path, std::nullopt);
        return std::nullopt;
    }
    auto breakdown = chart_difficulty::calculate_breakdown(*parsed.data);
    cache.emplace(chart->path, breakdown);
    return breakdown;
}

void draw_difficulty_breakdown(Rectangle rect,
                               const song_select::chart_option* chart,
                               unsigned char alpha,
                               unsigned char normal_row_alpha,
                               float chart_change_anim_t) {
    const auto& t = *g_theme;
    const Rectangle header_line = {rect.x, rect.y + 15.0f, rect.width, 1.0f};
    ui::draw_text_in_rect("DIFFICULTY FACTORS", 12,
                          {rect.x, rect.y, rect.width, 17.0f},
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_rect_f(header_line, with_alpha(t.border_light, scaled_alpha(alpha, 0.55f)));

    const std::optional<chart_difficulty::difficulty_breakdown> breakdown = cached_difficulty_breakdown(chart);
    if (!breakdown.has_value() || breakdown->factors.empty()) {
        ui::draw_text_in_rect("-", 13,
                              {rect.x, rect.y + 20.0f, rect.width, 24.0f},
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
        return;
    }

    constexpr int kVisibleRows = 5;
    constexpr float kRowHeight = 19.0f;
    constexpr float kLabelWidth = 88.0f;
    constexpr float kValueWidth = 34.0f;
    constexpr float kFullBarContribution = 4.0f;
    std::vector<const chart_difficulty::difficulty_factor_breakdown*> visible_factors;
    visible_factors.reserve(kVisibleRows);
    const chart_difficulty::difficulty_factor_breakdown* rhythm_factor = nullptr;
    for (const chart_difficulty::difficulty_factor_breakdown& factor : breakdown->factors) {
        if (factor.name == "rhythm") {
            rhythm_factor = &factor;
        }
        if (static_cast<int>(visible_factors.size()) < kVisibleRows) {
            visible_factors.push_back(&factor);
        }
    }
    if (rhythm_factor != nullptr &&
        std::find(visible_factors.begin(), visible_factors.end(), rhythm_factor) == visible_factors.end()) {
        if (visible_factors.empty()) {
            visible_factors.push_back(rhythm_factor);
        } else {
            visible_factors.back() = rhythm_factor;
        }
    }

    const int rows = static_cast<int>(visible_factors.size());
    for (int i = 0; i < rows; ++i) {
        const chart_difficulty::difficulty_factor_breakdown& factor = *visible_factors[static_cast<size_t>(i)];
        const float row_y = rect.y + 24.0f + static_cast<float>(i) * kRowHeight;
        const Rectangle label_rect = {rect.x, row_y - 2.0f, kLabelWidth, kRowHeight};
        const Rectangle bar_track = {
            rect.x + kLabelWidth,
            row_y + 4.0f,
            rect.width - kLabelWidth - kValueWidth,
            9.0f,
        };
        const Rectangle value_rect = {bar_track.x + bar_track.width + 6.0f, row_y - 2.0f,
                                      kValueWidth - 6.0f, kRowHeight};
        const float ratio = std::clamp(factor.average_contribution / kFullBarContribution, 0.0f, 1.0f);
        const float raw_reveal_t = 1.0f - std::clamp(chart_change_anim_t, 0.0f, 1.0f);
        const float row_delay = static_cast<float>(i) * 0.045f;
        const float reveal_t = tween::ease_out_cubic(
            std::clamp((raw_reveal_t - row_delay) / (1.0f - row_delay), 0.0f, 1.0f));
        const float bar_width = bar_track.width * ratio * reveal_t;
        const Color factor_color = difficulty_factor_color(factor.name);
        ui::draw_text_in_rect(difficulty_factor_label(factor.name), 12, label_rect,
                              with_alpha(factor_color, alpha), ui::text_align::left);
        ui::draw_rect_f(bar_track, with_alpha(t.bg_alt, normal_row_alpha));
        ui::draw_rect_f({bar_track.x, bar_track.y, bar_track.width, 1.0f},
                        with_alpha(t.border_light, scaled_alpha(alpha, 0.35f)));
        ui::draw_rect_f({bar_track.x, bar_track.y, bar_width, bar_track.height},
                        with_alpha(factor_color, alpha));
        if (chart_change_anim_t > 0.001f && bar_width > 3.0f) {
            const Rectangle sweep = {
                bar_track.x + std::max(0.0f, bar_width - 10.0f),
                bar_track.y,
                std::min(10.0f, bar_width),
                bar_track.height,
            };
            ui::draw_rect_f(sweep, with_alpha(WHITE, static_cast<unsigned char>(alpha / 3)));
        }
        ui::draw_text_in_rect(TextFormat("%.1f", factor.average_contribution), 11, value_rect,
                              with_alpha(t.text_muted, alpha), ui::text_align::right);
    }
}

void draw_chart_summary(Rectangle rect,
                        const song_select::chart_option* chart,
                        unsigned char alpha,
                        unsigned char normal_row_alpha) {
    const auto& t = *g_theme;
    const Color level_color = chart != nullptr ? difficulty_level_color(chart->meta.level) : t.accent;
    const Color fill = lerp_color(t.bg_alt, level_color, chart != nullptr ? 0.08f : 0.0f);
    ui::draw_rect_f(rect, with_alpha(fill, normal_row_alpha));
    ui::draw_rect_f({rect.x, rect.y, 4.0f, rect.height}, with_alpha(level_color, alpha));
    ui::draw_rect_f({rect.x + 4.0f, rect.y, rect.width - 4.0f, 1.0f},
                    with_alpha(t.border_light, scaled_alpha(alpha, 0.65f)));
    ui::draw_rect_f({rect.x + 4.0f, rect.y + rect.height - 1.0f, rect.width - 4.0f, 1.0f},
                    with_alpha(t.border_light, scaled_alpha(alpha, 0.45f)));

    if (chart == nullptr) {
        ui::draw_text_in_rect("No chart selected", 18, ui::inset(rect, 12.0f),
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
        return;
    }

    const int keys = chart->meta.key_count > 0 ? chart->meta.key_count : 4;
    const Rectangle content = ui::inset(rect, ui::edge_insets::symmetric(0.0f, 16.0f));
    const float source_width = status_badge_width(chart->source_status);
    const Rectangle key_chip = {content.x, content.y + 8.0f, 42.0f, 26.0f};
    const Rectangle badge_rect = {content.x + content.width - 82.0f, content.y + 7.0f, 78.0f, 28.0f};
    const Rectangle source_rect = {
        badge_rect.x - source_width - 10.0f,
        content.y + 10.0f,
        source_width,
        22.0f
    };
    constexpr float kAuthorReservedWidth = 220.0f;
    const float difficulty_x = key_chip.x + key_chip.width + 14.0f;
    const float max_difficulty_width = std::max(0.0f, source_rect.x - difficulty_x - kAuthorReservedWidth - 24.0f);
    const float measured_difficulty_width = ui::measure_text_size(chart->meta.difficulty.c_str(), 22.0f).x + 8.0f;
    const float difficulty_width = std::clamp(measured_difficulty_width, 64.0f, max_difficulty_width);
    const Rectangle difficulty_rect = {
        difficulty_x,
        content.y + 5.0f,
        difficulty_width,
        32.0f,
    };
    const float author_x = std::max(difficulty_rect.x + difficulty_rect.width + 18.0f,
                                    content.x + 210.0f);
    const Rectangle author_rect = {
        author_x,
        content.y + 10.0f,
        std::max(0.0f, source_rect.x - author_x - 10.0f),
        22.0f
    };
    const Rectangle author_prefix_rect = {author_rect.x, author_rect.y, 18.0f, author_rect.height};
    const Rectangle author_name_rect = {
        author_prefix_rect.x + author_prefix_rect.width + 8.0f,
        author_rect.y,
        std::max(0.0f, author_rect.width - author_prefix_rect.width - 8.0f),
        author_rect.height
    };

    ui::draw_rect_f(key_chip, with_alpha(lerp_color(t.section, level_color, 0.18f), scaled_alpha(alpha, 0.88f)));
    ui::draw_rect_lines(key_chip, 1.0f, with_alpha(level_color, scaled_alpha(alpha, 0.72f)));
    ui::draw_text_in_rect(TextFormat("%dK", keys), 15, key_chip,
                          with_alpha(t.text, alpha), ui::text_align::center);
    draw_square_status_badge(source_rect, chart->source_status, alpha, 10);
    draw_marquee_text(chart->meta.difficulty.c_str(), difficulty_rect,
                      22, with_alpha(t.text, alpha), GetTime());
    draw_difficulty_level_badge(chart->meta.level, badge_rect, 15, alpha);
    ui::draw_text_in_rect("by", 13, author_prefix_rect,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    draw_marquee_text(chart->meta.chart_author.empty() ? "Unknown" : chart->meta.chart_author.c_str(),
                      author_name_rect, 13, with_alpha(t.text_muted, alpha), GetTime());
}

void draw_preview_and_start_panel(const title_play_view::layout& current,
                                  const song_select::state& state,
                                  const title_preview_snapshot& preview,
                                  const song_select::song_entry* song,
                                  const song_select::chart_option* chart,
                                  unsigned char alpha,
                                  Color button_base,
                                  Color button_hover,
                                  Color button_selected,
                                  unsigned char normal_row_alpha,
                                  unsigned char hover_row_alpha,
                                  unsigned char selected_row_alpha) {
    const auto& t = *g_theme;
    const Rectangle panel = current.ranking_column;
    const Rectangle info_panel = play_info_panel_rect(panel);
    const Rectangle ranking_panel = play_ranking_panel_rect(panel);
    const Color chart_tone = chart != nullptr ? difficulty_level_color(chart->meta.level) : t.border_light;
    ui::draw_rect_f(info_panel, with_alpha(lerp_color(t.section, chart_tone, chart != nullptr ? 0.045f : 0.0f),
                                           static_cast<unsigned char>(normal_row_alpha / 2)));
    ui::draw_rect_f({info_panel.x, info_panel.y, 4.0f, info_panel.height},
                    with_alpha(chart_tone, chart != nullptr ? scaled_alpha(alpha, 0.82f) : 0));
    ui::draw_rect_f({info_panel.x + 4.0f, info_panel.y, info_panel.width - 4.0f, 2.0f},
                    with_alpha(chart_tone, chart != nullptr ? scaled_alpha(alpha, 0.18f) : 0));
    ui::draw_rect_lines(info_panel, 1.2f, with_alpha(t.border_light, scaled_alpha(alpha, 0.82f)));
    ui::draw_rect_f(ranking_panel, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
    ui::draw_rect_lines(ranking_panel, 1.2f, with_alpha(t.border_light, alpha));

    ui::draw_text_in_rect("選択中の譜面", 16,
                          {info_panel.x + 28.0f, info_panel.y + 24.0f, info_panel.width - 56.0f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    if (chart != nullptr) {
        const int keys = chart->meta.key_count > 0 ? chart->meta.key_count : 4;
        const Rectangle key_rect = {info_panel.x + 28.0f, info_panel.y + 72.0f, 78.0f, 54.0f};
        ui::draw_text_in_rect(TextFormat("%dK", keys), 33, key_rect,
                              with_alpha(t.success, alpha), ui::text_align::left);
        draw_marquee_text(chart->meta.difficulty.c_str(),
                          {key_rect.x + key_rect.width + 10.0f, key_rect.y + 2.0f, 180.0f, 48.0f},
                          32, with_alpha(t.text, alpha), GetTime());
        draw_marquee_text((std::string("by ") +
                           (chart->meta.chart_author.empty() ? "Unknown" : chart->meta.chart_author)).c_str(),
                          {key_rect.x + key_rect.width + 204.0f, key_rect.y + 17.0f, 170.0f, 22.0f},
                          14, with_alpha(t.text_muted, alpha), GetTime());
        draw_difficulty_level_badge(
            chart->meta.level,
            {info_panel.x + info_panel.width - 146.0f, info_panel.y + 82.0f, 88.0f, 28.0f},
            14, alpha);
        draw_square_status_badge(
            {info_panel.x + info_panel.width - 250.0f, info_panel.y + 84.0f, 86.0f, 24.0f},
            chart->source_status, alpha, 10);
        draw_difficulty_breakdown(
            {info_panel.x + 28.0f, info_panel.y + 164.0f, info_panel.width - 56.0f, 94.0f},
            chart, alpha, normal_row_alpha, state.chart_change_anim_t);
    } else {
        ui::draw_text_in_rect("No chart selected", 22,
                              {info_panel.x + 28.0f, info_panel.y + 86.0f, info_panel.width - 56.0f, 48.0f},
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
    }

    const Rectangle progress = current.meta_rect;
    const bool preview_loading =
        preview.audio_status == song_select::preview_audio_loader::load_status::loading;
    const double length = preview.length_seconds;
    const double pos = preview_loading ? 0.0 : state.preview_bar_dragging
        ? state.preview_bar_drag_position_seconds
        : preview.position_seconds;
    const float ratio = length > 0.0 ? std::clamp(static_cast<float>(pos / length), 0.0f, 1.0f) : 0.0f;
    const unsigned char progress_alpha = preview_loading ? scaled_alpha(alpha, 0.38f) : alpha;
    ui::draw_rect_f(progress, with_alpha(t.bg_alt, preview_loading
        ? scaled_alpha(normal_row_alpha, 0.58f)
        : normal_row_alpha));
    ui::draw_rect_f({progress.x, progress.y, progress.width * ratio, progress.height},
                    with_alpha(t.accent, progress_alpha));
    ui::draw_rect_lines(progress, 1.0f, with_alpha(t.border_light, progress_alpha));
    ui::draw_text_in_rect(
        preview_loading
            ? "loading"
            : TextFormat("%s / %s",
                         format_duration_label(static_cast<float>(pos)).c_str(),
                         length > 0.0 ? format_duration_label(static_cast<float>(length)).c_str() : "--:--"),
        12,
        {progress.x, progress.y + 11.0f, progress.width, 16.0f},
        with_alpha(t.text_muted, progress_alpha), ui::text_align::right);

    const Rectangle prev_button = preview_prev_button_rect(current);
    const Rectangle play_button = preview_play_button_rect(current);
    const Rectangle next_button = preview_next_button_rect(current);
    draw_transport_skip_button(prev_button, false, alpha);
    draw_transport_toggle_button(play_button, preview.playing, alpha);
    draw_transport_skip_button(next_button, true, alpha);

    const Rectangle mods = best_score_rect(panel);
    const bool mods_hovered = ui::is_hovered(mods);
    const bool mods_pressed = ui::is_pressed(mods);
    const Rectangle mods_visual = mods_pressed ? ui::inset(mods, 1.5f) : mods;
    const bool mods_active = state.mods.any_enabled();
    ui::draw_rect_f(mods_visual,
                    with_alpha(mods_active ? button_selected : button_base,
                               mods_hovered ? hover_row_alpha : normal_row_alpha));
    ui::draw_rect_lines(mods_visual, mods_active ? 1.6f : 1.0f,
                        with_alpha(mods_active ? t.accent : t.border_light, alpha));
    raythm_icons::draw_settings_gear(
        centered_icon_rect({mods_visual.x + 14.0f, mods_visual.y, 42.0f, mods_visual.height}, 8.0f),
        with_alpha(mods_active ? t.accent : t.text_secondary, alpha), 3.0f);
    ui::draw_text_in_rect("MODS", 18, {mods_visual.x + 62.0f, mods_visual.y + 8.0f, 112.0f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    const std::string mods_label = mod_summary(state.mods);
    ui::draw_text_in_rect(mods_label.c_str(), 14,
                          {mods_visual.x + 62.0f, mods_visual.y + 30.0f, 130.0f, 20.0f},
                          with_alpha(mods_active ? t.accent : t.text_muted, alpha), ui::text_align::left);
    ui::draw_button_colored(start_button_rect(panel), localization::tr_literal(state.filter.multiplayer_queueable_only ? "SELECT" : "START"), 22,
                            with_alpha(button_selected, selected_row_alpha),
                            with_alpha(button_hover, hover_row_alpha),
                            with_alpha(t.text, alpha), 1.4f);
}

void draw_mod_toggle(Rectangle rect,
                     const char* label,
                     const char* description,
                     bool enabled,
                     unsigned char alpha,
                     unsigned char normal_row_alpha,
                     unsigned char hover_row_alpha) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect, song_select::layout::kContextMenuLayer);
    const bool pressed = ui::is_pressed(rect, song_select::layout::kContextMenuLayer);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    const Color fill = enabled ? lerp_color(t.row_soft_selected, t.accent, 0.20f) : t.row_soft;
    ui::draw_rect_f(visual, with_alpha(fill, hovered ? hover_row_alpha : normal_row_alpha));
    ui::draw_rect_lines(visual, enabled ? 1.6f : 1.0f, with_alpha(enabled ? t.accent : t.border_light, alpha));
    ui::draw_text_in_rect(label, 18,
                          {visual.x + 14.0f, visual.y + 5.0f, visual.width - 100.0f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    ui::draw_text_in_rect(description, 12,
                          {visual.x + 14.0f, visual.y + 29.0f, visual.width - 100.0f, 18.0f},
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    const Rectangle track = {visual.x + visual.width - 70.0f, visual.y + 17.0f, 48.0f, 20.0f};
    ui::draw_rect_f(track, enabled ? with_alpha(t.accent, 160) : with_alpha(t.text_muted, 70));
    ui::draw_rect_lines(track, 1.0f, with_alpha(enabled ? t.accent : t.border_light, alpha));
    const float knob_x = enabled ? track.x + track.width - 18.0f : track.x + 2.0f;
    ui::draw_rect_f({knob_x, track.y + 2.0f, 16.0f, 16.0f},
                    with_alpha(enabled ? t.text : t.text_secondary, alpha));
}

void draw_mod_modal(const title_play_view::layout& current,
                    const song_select::state& state,
                    unsigned char alpha,
                    unsigned char normal_row_alpha,
                    unsigned char hover_row_alpha) {
    if (!state.play_mod_modal_open) {
        return;
    }
    const Rectangle modal = mod_modal_rect(current.ranking_column);
    ui::enqueue_draw_command(song_select::layout::kContextMenuLayer,
                             [modal, mods = state.mods, alpha, normal_row_alpha, hover_row_alpha]() {
        using namespace title_play_view::mod_layout;
        const auto& theme = *g_theme;
        ui::draw_rect_f(modal, with_alpha(theme.section, 242));
        ui::draw_rect_lines(modal, 1.4f, with_alpha(theme.border_light, alpha));
        const Rectangle content = {
            modal.x + kModalSidePadding,
            modal.y + kModalTopPadding,
            modal.width - kModalSidePadding * 2.0f,
            modal.height - kModalTopPadding - kModalBottomPadding,
        };
        ui::draw_text_in_rect("MOD SETTINGS", 13,
                              {content.x, content.y, content.width, kHeaderHeight},
                              with_alpha(theme.text_muted, alpha), ui::text_align::left);
        ui::draw_text_in_rect("Score submission is disabled while mods are active.", 11,
                              {content.x,
                               content.y + kHeaderHeight + kHeaderToDescriptionGap,
                               content.width,
                               kDescriptionHeight},
                              with_alpha(theme.text_muted, alpha), ui::text_align::left);
        draw_mod_toggle(auto_mod_toggle_rect(modal), "Auto", "Perfect autoplay",
                        mods.auto_play, alpha, normal_row_alpha, hover_row_alpha);
        draw_mod_toggle(no_fail_mod_toggle_rect(modal), "NoFail", "Survive gauge failure",
                        mods.no_fail, alpha, normal_row_alpha, hover_row_alpha);
    });
}

void enqueue_delete_context_menu(Rectangle menu_rect, const char* label) {
    const auto& t = *g_theme;
    const Rectangle item_rect = context_menu_item_rect(menu_rect);
    const bool hovered = ui::is_hovered(item_rect, song_select::layout::kContextMenuLayer);
    const bool pressed = ui::is_pressed(item_rect, song_select::layout::kContextMenuLayer);
    const Rectangle visual = pressed ? ui::inset(item_rect, 1.5f) : item_rect;
    const Color bg = hovered ? t.row_hover : t.row;
    const Color fill = with_alpha(lerp_color(bg, t.error, hovered ? 0.16f : 0.08f), 228);
    const Color border = with_alpha(lerp_color(t.border, t.error, 0.35f), 220);
    const std::string label_copy = label != nullptr ? label : "";

    ui::enqueue_draw_command(song_select::layout::kContextMenuLayer,
                             [menu_rect, item_rect, visual, fill, border, label_copy]() {
        ui::draw_section(menu_rect);
        ui::draw_rect_f(visual, fill);
        ui::draw_rect_lines(visual, 1.5f, border);
        const Rectangle accent = {
            visual.x + 10.0f,
            visual.y + 9.0f,
            4.0f,
            std::max(0.0f, visual.height - 18.0f),
        };
        ui::draw_rect_f(accent, with_alpha(g_theme->error, 220));
        ui::draw_text_in_rect(label_copy.c_str(), 16,
                              {visual.x + 24.0f, visual.y, visual.width - 34.0f, visual.height},
                              g_theme->text, ui::text_align::left);
    });
}

Rectangle centered_scaled_rect(Rectangle anchor, Rectangle target, float scale, Vector2 offset = {0.0f, 0.0f}) {
    const Vector2 center = {
        anchor.x + anchor.width * 0.5f + offset.x,
        anchor.y + anchor.height * 0.5f + offset.y,
    };
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {
        center.x - width * 0.5f,
        center.y - height * 0.5f,
        width,
        height,
    };
}

Rectangle fallback_origin_rect() {
    return kFallbackOriginRect;
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}
layout make_layout_for_targets(float anim_t,
                               Rectangle origin_rect,
                               Rectangle song_rect,
                               Rectangle main_rect,
                               Rectangle ranking_rect,
                               Rectangle jacket_rect,
                               Rectangle meta_rect,
                               Rectangle chart_detail_rect,
                               Rectangle chart_buttons_rect,
                               Rectangle ranking_header_rect,
                               Rectangle ranking_source_local_rect,
                               Rectangle ranking_source_online_rect,
                               Rectangle ranking_list_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);

    const Rectangle seed_song = centered_scaled_rect(origin, song_rect, 0.68f, kSeedSongOffset);
    const Rectangle seed_main = centered_scaled_rect(origin, main_rect, 0.74f, kSeedMainOffset);
    const Rectangle seed_ranking = centered_scaled_rect(origin, ranking_rect, 0.68f, kSeedRankingOffset);
    const Rectangle seed_back = centered_scaled_rect(origin, kPlayBackButtonRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_jacket = centered_scaled_rect(origin, jacket_rect, 0.82f, kSeedJacketOffset);
    const Rectangle seed_meta = centered_scaled_rect(origin, meta_rect, 0.8f, kSeedMetaOffset);
    const Rectangle seed_chart_detail = centered_scaled_rect(origin, chart_detail_rect, 0.76f, kSeedChartDetailOffset);
    const Rectangle seed_chart_buttons = centered_scaled_rect(origin, chart_buttons_rect, 0.88f, kSeedChartButtonsOffset);
    const Rectangle seed_ranking_header = centered_scaled_rect(origin, ranking_header_rect, 0.7f, kSeedRankingHeaderOffset);
    const Rectangle seed_ranking_source_local = centered_scaled_rect(origin, ranking_source_local_rect, 0.8f, kSeedRankingSourceLocalOffset);
    const Rectangle seed_ranking_source_online = centered_scaled_rect(origin, ranking_source_online_rect, 0.8f, kSeedRankingSourceOnlineOffset);
    const Rectangle seed_ranking_list = centered_scaled_rect(origin, ranking_list_rect, 0.7f, kSeedRankingListOffset);

    return {
        tween::lerp(seed_back, kPlayBackButtonRect, t),
        tween::lerp(seed_song, song_rect, t),
        tween::lerp(seed_main, main_rect, t),
        tween::lerp(seed_ranking, ranking_rect, t),
        tween::lerp(seed_jacket, jacket_rect, t),
        tween::lerp(seed_meta, meta_rect, t),
        tween::lerp(seed_chart_detail, chart_detail_rect, t),
        tween::lerp(seed_chart_buttons, chart_buttons_rect, t),
        tween::lerp(seed_ranking_header, ranking_header_rect, t),
        tween::lerp(seed_ranking_source_local, ranking_source_local_rect, t),
        tween::lerp(seed_ranking_source_online, ranking_source_online_rect, t),
        tween::lerp(seed_ranking_list, ranking_list_rect, t),
    };
}

layout build_mode_layout(float anim_t, Rectangle origin_rect, mode view_mode) {
    const bool play = view_mode == mode::play;
    return make_layout_for_targets(
        anim_t,
        origin_rect,
        play ? kPlaySongColumnRect : kCreateSongColumnRect,
        play ? kPlayMainColumnRect : kCreateMainColumnRect,
        play ? kPlayRankingColumnRect : kCreateRankingColumnRect,
        play ? kPlayJacketRect : kCreateJacketRect,
        play ? kPlayMetaRect : kCreateMetaRect,
        play ? kPlayChartDetailRect : kCreateChartDetailRect,
        play ? kPlayChartButtonsRect : kCreateChartButtonsRect,
        play ? kPlayRankingHeaderRect : kCreateRankingHeaderRect,
        play ? kPlayRankingSourceLocalRect : kCreateRankingSourceLocalRect,
        play ? kPlayRankingSourceOnlineRect : kCreateRankingSourceOnlineRect,
        play ? kPlayRankingListRect : kCreateRankingListRect);
}

}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    return make_mode_layout(anim_t, origin_rect, mode::play);
}

layout make_mode_layout(float anim_t, Rectangle origin_rect, mode view_mode) {
    return build_mode_layout(anim_t, origin_rect, view_mode);
}

void draw(song_select::state& state,
          const title_audio_controller& audio_controller,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect,
          const title_create_tools_model::view_model* create_tools_model,
          song_select::ranking_load_controller::load_status ranking_status) {
    const auto& t = *g_theme;
    const float play_t = tween::ease_out_cubic(anim_t);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_mode_layout(anim_t, origin_rect, view_mode);
    const float content_fade_t = std::clamp((play_t - 0.18f) / 0.62f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    const bool hide_unloaded_content =
        state.catalog_loading && !state.catalog_loaded_once && state.songs.empty();
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const Color button_selected_hover = t.row_soft_selected_hover;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);
    const unsigned char selected_hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_hover_alpha) / 255);
    ui::draw_button_colored(current.back_rect, "戻る", 16,
                            with_alpha(button_base, normal_row_alpha), with_alpha(button_hover, hover_row_alpha), with_alpha(t.text, alpha), 1.5f);

    if (view_mode == mode::play) {
        draw_play_filter_panel(current, state, alpha, button_base, button_selected, normal_row_alpha, selected_row_alpha);
        ui::draw_rect_f(current.main_column, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
        ui::draw_rect_lines(current.main_column, 1.2f, with_alpha(t.border_light, alpha));
    } else {
        ui::draw_line_ex({current.song_column.x + current.song_column.width + 22.0f, current.song_column.y + 18.0f},
                         {current.song_column.x + current.song_column.width + 22.0f, current.song_column.y + current.song_column.height - 18.0f},
                         1.2f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));
        ui::draw_line_ex({current.ranking_column.x - 24.0f, current.ranking_column.y + 24.0f},
                         {current.ranking_column.x - 24.0f, current.ranking_column.y + current.ranking_column.height - 20.0f},
                         1.2f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));
    }

    if (!hide_unloaded_content) {
        const Rectangle song_list_rect =
            view_mode == mode::play
                ? Rectangle{current.song_column.x + 18.0f, current.song_column.y + 128.0f,
                            current.song_column.width - 36.0f, current.song_column.height - 146.0f}
                : current.song_column;
        title_song_list_view::draw(state, {
            .column_rect = song_list_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_selected = button_selected,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .expanded_cards = view_mode == mode::play,
            .show_header = view_mode != mode::play,
            .embedded_chart_scroll_y = 0.0f,
            .now = now,
        });
    }

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    const title_preview_snapshot preview = audio_controller.preview_snapshot(song);

    if (!hide_unloaded_content) {
        const Rectangle center_jacket =
            view_mode == mode::play
                ? Rectangle{current.main_column.x + 28.0f, current.main_column.y + 58.0f, 248.0f, 248.0f}
                : current.jacket_rect;
        const Rectangle center_detail =
            view_mode == mode::play
                ? Rectangle{center_jacket.x + center_jacket.width + 34.0f, current.main_column.y + 54.0f,
                            current.main_column.x + current.main_column.width - center_jacket.x -
                                center_jacket.width - 62.0f,
                            150.0f}
                : current.chart_detail_rect;
        title_center_view::draw(state, preview, song, chart, filtered, {
            .main_column_rect = current.main_column,
            .jacket_rect = center_jacket,
            .chart_detail_rect = center_detail,
            .chart_buttons_rect = current.chart_buttons_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_selected = button_selected,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .compact_song_header = view_mode == mode::play,
            .now = now,
        });
    }

    if (view_mode == mode::play) {
        if (hide_unloaded_content) {
            return;
        }
        ui::draw_rect_f({current.main_column.x + 24.0f, current.main_column.y + 342.0f,
                         current.main_column.width - 48.0f, 1.0f},
                        with_alpha(t.border_light, alpha));
        draw_preview_and_start_panel(current, state, preview, song, chart, alpha, button_base,
                                     button_hover, button_selected, normal_row_alpha, hover_row_alpha,
                                     selected_row_alpha);
        title_ranking_view::draw(state.ranking_panel, {
            .header_rect = current.ranking_header_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_hover = button_hover,
            .button_selected = button_selected,
            .button_selected_hover = button_selected_hover,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .selected_hover_row_alpha = selected_hover_row_alpha,
            .ranking_status = ranking_status,
            .self_player_display_name = state.auth.display_name,
            .avatar_base_url = state.auth.server_url,
        });
    } else {
        const title_create_tools_model::view_model empty_create_tools;
        title_create_tools_view::draw(create_tools_model != nullptr ? *create_tools_model : empty_create_tools, {
            .current = current,
            .alpha = alpha,
            .button_base = button_base,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
        });
    }

    if (view_mode == mode::play) {
        draw_play_filter_modal(current, state, alpha, button_base, button_selected, normal_row_alpha, selected_row_alpha);
        draw_mod_modal(current, state, alpha, normal_row_alpha, hover_row_alpha);
    }

    if (view_mode == mode::play &&
        state.context_menu.open &&
        (state.context_menu.target == song_select::context_menu_target::song ||
         state.context_menu.target == song_select::context_menu_target::chart)) {
        enqueue_delete_context_menu(
            state.context_menu.rect,
            state.context_menu.target == song_select::context_menu_target::chart
                ? "DELETE CHART"
                : "DELETE SONG");
    }

}

}  // namespace title_play_view
