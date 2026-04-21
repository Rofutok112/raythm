#include "title/online_download_view.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "audio_manager.h"
#include "app_paths.h"
#include "chart_difficulty.h"
#include "path_utils.h"
#include "scene_common.h"
#include "song_loader.h"
#include "title/title_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_online_view {
namespace {

constexpr Rectangle kBackRect = {48.0f, 38.0f, 98.0f, 38.0f};
constexpr Rectangle kOfficialTabRect = {186.0f, 40.0f, 128.0f, 38.0f};
constexpr Rectangle kCommunityTabRect = {322.0f, 40.0f, 146.0f, 38.0f};
constexpr Rectangle kOwnedTabRect = {476.0f, 40.0f, 108.0f, 38.0f};
constexpr Rectangle kContentRect = {60.0f, 98.0f, 1160.0f, 568.0f};
constexpr Rectangle kSongGridRect = {72.0f, 154.0f, 1136.0f, 488.0f};
constexpr Rectangle kDetailLeftRect = {72.0f, 108.0f, 336.0f, 548.0f};
constexpr Rectangle kDetailRightRect = {470.0f, 108.0f, 738.0f, 548.0f};
constexpr Rectangle kHeroJacketRect = {92.0f, 174.0f, 270.0f, 270.0f};
constexpr Rectangle kPreviewBarRect = {92.0f, 505.0f, 270.0f, 8.0f};
constexpr Rectangle kPreviewPlayRect = {92.0f, 529.0f, 126.0f, 40.0f};
constexpr Rectangle kPreviewStopRect = {236.0f, 576.0f, 126.0f, 40.0f};
constexpr Rectangle kPrimaryActionRect = {92.0f, 577.0f, 126.0f, 40.0f};
constexpr Rectangle kOpenLocalRect = {236.0f, 624.0f, 126.0f, 40.0f};
constexpr Rectangle kChartListRect = {500.0f, 130.0f, 680.0f, 494.0f};

constexpr int kSongGridColumns = 4;
constexpr float kSongCardHeight = 164.0f;
constexpr float kSongGridGapX = 18.0f;
constexpr float kSongGridGapY = 18.0f;
constexpr int kChartGridColumns = 2;
constexpr float kChartCardHeight = 92.0f;
constexpr float kChartGridGapX = 22.0f;
constexpr float kChartGridGapY = 18.0f;
constexpr float kControlButtonGap = kPreviewStopRect.x - (kPreviewPlayRect.x + kPreviewPlayRect.width);
constexpr float kPreviewBarBottomInset = (kContentRect.y + kContentRect.height) - kPreviewBarRect.y;
constexpr float kPreviewButtonBottomInset = (kContentRect.y + kContentRect.height) - kPreviewPlayRect.y;
constexpr float kActionButtonBottomInset = (kContentRect.y + kContentRect.height) - kPrimaryActionRect.y;

float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

float lerp_value(float from, float to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return from + (to - from) * clamped;
}

Rectangle lerp_rect(Rectangle from, Rectangle to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return {
        lerp_value(from.x, to.x, clamped),
        lerp_value(from.y, to.y, clamped),
        lerp_value(from.width, to.width, clamped),
        lerp_value(from.height, to.height, clamped),
    };
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
    return {
        static_cast<float>(kScreenWidth) * 0.5f + 120.0f,
        376.0f,
        160.0f,
        60.0f,
    };
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool contains_case_insensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return to_lower_ascii(haystack).find(to_lower_ascii(needle)) != std::string::npos;
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
    DrawRectangleRec(visual, with_alpha(state.active ? button_selected : button_base, row_alpha));
    DrawRectangleLinesEx(visual, 1.2f,
                         with_alpha(state.active ? t.border_active : t.border_light, alpha));

    const Rectangle content_rect = ui::inset(visual, ui::edge_insets::symmetric(0.0f, 14.0f));
    constexpr float kLabelWidth = 82.0f;
    const Rectangle label_rect = {content_rect.x, content_rect.y, kLabelWidth, content_rect.height};
    const Rectangle text_rect = {
        content_rect.x + kLabelWidth,
        content_rect.y,
        std::max(0.0f, content_rect.width - kLabelWidth),
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

    ui::draw_text_in_rect(label, font_size, label_rect,
                          with_alpha(state.active ? t.text : t.text_secondary, alpha), ui::text_align::left);

    std::string display_value = state.value;
    if (display_value.empty() && !state.active && placeholder != nullptr) {
        display_value = placeholder;
    }

    const Color text_color = with_alpha(state.value.empty() && !state.active ? t.text_hint : t.text, alpha);
    const float text_y = text_rect.y + (text_rect.height - static_cast<float>(font_size)) * 0.5f + 1.5f;
    const float selection_y = text_y - 1.0f;
    const float selection_height = static_cast<float>(font_size) + 4.0f;
    const float cursor_y = text_y + 1.0f;
    const float cursor_height = std::max(12.0f, static_cast<float>(font_size) - 3.0f);

    if (!state.active && !state.value.empty()) {
        draw_marquee_text(display_value.c_str(), text_rect.x, text_y, font_size, text_color,
                          text_rect.width, GetTime());
    } else if (!state.active) {
        ui::draw_text_in_rect(display_value.c_str(), font_size,
                              {text_rect.x, text_rect.y + 1.5f, text_rect.width, text_rect.height},
                              text_color, ui::text_align::left);
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
    const Color icon = with_alpha(hovered ? t.text : t.text_secondary, alpha);
    if (playing) {
        const float bar_width = 5.0f;
        const float bar_height = 18.0f;
        const float gap = 7.0f;
        const float total_width = bar_width * 2.0f + gap;
        const float x = visual.x + (visual.width - total_width) * 0.5f;
        const float y = visual.y + (visual.height - bar_height) * 0.5f;
        DrawRectangleRec({x, y, bar_width, bar_height}, icon);
        DrawRectangleRec({x + bar_width + gap, y, bar_width, bar_height}, icon);
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

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    return key_count == 6 ? theme.rank_c : theme.rank_b;
}

std::string format_bpm_range(float min_bpm, float max_bpm) {
    if (min_bpm <= 0.0f && max_bpm <= 0.0f) {
        return "-";
    }
    if (std::fabs(max_bpm - min_bpm) < 0.05f) {
        return TextFormat("%.0f", min_bpm);
    }
    return TextFormat("%.0f-%.0f", min_bpm, max_bpm);
}

std::pair<float, float> collect_bpm_range(const chart_data& chart) {
    float min_bpm = 0.0f;
    float max_bpm = 0.0f;
    bool found = false;

    for (const timing_event& event : chart.timing_events) {
        if (event.type != timing_event_type::bpm || event.bpm <= 0.0f) {
            continue;
        }
        if (!found) {
            min_bpm = event.bpm;
            max_bpm = event.bpm;
            found = true;
            continue;
        }
        min_bpm = std::min(min_bpm, event.bpm);
        max_bpm = std::max(max_bpm, event.bpm);
    }

    return found ? std::pair<float, float>{min_bpm, max_bpm}
                 : std::pair<float, float>{0.0f, 0.0f};
}

song_select::chart_option make_chart_option(const std::string& chart_path, const chart_data& chart) {
    const auto [min_bpm, max_bpm] = collect_bpm_range(chart);
    chart_meta meta = chart.meta;
    meta.level = chart_difficulty::calculate_level(chart);
    return {
        chart_path,
        meta,
        0,
        std::nullopt,
        static_cast<int>(chart.notes.size()),
        min_bpm,
        max_bpm,
    };
}

std::vector<song_select::song_entry> load_song_entries_from_directory(const std::filesystem::path& songs_root,
                                                                      bool attach_external_charts) {
    std::vector<song_select::song_entry> songs;
    if (!std::filesystem::exists(songs_root) || !std::filesystem::is_directory(songs_root)) {
        return songs;
    }

    const song_load_result load_result = song_loader::load_all(path_utils::to_utf8(songs_root));
    std::vector<song_data> all_songs = load_result.songs;
    if (attach_external_charts) {
        song_loader::attach_external_charts(path_utils::to_utf8(app_paths::charts_root()), all_songs);
    }

    std::sort(all_songs.begin(), all_songs.end(), [](const song_data& left, const song_data& right) {
        return left.meta.title < right.meta.title;
    });

    songs.reserve(all_songs.size());
    for (const song_data& song : all_songs) {
        song_select::song_entry entry;
        entry.song = song;

        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }
            entry.charts.push_back(make_chart_option(chart_path, *parse_result.data));
        }

        std::sort(entry.charts.begin(), entry.charts.end(), [](const song_select::chart_option& left,
                                                               const song_select::chart_option& right) {
            if (left.meta.key_count != right.meta.key_count) {
                return left.meta.key_count < right.meta.key_count;
            }
            if (left.meta.level != right.meta.level) {
                return left.meta.level < right.meta.level;
            }
            return left.meta.difficulty < right.meta.difficulty;
        });

        songs.push_back(std::move(entry));
    }

    return songs;
}

using local_song_lookup = std::unordered_map<std::string, const song_select::song_entry*>;

local_song_lookup build_local_lookup(const std::vector<song_select::song_entry>& local_songs) {
    local_song_lookup lookup;
    for (const song_select::song_entry& song : local_songs) {
        lookup[song.song.meta.song_id] = &song;
    }
    return lookup;
}

bool local_chart_exists(const song_select::song_entry* local_song, const std::string& chart_id) {
    if (local_song == nullptr || chart_id.empty()) {
        return false;
    }
    return std::any_of(local_song->charts.begin(), local_song->charts.end(),
                       [&](const song_select::chart_option& chart) {
                           return chart.meta.chart_id == chart_id;
                       });
}

song_entry_state build_song_state(const song_select::song_entry& remote_song,
                                  const local_song_lookup& local_lookup) {
    song_entry_state state_entry;
    state_entry.song = remote_song;

    const song_select::song_entry* local_song = nullptr;
    if (const auto it = local_lookup.find(remote_song.song.meta.song_id); it != local_lookup.end()) {
        local_song = it->second;
    }

    state_entry.installed = local_song != nullptr;
    state_entry.update_available = local_song != nullptr &&
                                   local_song->song.meta.song_version < remote_song.song.meta.song_version;

    state_entry.charts.reserve(remote_song.charts.size());
    for (const song_select::chart_option& chart : remote_song.charts) {
        state_entry.charts.push_back({
            chart,
            local_chart_exists(local_song, chart.meta.chart_id),
            state_entry.update_available,
        });
    }

    return state_entry;
}

song_entry_state build_owned_song_state(const song_select::song_entry& local_song) {
    song_entry_state state_entry;
    state_entry.song = local_song;
    state_entry.installed = true;
    state_entry.update_available = false;
    state_entry.charts.reserve(local_song.charts.size());
    for (const song_select::chart_option& chart : local_song.charts) {
        state_entry.charts.push_back({
            chart,
            true,
            false,
        });
    }
    return state_entry;
}

std::vector<song_select::song_entry> load_remote_song_entries(catalog_mode mode) {
    (void)mode;
    return {};
}

catalog_load_result load_catalog_result() {
    const std::vector<song_select::song_entry> local_songs =
        load_song_entries_from_directory(app_paths::songs_root(), true);
    const local_song_lookup local_lookup = build_local_lookup(local_songs);

    const std::vector<song_select::song_entry> official_remote_songs =
        load_remote_song_entries(catalog_mode::official);
    const std::vector<song_select::song_entry> community_remote_songs =
        load_remote_song_entries(catalog_mode::community);

    catalog_load_result result;
    result.official_songs.reserve(official_remote_songs.size());
    for (const song_select::song_entry& song : official_remote_songs) {
        result.official_songs.push_back(build_song_state(song, local_lookup));
    }

    result.community_songs.reserve(community_remote_songs.size());
    for (const song_select::song_entry& song : community_remote_songs) {
        result.community_songs.push_back(build_song_state(song, local_lookup));
    }

    result.owned_songs.reserve(local_songs.size());
    for (const song_select::song_entry& song : local_songs) {
        result.owned_songs.push_back(build_owned_song_state(song));
    }

    return result;
}

const std::vector<song_entry_state>& active_songs(const state& state);

std::vector<int> filtered_indices_for(const std::vector<song_entry_state>& songs, const std::string& query) {
    std::vector<int> indices;
    indices.reserve(songs.size());

    for (int index = 0; index < static_cast<int>(songs.size()); ++index) {
        const song_entry_state& song = songs[static_cast<size_t>(index)];
        if (query.empty() ||
            contains_case_insensitive(song.song.song.meta.title, query) ||
            contains_case_insensitive(song.song.song.meta.artist, query)) {
            indices.push_back(index);
        }
    }

    return indices;
}

std::vector<int> filtered_indices(const state& state) {
    const auto& songs = active_songs(state);
    return filtered_indices_for(songs, state.search_input.value);
}

int& selected_song_index_ref(state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_song_index;
    case catalog_mode::community:
        return state.community_selected_song_index;
    case catalog_mode::owned:
        return state.owned_selected_song_index;
    }
    return state.official_selected_song_index;
}

const int& selected_song_index_ref(const state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_song_index;
    case catalog_mode::community:
        return state.community_selected_song_index;
    case catalog_mode::owned:
        return state.owned_selected_song_index;
    }
    return state.official_selected_song_index;
}

