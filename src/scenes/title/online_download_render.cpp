#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "platform/windows_input_source.h"
#include "scene_common.h"
#include "tween.h"
#include "title/title_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace title_online_view {
namespace {

Rectangle song_card_jacket_rect(Rectangle card) {
    const float jacket_size = std::min(card.height - 42.0f, 96.0f);
    return {
        card.x + 16.0f,
        card.y + 30.0f,
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

std::string song_subtitle(const song_meta& meta) {
    const std::string genres = genre_summary(meta);
    if (genres.empty()) {
        return meta.artist;
    }
    return meta.artist + " / " + genres;
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

Rectangle sidebar_button_rect(Rectangle sidebar, int index) {
    constexpr float kSidebarXInset = 14.0f;
    constexpr float kSidebarDiscoveryTitleY = 32.0f;
    constexpr float kSidebarTitleToButtonGap = 42.0f;
    constexpr float kSidebarButtonHeight = 54.0f;
    constexpr float kSidebarButtonGap = 8.0f;
    return {
        sidebar.x + kSidebarXInset,
        sidebar.y + kSidebarDiscoveryTitleY + kSidebarTitleToButtonGap +
            static_cast<float>(index) * (kSidebarButtonHeight + kSidebarButtonGap),
        sidebar.width - kSidebarXInset * 2.0f,
        kSidebarButtonHeight,
    };
}

Rectangle source_button_rect(Rectangle sidebar, int index) {
    constexpr float kSidebarXInset = 14.0f;
    constexpr float kSidebarButtonHeight = 54.0f;
    constexpr float kSidebarButtonGap = 8.0f;
    constexpr float kSourceTitleY = 520.0f;
    constexpr float kSidebarTitleToButtonGap = 42.0f;
    return {
        sidebar.x + kSidebarXInset,
        sidebar.y + kSourceTitleY + kSidebarTitleToButtonGap +
            static_cast<float>(index) * (kSidebarButtonHeight + kSidebarButtonGap),
        sidebar.width - kSidebarXInset * 2.0f,
        kSidebarButtonHeight,
    };
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
    constexpr float kPreviewPlayY = 438.0f;
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
    constexpr float kPreviewPlayY = 438.0f;
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
    constexpr float kPreviewPlayY = 438.0f;
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
    constexpr float kPreviewBarY = 506.0f;
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
    const bool show_label = !state.active && state.value.empty();
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
        ui::draw_text_in_rect(label, font_size, label_rect,
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
        draw_marquee_text(display_value.c_str(), text_rect.x, text_y, font_size, text_color,
                          text_rect.width, GetTime());
    } else if (!state.active) {
        ui::draw_text_f(display_value.c_str(), text_rect.x, text_y, font_size, text_color);
    } else {
        ui::begin_scissor_rect(text_rect);

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

        ui::draw_text_f(state.value.c_str(), text_rect.x - state.scroll_x, text_y, font_size, with_alpha(t.text, alpha));

        const double blink = GetTime() * 1.6;
        if (std::fmod(blink, 1.0) < 0.6) {
            const float cursor_x = text_rect.x +
                                   ui::text_input_prefix_width(state.value, state.cursor, font_size) -
                                   state.scroll_x;
            ui::draw_rect_span({cursor_x, cursor_y, 1.5f, cursor_height},
                               with_alpha(t.text, alpha));
        }

        EndScissorMode();
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
                     {right, cy + tri_h * 0.5f},
                     {left, cy},
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
    const float top = visual.y + 6.0f;
    const float mid = visual.y + 14.0f;
    const float bottom = visual.y + visual.height - 7.0f;
    DrawLineEx({cx, top}, {cx, mid}, 2.0f, stroke);
    DrawTriangle({cx - 6.0f, mid - 1.0f},
                 {cx + 6.0f, mid - 1.0f},
                 {cx, mid + 6.0f},
                 stroke);
    DrawLineEx({cx - 8.0f, bottom}, {cx + 8.0f, bottom}, 2.0f, stroke);
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
    draw_song_search_input(current.search_rect, state.search_input, "SEARCH", "songs / artists / tags",
                           16, 64,
                           button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha,
                           alpha);

    ui::draw_rect_f(current.sidebar_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
    ui::draw_rect_lines(current.sidebar_rect, 1.2f, with_alpha(t.border_light, alpha));
    constexpr float kSidebarTitleY = 32.0f;
    constexpr float kSourceTitleY = 520.0f;
    constexpr float kSidebarDividerY = 488.0f;
    ui::draw_text_in_rect("DISCOVERY", 15,
                          {current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSidebarTitleY,
                           current.sidebar_rect.width - 48.0f, 20.0f},
                          with_alpha(t.accent, alpha), ui::text_align::left);
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
        ui::draw_button_colored(sidebar_button_rect(current.sidebar_rect, index),
                                view_label(views[index]), 14,
                                with_alpha(active ? button_selected : button_base,
                                           active ? selected_row_alpha : normal_row_alpha),
                                with_alpha(active ? button_selected : button_hover,
                                           active ? selected_row_alpha : hover_row_alpha),
                                with_alpha(active ? t.text : t.text_secondary, alpha), 1.2f);
    }
    ui::draw_line_ex({current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSidebarDividerY},
                     {current.sidebar_rect.x + current.sidebar_rect.width - 24.0f, current.sidebar_rect.y + kSidebarDividerY},
                     1.2f, with_alpha(t.border_active, alpha));
    ui::draw_text_in_rect("SOURCE", 15,
                          {current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSourceTitleY,
                           current.sidebar_rect.width - 48.0f, 20.0f},
                          with_alpha(t.accent, alpha), ui::text_align::left);
    const source_filter sources[] = {
        source_filter::all,
        source_filter::official,
        source_filter::community,
    };
    for (int index = 0; index < 3; ++index) {
        const bool active = state.source == sources[index];
        ui::draw_button_colored(source_button_rect(current.sidebar_rect, index),
                                source_label(sources[index]), 14,
                                with_alpha(active ? button_selected : button_base,
                                           active ? selected_row_alpha : normal_row_alpha),
                                with_alpha(active ? button_selected : button_hover,
                                           active ? selected_row_alpha : hover_row_alpha),
                                with_alpha(active ? t.text : t.text_secondary, alpha), 1.2f);
    }

    ui::draw_rect_f(current.content_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
    ui::draw_rect_lines(current.content_rect, 1.2f, with_alpha(t.border_light, alpha));
    const std::string content_caption = state.view == discovery_view::overview
        ? "Overview"
        : view_label(state.view);
    ui::draw_text_in_rect(content_caption.c_str(), 17,
                          {current.content_rect.x + 12.0f, current.content_rect.y + 6.0f,
                           current.content_rect.width * 0.52f, 20.0f},
                          with_alpha(loading ? t.text : t.text_secondary, grid_alpha), ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())),
                          14,
                          {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                           current.content_rect.width * 0.5f - 12.0f, 16.0f},
                          with_alpha(t.text_muted, grid_alpha), ui::text_align::right);
    ui::draw_text_in_rect("Press Esc to return to the grid",
                          14,
                          {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                           current.content_rect.width * 0.5f - 12.0f, 16.0f},
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);

    Rectangle source_jacket_rect = current.hero_jacket_rect;
    bool selected_card_drawn = false;
    std::vector<std::string> shelf_headers_for_song;
    shelf_headers_for_song.reserve(indices.size());
    if (state.view == discovery_view::overview && state.search_input.value.empty()) {
        for (const discovery_shelf_state& shelf : state.discovery_shelves) {
            bool first_in_shelf = true;
            for (const song_entry_state& shelf_song : shelf.songs) {
                const auto it = std::find_if(indices.begin(), indices.end(), [&](int song_index) {
                    return song_index >= 0 && song_index < static_cast<int>(songs.size()) &&
                           songs[static_cast<size_t>(song_index)].song.song.meta.song_id ==
                               shelf_song.song.song.meta.song_id;
                });
                if (it == indices.end()) {
                    continue;
                }
                const size_t display_index = static_cast<size_t>(it - indices.begin());
                if (shelf_headers_for_song.size() <= display_index) {
                    shelf_headers_for_song.resize(display_index + 1);
                }
                if (first_in_shelf) {
                    shelf_headers_for_song[display_index] = shelf.title;
                    first_in_shelf = false;
                }
            }
        }
    }
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
            ui::draw_text_in_rect(empty_title,
                                  26, {placeholder.x, placeholder.y + 8.0f, placeholder.width, 28.0f},
                                  with_alpha(t.text, grid_alpha), ui::text_align::center);
            if (!loading && state.catalog_request_failed) {
                const std::string detail = !state.catalog_status_message.empty()
                    ? state.catalog_status_message
                    : state.catalog_maintenance
                        ? "Online features are temporarily unavailable. Please try again later."
                        : "Check the server URL and confirm raythm-Server is running.";
                ui::draw_text_in_rect(detail.c_str(),
                                      14, {placeholder.x + 20.0f, placeholder.y + 42.0f, placeholder.width - 40.0f, 16.0f},
                                      with_alpha(t.text_muted, grid_alpha), ui::text_align::center);
                if (!state.catalog_server_url.empty()) {
                    const std::string server_label = "Tried: " + state.catalog_server_url;
                    ui::draw_text_in_rect(server_label.c_str(),
                                          12, {placeholder.x + 20.0f, placeholder.y + 58.0f, placeholder.width - 40.0f, 14.0f},
                                          with_alpha(t.text_hint, grid_alpha), ui::text_align::center);
                }
            }
        }

        for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
            const int song_index = indices[static_cast<size_t>(display_index)];
            const song_entry_state& song = songs[static_cast<size_t>(song_index)];
            const Rectangle card = detail::song_row_rect(current.song_grid_rect, display_index, state.song_scroll_y);
            if (card.y + card.height < current.song_grid_rect.y - 4.0f ||
                card.y > current.song_grid_rect.y + current.song_grid_rect.height + 4.0f) {
                continue;
            }
            if (static_cast<size_t>(display_index) < shelf_headers_for_song.size() &&
                !shelf_headers_for_song[static_cast<size_t>(display_index)].empty()) {
                ui::draw_text_in_rect(shelf_headers_for_song[static_cast<size_t>(display_index)].c_str(),
                                      15,
                                      {card.x + 10.0f, card.y - 28.0f, card.width - 20.0f, 20.0f},
                                      with_alpha(t.text, grid_alpha), ui::text_align::left);
            }

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
                    ui::draw_text_in_rect("JACKET", 18, jacket_rect, with_alpha(t.text_muted, placeholder_alpha),
                                          ui::text_align::center);
                }
                ui::draw_rect_lines(jacket_rect, 1.0f, with_alpha(t.border_image, grid_alpha));
            }

            const std::string badge_label = detail::song_status_label(song);
            if (!badge_label.empty()) {
                const Rectangle badge_rect = {card.x + card.width - 90.0f, card.y + 12.0f, 72.0f, 18.0f};
                ui::draw_text_in_rect(badge_label.c_str(), 12, badge_rect,
                                      with_alpha(detail::song_status_color(song), grid_alpha), ui::text_align::right);
            }

            draw_marquee_text(song.song.song.meta.title.c_str(),
                              {jacket_rect.x + jacket_rect.width + 16.0f, card.y + 24.0f,
                               card.width - jacket_rect.width - 48.0f, 28.0f},
                              18, with_alpha(t.text, grid_alpha), now);
            const std::string card_subtitle = song_subtitle(song.song.song.meta);
            draw_marquee_text(card_subtitle.c_str(),
                              {jacket_rect.x + jacket_rect.width + 16.0f, card.y + 52.0f,
                               card.width - jacket_rect.width - 48.0f, 22.0f},
                              13, with_alpha(t.text_muted, grid_alpha), now);
            const std::string first_genre = genre_summary(song.song.song.meta);
            if (!first_genre.empty()) {
                ui::draw_text_in_rect(first_genre.c_str(), 12,
                                      {jacket_rect.x + jacket_rect.width + 16.0f, card.y + 82.0f,
                                       card.width - jacket_rect.width - 48.0f, 18.0f},
                                      with_alpha(t.accent, grid_alpha), ui::text_align::left);
            }
            ui::draw_text_in_rect(TextFormat("charts %d", std::max(song.song.song.meta.chart_count,
                                                                    static_cast<int>(song.charts.size()))),
                                  12,
                                  {jacket_rect.x + jacket_rect.width + 16.0f, card.y + card.height - 28.0f,
                                   120.0f, 16.0f},
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
                song_card_jacket_rect(detail::song_row_rect(current.song_grid_rect, display_index, state.song_scroll_y));
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
            ui::draw_text_in_rect("JACKET", 24, jacket_rect, with_alpha(t.text_muted, alpha), ui::text_align::center);
        }
        ui::draw_rect_lines(jacket_rect, 1.4f, with_alpha(t.border_image, alpha));

        draw_marquee_text(song->song.song.meta.title.c_str(),
                          {current.preview_panel_rect.x + 26.0f, jacket_rect.y + jacket_rect.height + 24.0f,
                           current.preview_panel_rect.width - 52.0f, 38.0f},
                          25, with_alpha(t.text, alpha), now);
        draw_marquee_text(song->song.song.meta.artist.c_str(),
                          {current.preview_panel_rect.x + 26.0f, jacket_rect.y + jacket_rect.height + 62.0f,
                           current.preview_panel_rect.width - 52.0f, 26.0f},
                          17, with_alpha(t.text_secondary, alpha), now);

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
        ui::draw_text_in_rect(
            TextFormat("%s / %s",
                       detail::format_time_label(preview_position).c_str(),
                       preview_length > 0.0 ? detail::format_time_label(preview_length).c_str() : "--:--"),
            12,
            {bar.x, bar.y + 16.0f, bar.width, 18.0f},
            with_alpha(t.text_muted, alpha), ui::text_align::right);

        ui::draw_text_in_rect("GENRES", 13,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 42.0f, 140.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
        draw_marquee_text(genre_summary(song->song.song.meta).c_str(),
                          {current.preview_panel_rect.x + 24.0f, bar.y + 64.0f,
                           current.preview_panel_rect.width - 48.0f, 22.0f},
                          14, with_alpha(t.text, alpha), now);
        ui::draw_text_in_rect("KEYWORDS", 13,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 98.0f, 140.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
        draw_marquee_text(join_labels(song->song.song.meta.keywords, " / ").c_str(),
                          {current.preview_panel_rect.x + 24.0f, bar.y + 120.0f,
                           current.preview_panel_rect.width - 48.0f, 22.0f},
                          14, with_alpha(t.text, alpha), now);
        ui::draw_text_in_rect(TextFormat("BPM %.0f", song->song.song.meta.base_bpm), 14,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 164.0f,
                               current.preview_panel_rect.width - 48.0f, 18.0f},
                              with_alpha(t.text, alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("charts %d", std::max(song->song.song.meta.chart_count,
                                                               static_cast<int>(song->charts.size()))),
                              14,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 188.0f,
                               current.preview_panel_rect.width - 48.0f, 18.0f},
                              with_alpha(t.text, alpha), ui::text_align::left);
        draw_toned_button(preview_open_button_rect(current.preview_panel_rect), "OPEN SONG", 16,
                          t.accent, alpha, selected_row_alpha, hover_row_alpha);
        return;
    }

    const Rectangle animated_jacket_rect = tween::lerp(source_jacket_rect, current.hero_jacket_rect, jacket_t);

    const float detail_left_right = current.detail_left_rect.x + current.detail_left_rect.width;
    const float separator_x = detail_left_right + (current.detail_right_rect.x - detail_left_right) * 0.5f;
    ui::draw_line_ex({separator_x, current.detail_left_rect.y + 8.0f},
                     {separator_x, current.detail_left_rect.y + current.detail_left_rect.height - 8.0f},
                     1.4f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * detail_content_t)));

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
                ui::draw_text_in_rect("JACKET", 26, animated_jacket_rect,
                                      with_alpha(t.text_muted, placeholder_alpha), ui::text_align::center);
            }
        }
        ui::draw_rect_lines(animated_jacket_rect, 1.5f, with_alpha(t.border_image, alpha));
    }

    if (detail_alpha == 0) {
        return;
    }

    const Rectangle title_rect = {
        current.detail_left_rect.x + 12.0f,
        current.hero_jacket_rect.y + current.hero_jacket_rect.height + 12.0f,
        current.detail_left_rect.width - 24.0f,
        42.0f
    };
    const Rectangle artist_rect = {
        title_rect.x,
        title_rect.y + 40.0f,
        title_rect.width,
        27.0f
    };
    draw_marquee_text(song->song.song.meta.title.c_str(), title_rect, 28, with_alpha(t.text, detail_alpha), now);
    const std::string detail_subtitle = song_subtitle(song->song.song.meta);
    draw_marquee_text(detail_subtitle.c_str(), artist_rect, 17,
                      with_alpha(t.text_secondary, detail_alpha), now);
    std::string meta_line;
    if (!song->song.song.meta.keywords.empty()) {
        meta_line += join_labels(song->song.song.meta.keywords, " / ");
    }
    if (!meta_line.empty()) {
        draw_marquee_text(meta_line.c_str(),
                          {artist_rect.x, artist_rect.y + 26.0f, artist_rect.width, 20.0f},
                          14, with_alpha(t.text_muted, detail_alpha), now);
    }

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
    ui::draw_text_in_rect(
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
        ui::draw_text_in_rect(TextFormat("%d%%", static_cast<int>(std::round(progress_ratio * 100.0f))),
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
        current.chart_list_rect.x,
        current.chart_list_rect.y - 42.0f,
        current.chart_list_rect.width * 0.54f,
        34.0f,
    };
    ui::draw_text_in_rect("CHARTS", 15,
                          {current.chart_list_rect.x, current.chart_list_rect.y - 112.0f,
                           120.0f, 20.0f},
                          with_alpha(t.text, detail_alpha), ui::text_align::left);
    const char* filter_labels[] = {"ALL", "4K", "6K", "ANY", "LOCAL", "GET"};
    for (int index = 0; index < 6; ++index) {
        const bool active = (index == 0 && state.chart_key_filter == 0) ||
            (index == 1 && state.chart_key_filter == 4) ||
            (index == 2 && state.chart_key_filter == 6) ||
            (index == 3 && state.chart_download_filter == 0) ||
            (index == 4 && state.chart_download_filter == 1) ||
            (index == 5 && state.chart_download_filter == 2);
        ui::draw_button_colored(chart_filter_button_rect(current.chart_list_rect, index),
                                filter_labels[index], 11,
                                with_alpha(active ? button_selected : button_base,
                                           active ? selected_row_alpha : normal_row_alpha),
                                with_alpha(active ? button_selected : button_hover,
                                           active ? selected_row_alpha : hover_row_alpha),
                                with_alpha(active ? t.text : t.text_secondary, detail_alpha), 1.0f);
    }
    draw_song_search_input(chart_search_rect, state.chart_search_input,
                           "CHART", "difficulty / author / level / bpm",
                           13, 80,
                           button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha,
                           detail_alpha);
    draw_song_search_input({chart_search_rect.x + chart_search_rect.width + 12.0f, chart_search_rect.y,
                            82.0f, chart_search_rect.height},
                           state.min_level_input, "MIN", "lv",
                           12, 5, button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha, detail_alpha);
    draw_song_search_input({chart_search_rect.x + chart_search_rect.width + 100.0f, chart_search_rect.y,
                            82.0f, chart_search_rect.height},
                           state.max_level_input, "MAX", "lv",
                           12, 5, button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha, detail_alpha);
    draw_song_search_input({chart_search_rect.x + chart_search_rect.width + 188.0f, chart_search_rect.y,
                            82.0f, chart_search_rect.height},
                           state.min_bpm_input, "MIN", "bpm",
                           12, 5, button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha, detail_alpha);
    draw_song_search_input({chart_search_rect.x + chart_search_rect.width + 276.0f, chart_search_rect.y,
                            82.0f, chart_search_rect.height},
                           state.max_bpm_input, "MAX", "bpm",
                           12, 5, button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha, detail_alpha);
    const auto chart_indices = detail::filtered_chart_indices(state);
    const int visible_chart_count = song->song.song.meta.chart_count > 0
        ? std::max(song->song.song.meta.chart_count, static_cast<int>(song->charts.size()))
        : static_cast<int>(song->charts.size());
    const int filtered_chart_count = static_cast<int>(chart_indices.size());
    ui::draw_text_in_rect(
        state.chart_search_input.value.empty()
            ? TextFormat("%d items", visible_chart_count)
            : TextFormat("%d / %d items", filtered_chart_count, visible_chart_count),
        14,
                          {current.chart_list_rect.x + current.chart_list_rect.width * 0.46f, current.chart_list_rect.y - 26.0f,
                           current.chart_list_rect.width * 0.54f, 16.0f},
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
        ui::draw_text_in_rect(chart_empty, 28, placeholder, with_alpha(t.text, detail_alpha), ui::text_align::center);
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

        ui::draw_text_in_rect(
            TextFormat("%s  %s", detail::key_mode_label(item.chart.meta.key_count).c_str(),
                       item.chart.meta.difficulty.c_str()),
            16,
            {card.x + 14.0f, card.y + 12.0f, card.width - 110.0f, 18.0f},
            with_alpha(detail::key_mode_color(item.chart.meta.key_count), detail_alpha), ui::text_align::left);
        const std::string chart_badge = detail::chart_status_label(item);
        const bool can_download_chart = !state.download_in_progress && detail::can_download_chart(*song, item);
        const float badge_right_padding = can_download_chart ? 54.0f : 14.0f;
        if (!chart_badge.empty()) {
            ui::draw_text_in_rect(chart_badge.c_str(), 12,
                                  {card.x + card.width - badge_right_padding - 72.0f,
                                   card.y + 14.0f, 72.0f, 14.0f},
                                  with_alpha(item.update_available ? t.accent : t.text_muted, detail_alpha),
                                  ui::text_align::right);
        }
        if (can_download_chart) {
            draw_download_icon_button(detail::chart_download_icon_rect(card),
                                      item.update_available,
                                      detail_alpha);
        }
        ui::draw_text_in_rect(TextFormat("Lv.%.1f", item.chart.meta.level), 20,
                              {card.x + 14.0f, card.y + 38.0f, 96.0f, 22.0f},
                              with_alpha(t.text, detail_alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%d Notes", item.chart.note_count), 13,
                              {card.x + 14.0f, card.y + 64.0f, card.width * 0.45f, 14.0f},
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
        ui::draw_text_in_rect(
            TextFormat("BPM %s", detail::format_bpm_range(item.chart.min_bpm, item.chart.max_bpm).c_str()),
            13,
            {card.x + card.width * 0.45f, card.y + 64.0f, card.width * 0.55f - 14.0f, 14.0f},
            with_alpha(t.text_muted, detail_alpha), ui::text_align::right);
    }
}

}  // namespace title_online_view