int& selected_chart_index_ref(state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_chart_index;
    case catalog_mode::community:
        return state.community_selected_chart_index;
    case catalog_mode::owned:
        return state.owned_selected_chart_index;
    }
    return state.official_selected_chart_index;
}

const int& selected_chart_index_ref(const state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_chart_index;
    case catalog_mode::community:
        return state.community_selected_chart_index;
    case catalog_mode::owned:
        return state.owned_selected_chart_index;
    }
    return state.official_selected_chart_index;
}

const std::vector<song_entry_state>& active_songs(const state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_songs;
    case catalog_mode::community:
        return state.community_songs;
    case catalog_mode::owned:
        return state.owned_songs;
    }
    return state.official_songs;
}

void ensure_selection_valid(state& state) {
    const auto indices = filtered_indices(state);
    int& selected_song_index = selected_song_index_ref(state);
    int& selected_chart_index = selected_chart_index_ref(state);

    if (indices.empty()) {
        selected_song_index = 0;
        selected_chart_index = 0;
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        state.detail_open = false;
        return;
    }

    if (std::find(indices.begin(), indices.end(), selected_song_index) == indices.end()) {
        selected_song_index = indices.front();
        selected_chart_index = 0;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
    }

    const auto& songs = active_songs(state);
    if (selected_song_index < 0 || selected_song_index >= static_cast<int>(songs.size())) {
        selected_song_index = indices.front();
    }

    const auto& charts = songs[static_cast<size_t>(selected_song_index)].charts;
    if (charts.empty()) {
        selected_chart_index = 0;
    } else {
        selected_chart_index = std::clamp(selected_chart_index, 0, static_cast<int>(charts.size()) - 1);
    }
}

void sync_preview(state& state, song_select::preview_controller& preview_controller) {
    preview_controller.select_song(preview_song(state));
}

float song_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    const int rows = (count + kSongGridColumns - 1) / kSongGridColumns;
    return static_cast<float>(rows) * (kSongCardHeight + kSongGridGapY) - kSongGridGapY;
}

float max_song_scroll(Rectangle area, int count) {
    return std::max(0.0f, song_list_content_height(count) - area.height + 4.0f);
}

Rectangle song_row_rect(Rectangle area, int display_index, float scroll_y) {
    const float width =
        (area.width - static_cast<float>(kSongGridColumns - 1) * kSongGridGapX) /
        static_cast<float>(kSongGridColumns);
    const int row = display_index / kSongGridColumns;
    const int column = display_index % kSongGridColumns;
    return {
        area.x + static_cast<float>(column) * (width + kSongGridGapX),
        area.y + static_cast<float>(row) * (kSongCardHeight + kSongGridGapY) - scroll_y,
        width,
        kSongCardHeight
    };
}

Rectangle centered_left_pane_jacket_rect(Rectangle detail_left_rect, float scale) {
    const float width = kHeroJacketRect.width * scale;
    const float height = kHeroJacketRect.height * scale;
    const float inset = std::max(0.0f, (detail_left_rect.width - width) * 0.5f);
    return {
        detail_left_rect.x + inset,
        detail_left_rect.y + inset,
        width,
        height,
    };
}

Rectangle left_pane_preview_bar_rect(Rectangle content_rect, Rectangle hero_jacket_rect) {
    return {
        hero_jacket_rect.x,
        content_rect.y + content_rect.height - kPreviewBarBottomInset,
        hero_jacket_rect.width,
        kPreviewBarRect.height,
    };
}

Rectangle left_pane_button_rect(Rectangle content_rect, Rectangle hero_jacket_rect,
                                float bottom_inset, float height, int column) {
    const float width = (hero_jacket_rect.width - kControlButtonGap) * 0.5f;
    return {
        hero_jacket_rect.x + static_cast<float>(column) * (width + kControlButtonGap),
        content_rect.y + content_rect.height - bottom_inset,
        width,
        height,
    };
}

Rectangle left_pane_full_width_button_rect(Rectangle content_rect, Rectangle hero_jacket_rect,
                                           float bottom_inset, float height) {
    return {
        hero_jacket_rect.x,
        content_rect.y + content_rect.height - bottom_inset,
        hero_jacket_rect.width,
        height,
    };
}

Rectangle browse_search_rect() {
    const Rectangle account_rect = title_layout::account_chip_rect();
    const float left = kOwnedTabRect.x + kOwnedTabRect.width + 24.0f;
    const float right = account_rect.x - 18.0f;
    return {
        left,
        40.0f,
        std::max(220.0f, right - left),
        38.0f,
    };
}

float chart_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    const int rows = (count + kChartGridColumns - 1) / kChartGridColumns;
    return static_cast<float>(rows) * (kChartCardHeight + kChartGridGapY) - kChartGridGapY;
}

float max_chart_scroll(Rectangle area, int count) {
    return std::max(0.0f, chart_list_content_height(count) - area.height + 4.0f);
}

Rectangle chart_row_rect(Rectangle area, int index, float scroll_y) {
    const float width =
        (area.width - static_cast<float>(kChartGridColumns - 1) * kChartGridGapX) /
        static_cast<float>(kChartGridColumns);
    const int row = index / kChartGridColumns;
    const int column = index % kChartGridColumns;
    return {
        area.x + static_cast<float>(column) * (width + kChartGridGapX),
        area.y + static_cast<float>(row) * (kChartCardHeight + kChartGridGapY) - scroll_y,
        width,
        kChartCardHeight
    };
}

Rectangle song_list_viewport(Rectangle content_rect) {
    return {
        content_rect.x + 12.0f,
        content_rect.y + 34.0f,
        content_rect.width - 24.0f,
        content_rect.height - 46.0f,
    };
}

int hit_test_song_list(const state& state, Rectangle area, Vector2 point) {
    const auto indices = filtered_indices(state);
    if (!CheckCollisionPointRec(point, area)) {
        return -1;
    }

    for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
        if (CheckCollisionPointRec(point, song_row_rect(area, display_index, state.song_scroll_y))) {
            return indices[static_cast<size_t>(display_index)];
        }
    }
    return -1;
}

int hit_test_chart_list(const state& state, Rectangle area, Vector2 point) {
    const song_entry_state* song = selected_song(state);
    if (song == nullptr || !CheckCollisionPointRec(point, area)) {
        return -1;
    }

    for (int index = 0; index < static_cast<int>(song->charts.size()); ++index) {
        if (CheckCollisionPointRec(point, chart_row_rect(area, index, state.chart_scroll_y))) {
            return index;
        }
    }
    return -1;
}

std::string song_status_label(const song_entry_state& song) {
    if (song.update_available) {
        return "UPDATE";
    }
    return "";
}

Color song_status_color(const song_entry_state& song) {
    const auto& t = *g_theme;
    if (song.update_available) {
        return t.accent;
    }
    if (song.installed) {
        return t.success;
    }
    return t.text_muted;
}

std::string chart_status_label(const chart_entry_state& chart) {
    if (chart.update_available) {
        return "UPDATE";
    }
    if (!chart.installed) {
        return "GET";
    }
    return "";
}

const char* mode_label(catalog_mode mode) {
    switch (mode) {
    case catalog_mode::official:
        return "OFFICIAL";
    case catalog_mode::community:
        return "COMMUNITY";
    case catalog_mode::owned:
        return "OWNED";
    }
    return "OFFICIAL";
}

const char* catalog_caption(const state& state, const std::vector<song_entry_state>& songs) {
    if (state.catalog_loading) {
        return state.catalog_loaded_once ? "Refreshing..." : "Loading...";
    }
    switch (state.mode) {
    case catalog_mode::official:
        return songs.empty() ? "Official catalog unavailable" : "Official catalog";
    case catalog_mode::community:
        return songs.empty() ? "Community catalog unavailable" : "Community catalog";
    case catalog_mode::owned:
        return songs.empty() ? "No owned songs yet" : "Owned library";
    }
    return "Catalog";
}

std::string format_time_label(double seconds) {
    const int total = std::max(0, static_cast<int>(std::floor(seconds + 0.5)));
    return TextFormat("%d:%02d", total / 60, total % 60);
}

}  // namespace

jacket_cache::~jacket_cache() {
    clear();
}

const Texture2D* jacket_cache::get(const song_data& song) {
    const std::string key = song.meta.song_id;
    auto& entry = entries_[key];
    if (entry.loaded) {
        return &entry.texture;
    }
    if (entry.missing || song.meta.jacket_file.empty()) {
        return nullptr;
    }

    const std::filesystem::path jacket_path = path_utils::join_utf8(song.directory, song.meta.jacket_file);
    if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
        entry.missing = true;
        return nullptr;
    }

    const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
    entry.texture = LoadTexture(jacket_path_utf8.c_str());
    entry.loaded = entry.texture.id != 0;
    entry.missing = !entry.loaded;
    if (entry.loaded) {
        SetTextureFilter(entry.texture, TEXTURE_FILTER_BILINEAR);
        return &entry.texture;
    }
    return nullptr;
}

void jacket_cache::clear() {
    for (auto& [_, entry] : entries_) {
        if (entry.loaded) {
            UnloadTexture(entry.texture);
        }
    }
    entries_.clear();
}

void reload_catalog(state& state) {
    if (state.catalog_loading) {
        return;
    }
    state.catalog_loading = true;
    state.catalog_future = std::async(std::launch::async, []() {
        return load_catalog_result();
    });
}

bool poll_catalog(state& state) {
    if (!state.catalog_loading) {
        return false;
    }
    if (state.catalog_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    catalog_load_result result = state.catalog_future.get();
    state.catalog_loading = false;
    state.catalog_loaded_once = true;
    state.jackets.clear();
    state.official_songs = std::move(result.official_songs);
    state.community_songs = std::move(result.community_songs);
    state.owned_songs = std::move(result.owned_songs);

    if (state.official_songs.empty() && state.community_songs.empty() && !state.owned_songs.empty()) {
        state.mode = catalog_mode::owned;
    } else if (state.official_songs.empty() && !state.community_songs.empty()) {
        state.mode = catalog_mode::community;
    } else if (state.community_songs.empty() && !state.official_songs.empty()) {
        state.mode = catalog_mode::official;
    }

    ensure_selection_valid(state);
    return true;
}

void on_enter(state& state, song_select::preview_controller& preview_controller) {
    if (!state.catalog_loaded_once && !state.catalog_loading) {
        reload_catalog(state);
    }
    sync_preview(state, preview_controller);
}

void on_exit(state& state) {
    state.jackets.clear();
}

const song_entry_state* selected_song(const state& state) {
    const auto indices = filtered_indices(state);
    if (indices.empty()) {
        return nullptr;
    }

    const auto& songs = active_songs(state);
    const int selected_song_index = selected_song_index_ref(state);
    if (selected_song_index < 0 || selected_song_index >= static_cast<int>(songs.size())) {
        return nullptr;
    }
    if (std::find(indices.begin(), indices.end(), selected_song_index) == indices.end()) {
        return nullptr;
    }
    return &songs[static_cast<size_t>(selected_song_index)];
}

const chart_entry_state* selected_chart(const state& state) {
    const song_entry_state* song = selected_song(state);
    if (song == nullptr || song->charts.empty()) {
        return nullptr;
    }

    const int selected_chart_index = selected_chart_index_ref(state);
    if (selected_chart_index < 0 || selected_chart_index >= static_cast<int>(song->charts.size())) {
        return nullptr;
    }
    return &song->charts[static_cast<size_t>(selected_chart_index)];
}

const song_select::song_entry* preview_song(const state& state) {
    const song_entry_state* song = selected_song(state);
    return song != nullptr ? &song->song : nullptr;
}

bool can_open_local(const state& state) {
    const song_entry_state* song = selected_song(state);
    return song != nullptr && song->installed;
}

std::string selected_song_id(const state& state) {
    const song_entry_state* song = selected_song(state);
    return song != nullptr ? song->song.song.meta.song_id : "";
}

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);
    const Rectangle search_rect = browse_search_rect();

    const Rectangle seed_back = centered_scaled_rect(origin, kBackRect, 0.9f, {-448.0f, -198.0f});
    const Rectangle seed_official_tab = centered_scaled_rect(origin, kOfficialTabRect, 0.84f, {-276.0f, -194.0f});
    const Rectangle seed_community_tab = centered_scaled_rect(origin, kCommunityTabRect, 0.84f, {-132.0f, -194.0f});
    const Rectangle seed_owned_tab = centered_scaled_rect(origin, kOwnedTabRect, 0.84f, {12.0f, -194.0f});
    const Rectangle seed_search = centered_scaled_rect(origin, search_rect, 0.86f, {318.0f, -192.0f});
    const Rectangle seed_content = centered_scaled_rect(origin, kContentRect, 0.84f, {96.0f, 48.0f});
    const Rectangle seed_detail_left = centered_scaled_rect(origin, kDetailLeftRect, 0.78f, {-238.0f, 66.0f});
    const Rectangle seed_detail_right = centered_scaled_rect(origin, kDetailRightRect, 0.8f, {186.0f, 66.0f});
    const Rectangle seed_chart_list = centered_scaled_rect(origin, kChartListRect, 0.84f, {198.0f, 44.0f});
    const Rectangle current_content = lerp_rect(seed_content, kContentRect, t);
    const Rectangle current_detail_left = lerp_rect(seed_detail_left, kDetailLeftRect, t);
    const Rectangle current_detail_right = lerp_rect(seed_detail_right, kDetailRightRect, t);
    const Rectangle current_jacket = centered_left_pane_jacket_rect(current_detail_left, lerp_value(0.82f, 1.0f, t));
    const Rectangle current_preview_bar = left_pane_preview_bar_rect(current_content, current_jacket);
    const Rectangle current_preview_play =
        left_pane_full_width_button_rect(current_content, current_jacket, kPreviewButtonBottomInset, kPreviewPlayRect.height);
    const Rectangle current_primary =
        left_pane_full_width_button_rect(current_content, current_jacket, kActionButtonBottomInset, kPrimaryActionRect.height);

    return {
        lerp_rect(seed_back, kBackRect, t),
        lerp_rect(seed_official_tab, kOfficialTabRect, t),
        lerp_rect(seed_community_tab, kCommunityTabRect, t),
        lerp_rect(seed_owned_tab, kOwnedTabRect, t),
        lerp_rect(seed_search, search_rect, t),
        current_content,
        song_list_viewport(current_content),
        current_detail_left,
        current_detail_right,
        current_jacket,
        current_preview_bar,
        current_preview_play,
        {},
        lerp_rect(seed_chart_list, kChartListRect, t),
        current_primary,
        {},
    };
}

update_result update(state& state, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const layout current = make_layout(anim_t, origin_rect);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();

    if (ui::is_clicked(current.back_rect)) {
        result.back_requested = true;
        return result;
    }
    if (state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        state.detail_open = false;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        return result;
    }
    if (!state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        result.back_requested = true;
        return result;
    }

    const auto switch_mode = [&](catalog_mode new_mode) -> bool {
        if (state.mode == new_mode) {
            return false;
        }
        state.mode = new_mode;
        state.detail_open = false;
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        ensure_selection_valid(state);
        result.song_selection_changed = true;
        return true;
    };

    if (ui::is_clicked(current.official_tab_rect) && switch_mode(catalog_mode::official)) {
        return result;
    }
    if (ui::is_clicked(current.community_tab_rect) && switch_mode(catalog_mode::community)) {
        return result;
    }
    if (ui::is_clicked(current.owned_tab_rect) && switch_mode(catalog_mode::owned)) {
        return result;
    }

    ensure_selection_valid(state);

    const Rectangle song_list_rect = current.song_grid_rect;

    if (!state.detail_open && left_pressed) {
        const int clicked_song = hit_test_song_list(state, song_list_rect, mouse);
        if (clicked_song >= 0) {
            int& selected_song_index = selected_song_index_ref(state);
            selected_song_index = clicked_song;
            selected_chart_index_ref(state) = 0;
            state.chart_scroll_y = 0.0f;
            state.chart_scroll_y_target = 0.0f;
            state.detail_open = true;
            result.song_selection_changed = true;
            return result;
        }
    }

    if (state.detail_open && left_pressed) {
        const int clicked_chart = hit_test_chart_list(state, current.chart_list_rect, mouse);
        if (clicked_chart >= 0) {
            int& selected_chart_index = selected_chart_index_ref(state);
            if (selected_chart_index != clicked_chart) {
                selected_chart_index = clicked_chart;
                result.chart_selection_changed = true;
                return result;
            }
        }
    }

    if (state.detail_open) {
        if (ui::is_clicked(current.preview_play_rect)) {
            result.action = audio_manager::instance().is_preview_playing()
                ? requested_action::stop_preview
                : requested_action::restart_preview;
            return result;
        }
        if (selected_song(state) != nullptr && ui::is_clicked(current.primary_action_rect)) {
            result.action = requested_action::primary;
            return result;
        }
    }

    const bool allow_navigation_keys = !state.search_input.active;
    if (allow_navigation_keys) {
        auto indices = filtered_indices(state);
        int& selected_song_index = selected_song_index_ref(state);
        int& selected_chart_index = selected_chart_index_ref(state);

        if (!indices.empty() && !state.detail_open) {
            auto selected_it = std::find(indices.begin(), indices.end(), selected_song_index);
            int display_index = selected_it == indices.end() ? 0 : static_cast<int>(selected_it - indices.begin());

            int next_display_index = display_index;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                next_display_index = std::max(0, display_index - 1);
            } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                next_display_index = std::min(static_cast<int>(indices.size()) - 1, display_index + 1);
            } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                next_display_index = std::max(0, display_index - kSongGridColumns);
            } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                next_display_index = std::min(static_cast<int>(indices.size()) - 1, display_index + kSongGridColumns);
            }

            if (next_display_index != display_index) {
                selected_song_index = indices[static_cast<size_t>(next_display_index)];
                selected_chart_index = 0;
                state.chart_scroll_y = 0.0f;
                state.chart_scroll_y_target = 0.0f;
                result.song_selection_changed = true;
            }

            if (IsKeyPressed(KEY_ENTER) && selected_song(state) != nullptr) {
                state.detail_open = true;
                return result;
            }
        }

        const song_entry_state* song = selected_song(state);
        if (state.detail_open && song != nullptr && !song->charts.empty()) {
            int next_chart_index = selected_chart_index;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                next_chart_index = std::max(0, selected_chart_index - 1);
            } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                next_chart_index = std::min(static_cast<int>(song->charts.size()) - 1, selected_chart_index + 1);
            } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                next_chart_index = std::max(0, selected_chart_index - kChartGridColumns);
            } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                next_chart_index = std::min(static_cast<int>(song->charts.size()) - 1, selected_chart_index + kChartGridColumns);
            }
            if (next_chart_index != selected_chart_index) {
                selected_chart_index = next_chart_index;
                result.chart_selection_changed = true;
            }
        }
    }

    const int filtered_song_count = static_cast<int>(filtered_indices(state).size());
    const song_entry_state* song = selected_song(state);
    const int chart_count = song != nullptr ? static_cast<int>(song->charts.size()) : 0;
    if (!state.detail_open && CheckCollisionPointRec(mouse, song_list_rect) && wheel != 0.0f) {
        state.song_scroll_y_target -= wheel * 54.0f;
    } else if (state.detail_open && CheckCollisionPointRec(mouse, current.chart_list_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * 42.0f;
    }

    state.song_scroll_y_target = std::clamp(state.song_scroll_y_target, 0.0f,
                                            max_song_scroll(song_list_rect, filtered_song_count));
    state.song_scroll_y += (state.song_scroll_y_target - state.song_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.song_scroll_y - state.song_scroll_y_target) < 0.5f) {
        state.song_scroll_y = state.song_scroll_y_target;
    }

    state.chart_scroll_y_target = std::clamp(state.chart_scroll_y_target, 0.0f,
                                             max_chart_scroll(current.chart_list_rect, chart_count));
    state.chart_scroll_y += (state.chart_scroll_y_target - state.chart_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.chart_scroll_y - state.chart_scroll_y_target) < 0.5f) {
        state.chart_scroll_y = state.chart_scroll_y_target;
    }

    return result;
}

void draw(state& state, float anim_t, Rectangle origin_rect) {
    const auto& t = *g_theme;
    const float play_t = ease_out_cubic(anim_t);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_layout(anim_t, origin_rect);
    const float content_fade_t = std::clamp((play_t - 0.16f) / 0.66f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
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

    const bool official_active = state.mode == catalog_mode::official;
    const bool community_active = state.mode == catalog_mode::community;
    const bool owned_active = state.mode == catalog_mode::owned;
    const auto indices = filtered_indices(state);
    const auto& songs = active_songs(state);
    const bool loading = state.catalog_loading;
    const char* caption = catalog_caption(state, songs);

    ui::draw_button_colored(current.back_rect, "HOME", 16,
                            with_alpha(button_base, normal_row_alpha),
                            with_alpha(button_hover, hover_row_alpha),
                            with_alpha(t.text, alpha), 1.5f);
    ui::draw_button_colored(current.official_tab_rect, "OFFICIAL", 16,
                            with_alpha(official_active ? button_selected : button_base,
                                       official_active ? selected_row_alpha : normal_row_alpha),
                            with_alpha(official_active ? button_selected : button_hover,
                                       official_active ? selected_row_alpha : hover_row_alpha),
                            with_alpha(official_active ? t.text : t.text_secondary, alpha), 1.5f);
    ui::draw_button_colored(current.community_tab_rect, "COMMUNITY", 16,
                            with_alpha(community_active ? button_selected : button_base,
                                       community_active ? selected_row_alpha : normal_row_alpha),
                            with_alpha(community_active ? button_selected : button_hover,
                                       community_active ? selected_row_alpha : hover_row_alpha),
                            with_alpha(community_active ? t.text : t.text_secondary, alpha), 1.5f);
    ui::draw_button_colored(current.owned_tab_rect, "OWNED", 16,
                            with_alpha(owned_active ? button_selected : button_base,
                                       owned_active ? selected_row_alpha : normal_row_alpha),
                            with_alpha(owned_active ? button_selected : button_hover,
                                       owned_active ? selected_row_alpha : hover_row_alpha),
                            with_alpha(owned_active ? t.text : t.text_secondary, alpha), 1.5f);
    draw_song_search_input(current.search_rect, state.search_input, "SEARCH", "songs / artists",
                           16, 64,
                           button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha,
                           alpha);

    DrawRectangleRec(current.content_rect, with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)));
    DrawRectangleLinesEx(current.content_rect, 1.2f, with_alpha(t.border_light, alpha));
    if (!state.detail_open) {
        ui::draw_text_in_rect(caption, 17,
                              {current.content_rect.x + 12.0f, current.content_rect.y + 6.0f,
                               current.content_rect.width * 0.52f, 20.0f},
                              with_alpha(loading ? t.text : t.text_secondary, alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())),
                              14,
                              {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                               current.content_rect.width * 0.5f - 12.0f, 16.0f},
                              with_alpha(t.text_muted, alpha), ui::text_align::right);
    } else {
        ui::draw_text_in_rect("Press Esc to return to the grid",
                              14,
                              {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                               current.content_rect.width * 0.5f - 12.0f, 16.0f},
                              with_alpha(t.text_muted, alpha), ui::text_align::right);
    }

    if (!state.detail_open) {
        ui::scoped_clip_rect song_clip(current.song_grid_rect);
        if (indices.empty()) {
            const Rectangle placeholder = {
                current.song_grid_rect.x + 96.0f,
                current.song_grid_rect.y + current.song_grid_rect.height * 0.5f - 42.0f,
                current.song_grid_rect.width - 192.0f,
                84.0f,
            };
            DrawRectangleRec(placeholder, with_alpha(button_base, selected_row_alpha));
            DrawRectangleLinesEx(placeholder, 1.5f, with_alpha(t.border_light, alpha));
            ui::draw_text_in_rect(loading ? "Loading..." : "No songs found.",
                                  30, placeholder, with_alpha(t.text, alpha), ui::text_align::center);
        }

        for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
            const int song_index = indices[static_cast<size_t>(display_index)];
            const song_entry_state& song = songs[static_cast<size_t>(song_index)];
            const Rectangle card = song_row_rect(current.song_grid_rect, display_index, state.song_scroll_y);
            if (card.y + card.height < current.song_grid_rect.y - 4.0f ||
                card.y > current.song_grid_rect.y + current.song_grid_rect.height + 4.0f) {
                continue;
            }

            const bool selected = song_index == selected_song_index_ref(state);
            const bool hovered = ui::is_hovered(card);
            const unsigned char row_alpha = selected ? selected_row_alpha
                : hovered ? hover_row_alpha
                          : normal_row_alpha;
            DrawRectangleRec(card, with_alpha(selected ? button_selected : button_base, row_alpha));
            DrawRectangleLinesEx(card, 1.15f,
                                 with_alpha(selected ? t.border_active : t.border_light, alpha));

            const float jacket_size = std::min(card.width - 34.0f, 92.0f);
            const Rectangle jacket_rect = {
                card.x + (card.width - jacket_size) * 0.5f,
                card.y + 16.0f,
                jacket_size,
                jacket_size
            };
            if (const Texture2D* jacket = state.jackets.get(song.song.song)) {
                DrawTexturePro(*jacket,
                               {0.0f, 0.0f, static_cast<float>(jacket->width), static_cast<float>(jacket->height)},
                               jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, alpha));
            } else {
                DrawRectangleRec(jacket_rect, with_alpha(t.bg_alt, row_alpha));
                ui::draw_text_in_rect("JACKET", 18, jacket_rect, with_alpha(t.text_muted, alpha), ui::text_align::center);
            }
            DrawRectangleLinesEx(jacket_rect, 1.0f, with_alpha(t.border_image, alpha));

            const std::string badge_label = song_status_label(song);
            if (!badge_label.empty()) {
                const Rectangle badge_rect = {card.x + card.width - 90.0f, card.y + 12.0f, 72.0f, 18.0f};
                ui::draw_text_in_rect(badge_label.c_str(), 12, badge_rect,
                                      with_alpha(song_status_color(song), alpha), ui::text_align::right);
            }

            draw_marquee_text(song.song.song.meta.title.c_str(),
                              {card.x + 14.0f, card.y + 118.0f, card.width - 28.0f, 18.0f},
                              18, with_alpha(t.text, alpha), now);
            draw_marquee_text(song.song.song.meta.artist.c_str(),
                              {card.x + 14.0f, card.y + 138.0f, card.width - 28.0f, 16.0f},
                              13, with_alpha(t.text_muted, alpha), now);
        }

        ui::draw_notice_queue_bottom_right(state.notices,
                                           {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)});
        return;
    }

    const song_entry_state* song = selected_song(state);
    const chart_entry_state* chart = selected_chart(state);
    if (song == nullptr) {
        ui::draw_notice_queue_bottom_right(state.notices,
                                           {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)});
        return;
    }

    const float detail_left_right = current.detail_left_rect.x + current.detail_left_rect.width;
    const float separator_x = detail_left_right + (current.detail_right_rect.x - detail_left_right) * 0.5f;
    DrawLineEx({separator_x, current.detail_left_rect.y + 8.0f},
               {separator_x, current.detail_left_rect.y + current.detail_left_rect.height - 8.0f},
               1.4f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));

    if (const Texture2D* hero_jacket = state.jackets.get(song->song.song)) {
        DrawTexturePro(*hero_jacket,
                       {0.0f, 0.0f, static_cast<float>(hero_jacket->width), static_cast<float>(hero_jacket->height)},
                       current.hero_jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, alpha));
    } else {
        DrawRectangleRec(current.hero_jacket_rect, with_alpha(t.bg_alt, selected_row_alpha));
        ui::draw_text_in_rect("JACKET", 26, current.hero_jacket_rect, with_alpha(t.text_muted, alpha), ui::text_align::center);
    }
    DrawRectangleLinesEx(current.hero_jacket_rect, 1.5f, with_alpha(t.border_image, alpha));

    const Rectangle title_rect = {
        current.detail_left_rect.x + 12.0f,
        current.hero_jacket_rect.y + current.hero_jacket_rect.height + 12.0f,
        current.detail_left_rect.width - 24.0f,
        28.0f
    };
    const Rectangle artist_rect = {
        title_rect.x,
        title_rect.y + 32.0f,
        title_rect.width,
        18.0f
    };
    draw_marquee_text(song->song.song.meta.title.c_str(), title_rect, 28, with_alpha(t.text, alpha), now);
    draw_marquee_text(song->song.song.meta.artist.c_str(), artist_rect, 17,
                      with_alpha(t.text_secondary, alpha), now);

    const audio_manager& audio = audio_manager::instance();
    const double preview_length = audio.get_preview_length_seconds();
    const double preview_position = audio.get_preview_position_seconds();
    const float preview_ratio =
        preview_length > 0.0 ? std::clamp(static_cast<float>(preview_position / preview_length), 0.0f, 1.0f) : 0.0f;
    DrawRectangleRec(current.preview_bar_rect, with_alpha(t.bg_alt, normal_row_alpha));
    DrawRectangleRec({current.preview_bar_rect.x, current.preview_bar_rect.y,
                      current.preview_bar_rect.width * preview_ratio, current.preview_bar_rect.height},
                     with_alpha(t.accent, alpha));
    DrawRectangleLinesEx(current.preview_bar_rect, 1.0f, with_alpha(t.border_light, alpha));
    ui::draw_text_in_rect(
        TextFormat("%s / %s",
                   format_time_label(preview_position).c_str(),
                   format_time_label(preview_length).c_str()),
        13,
        {current.preview_bar_rect.x, current.preview_bar_rect.y - 18.0f, current.preview_bar_rect.width, 14.0f},
        with_alpha(t.text_muted, alpha), ui::text_align::left);

    draw_transport_toggle_button(current.preview_play_rect, audio.is_preview_playing(), alpha);

    const char* primary_label = song->update_available ? "UPDATE SONG"
        : (song->installed ? "DOWNLOAD AGAIN" : "DOWNLOAD SONG");
    ui::draw_button_colored(current.primary_action_rect, primary_label, 15,
                            with_alpha(button_selected, selected_row_alpha),
                            with_alpha(button_selected, hover_row_alpha),
                            with_alpha(t.text, alpha), 1.5f);

    ui::draw_text_in_rect("CHARTS", 19,
                          {current.chart_list_rect.x, current.chart_list_rect.y - 28.0f,
                           current.chart_list_rect.width * 0.45f, 18.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%d items", static_cast<int>(song->charts.size())), 14,
                          {current.chart_list_rect.x + current.chart_list_rect.width * 0.46f, current.chart_list_rect.y - 26.0f,
                           current.chart_list_rect.width * 0.54f, 16.0f},
                          with_alpha(t.text_muted, alpha), ui::text_align::right);

    ui::scoped_clip_rect chart_clip(current.chart_list_rect);
    if (song->charts.empty()) {
        const Rectangle placeholder = {
            current.chart_list_rect.x + 64.0f,
            current.chart_list_rect.y + current.chart_list_rect.height * 0.5f - 36.0f,
            current.chart_list_rect.width - 128.0f,
            72.0f,
        };
        DrawRectangleRec(placeholder, with_alpha(button_base, selected_row_alpha));
        DrawRectangleLinesEx(placeholder, 1.5f, with_alpha(t.border_light, alpha));
        ui::draw_text_in_rect("No charts found.", 28, placeholder, with_alpha(t.text, alpha), ui::text_align::center);
    }

    for (int index = 0; index < static_cast<int>(song->charts.size()); ++index) {
        const chart_entry_state& item = song->charts[static_cast<size_t>(index)];
        const Rectangle card = chart_row_rect(current.chart_list_rect, index, state.chart_scroll_y);
        if (card.y + card.height < current.chart_list_rect.y - 4.0f ||
            card.y > current.chart_list_rect.y + current.chart_list_rect.height + 4.0f) {
            continue;
        }

        const bool selected = chart != nullptr && index == selected_chart_index_ref(state);
        const bool hovered = ui::is_hovered(card);
        const unsigned char row_alpha = selected ? selected_row_alpha
            : hovered ? hover_row_alpha
                      : normal_row_alpha;
        DrawRectangleRec(card, with_alpha(selected ? button_selected : button_base, row_alpha));
        DrawRectangleLinesEx(card, 1.0f,
                             with_alpha(selected ? t.border_active : t.border_light, alpha));

        ui::draw_text_in_rect(
            TextFormat("%s  %s", key_mode_label(item.chart.meta.key_count).c_str(), item.chart.meta.difficulty.c_str()),
            16,
            {card.x + 14.0f, card.y + 12.0f, card.width - 110.0f, 18.0f},
            with_alpha(key_mode_color(item.chart.meta.key_count), alpha), ui::text_align::left);
        const std::string chart_badge = chart_status_label(item);
        if (!chart_badge.empty()) {
            ui::draw_text_in_rect(chart_badge.c_str(), 12,
                                  {card.x + card.width - 86.0f, card.y + 14.0f, 72.0f, 14.0f},
                                  with_alpha(item.update_available ? t.accent : t.text_muted, alpha),
                                  ui::text_align::right);
        }
        ui::draw_text_in_rect(TextFormat("Lv.%.1f", item.chart.meta.level), 20,
                              {card.x + 14.0f, card.y + 38.0f, 96.0f, 22.0f},
                              with_alpha(t.text, alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%d Notes", item.chart.note_count), 13,
                              {card.x + 14.0f, card.y + 64.0f, card.width * 0.45f, 14.0f},
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
        ui::draw_text_in_rect(
            TextFormat("BPM %s", format_bpm_range(item.chart.min_bpm, item.chart.max_bpm).c_str()),
            13,
            {card.x + card.width * 0.45f, card.y + 64.0f, card.width * 0.55f - 14.0f, 14.0f},
            with_alpha(t.text_muted, alpha), ui::text_align::right);
    }

    ui::draw_notice_queue_bottom_right(state.notices,
                                       {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)});
}

}  // namespace title_online_view
