#include "song_create_scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_paths.h"

#include "audio_manager.h"
#include "path_utils.h"
#include "editor/editor_meter_map.h"
#include "editor_scene.h"
#include "file_dialog.h"
#include "gameplay/timing_engine.h"
#include "localization/localization.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "song_writer.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "uuid_util.h"
#include "virtual_screen.h"

namespace {

namespace fs = std::filesystem;

constexpr ui::draw_layer kLayer = ui::draw_layer::base;
constexpr int kScreenW = kScreenWidth;
constexpr int kScreenH = kScreenHeight;
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenW), static_cast<float>(kScreenH)};
constexpr Rectangle kCardFrameRect = ui::place(kScreenRect, 1125.0f, 990.0f,
                                               ui::anchor::top_left, ui::anchor::top_left,
                                               Vector2{397.5f, 45.0f});
constexpr Rectangle kHeaderRect = ui::place(kCardFrameRect, 930.0f, 90.0f,
                                            ui::anchor::top_left, ui::anchor::top_left,
                                            Vector2{51.0f, 42.0f});

constexpr float kFormWidth = 1050.0f;
constexpr float kRowHeight = 63.0f;
constexpr float kRowGap = 9.0f;
constexpr float kFormStartY = 213.0f;
constexpr float kFormX = (static_cast<float>(kScreenW) - kFormWidth) * 0.5f;
constexpr float kFormCardPaddingX = 39.0f;
constexpr Rectangle kFormCardRect = {kFormX - kFormCardPaddingX, 177.0f, kFormWidth + kFormCardPaddingX * 2.0f, 875.0f};
constexpr Rectangle kDecisionCardRect = {525.0f, 315.0f, 870.0f, 330.0f};
constexpr Rectangle kTimingModalRect = ui::place(kScreenRect, 1160.0f, 720.0f,
                                                 ui::anchor::center, ui::anchor::center);
constexpr float kTextInputLabelWidth = 180.0f;
constexpr float kBrowseWidth = 138.0f;
constexpr float kBrowseGap = 12.0f;
constexpr float kButtonTopGap = 24.0f;
constexpr float kButtonWidth = 270.0f;
constexpr float kButtonHeight = 66.0f;
constexpr float kButtonGap = 18.0f;
constexpr float kErrorTopGap = 18.0f;
constexpr float kErrorHeight = 36.0f;
constexpr size_t kMaxSongGenres = 3;
constexpr size_t kMaxSongKeywords = 5;
constexpr size_t kMaxSongKeywordLength = 40;
constexpr ui::draw_layer kModalLayer = ui::draw_layer::modal;

struct midi_timing_import {
    bool ok = false;
    int resolution = 480;
    float base_bpm = 120.0f;
    std::vector<timing_event> events;
    std::string message;
};

constexpr std::array<std::string_view, 54> kSongGenreOptions = {
    "Artcore",
    "Hardcore",
    "Drum and bass",
    "Breakcore",
    "Hardstyle",
    "Trance",
    "Future bass",
    "Vocal",
    "Vocaloid",
    "J-pop",
    "Anime song",
    "2 tone",
    "2-step",
    "Acid house",
    "Acid jazz",
    "Acid techno",
    "Acid trance",
    "Ambient",
    "Ambient techno",
    "Big beat",
    "Chiptune",
    "Classical",
    "Dance-pop",
    "Darkstep",
    "Digital hardcore",
    "Disco",
    "Dubstep",
    "Edm",
    "Electro house",
    "Electronica",
    "Eurobeat",
    "Gabber",
    "Happy hardcore",
    "Hard techno",
    "Hip hop",
    "House",
    "Idm",
    "Jazz",
    "Jump up",
    "Jungle",
    "Liquid funk",
    "Neurofunk",
    "Pop",
    "Progressive house",
    "Progressive trance",
    "Psytrance",
    "Rock",
    "Speedcore",
    "Synth-pop",
    "Synthwave",
    "Tech house",
    "Techno",
    "Techstep",
    "Uk hardcore",
};

constexpr std::array<std::string_view, 8> kFeaturedSongGenres = {
    "Artcore",
    "Hardcore",
    "Drum and bass",
    "Breakcore",
    "Hardstyle",
    "Trance",
    "Future bass",
    "Vocaloid",
};

constexpr Rectangle make_row(int index) {
    return {kFormX, kFormStartY + static_cast<float>(index) * (kRowHeight + kRowGap), kFormWidth, kRowHeight};
}

constexpr Rectangle make_custom_row(float y, float height) {
    return {kFormX, y, kFormWidth, height};
}

bool numeric_filter(int codepoint, const std::string&) {
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.';
}

bool int_filter(int codepoint, const std::string&) {
    return codepoint >= '0' && codepoint <= '9';
}

bool signed_int_filter(int codepoint, const std::string& value) {
    return (codepoint >= '0' && codepoint <= '9') || (codepoint == '-' && value.find('-') == std::string::npos);
}

bool bar_beat_filter(int codepoint, const std::string& value) {
    return (codepoint >= '0' && codepoint <= '9') || (codepoint == ':' && value.find(':') == std::string::npos);
}

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

std::string trim_ascii(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

bool equals_case_insensitive(std::string_view left, std::string_view right) {
    return lower_ascii(left) == lower_ascii(right);
}

bool contains_case_insensitive(std::string_view text, std::string_view query) {
    if (query.empty()) {
        return true;
    }
    return lower_ascii(text).find(lower_ascii(query)) != std::string::npos;
}

bool contains_label(const std::vector<std::string>& values, std::string_view label) {
    return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
        return equals_case_insensitive(value, label);
    });
}

std::optional<std::string> normalize_genre_label(std::string_view value) {
    const std::string trimmed = trim_ascii(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    for (std::string_view candidate : kSongGenreOptions) {
        if (equals_case_insensitive(candidate, trimmed)) {
            return std::string(candidate);
        }
    }
    return std::nullopt;
}

std::vector<std::string> normalize_genres_for_editor(const song_meta& meta) {
    std::vector<std::string> result;
    std::vector<std::string> source = meta.genres;
    if (source.empty() && !meta.genre.empty()) {
        source.push_back(meta.genre);
    }
    for (const std::string& value : source) {
        const std::optional<std::string> normalized = normalize_genre_label(value);
        if (normalized.has_value() && !contains_label(result, *normalized) &&
            result.size() < kMaxSongGenres) {
            result.push_back(*normalized);
        }
    }
    return result;
}

std::vector<std::string> normalize_keywords_for_editor(const song_meta& meta) {
    std::vector<std::string> result;
    for (const std::string& value : meta.keywords) {
        std::string keyword = trim_ascii(value);
        if (keyword.empty()) {
            continue;
        }
        if (ui::utf8_codepoint_count(keyword) > kMaxSongKeywordLength) {
            keyword = ui::utf8_substr_codepoints(keyword, 0, kMaxSongKeywordLength);
        }
        if (!contains_label(result, keyword) && result.size() < kMaxSongKeywords) {
            result.push_back(std::move(keyword));
        }
    }
    return result;
}

std::vector<std::string> genre_suggestions(const std::string& query,
                                           const std::vector<std::string>& selected) {
    std::vector<std::string> result;
    const std::string trimmed_query = trim_ascii(query);

    const auto append_candidate = [&](std::string_view candidate) {
        if (result.size() >= 6 || contains_label(selected, candidate) ||
            contains_label(result, candidate)) {
            return;
        }
        if (trimmed_query.empty() || contains_case_insensitive(candidate, trimmed_query)) {
            result.emplace_back(candidate);
        }
    };

    if (trimmed_query.empty()) {
        for (std::string_view candidate : kFeaturedSongGenres) {
            append_candidate(candidate);
        }
    }
    for (std::string_view candidate : kSongGenreOptions) {
        append_candidate(candidate);
    }
    return result;
}

void deactivate_text_input(ui::text_input_state& input) {
    input.active = false;
    input.mouse_selecting = false;
    input.has_selection = false;
    input.selection_anchor = input.cursor;
}

bool add_genre(std::vector<std::string>& selected, ui::text_input_state& input, std::string_view label) {
    if (selected.size() >= kMaxSongGenres || contains_label(selected, label)) {
        return false;
    }
    const std::optional<std::string> normalized = normalize_genre_label(label);
    if (!normalized.has_value()) {
        return false;
    }
    selected.push_back(*normalized);
    input.value.clear();
    input.cursor = 0;
    deactivate_text_input(input);
    return true;
}

bool add_keyword(std::vector<std::string>& selected, ui::text_input_state& input) {
    if (selected.size() >= kMaxSongKeywords) {
        return false;
    }
    std::string keyword = trim_ascii(input.value);
    if (keyword.empty() || contains_label(selected, keyword)) {
        return false;
    }
    if (ui::utf8_codepoint_count(keyword) > kMaxSongKeywordLength) {
        keyword = ui::utf8_substr_codepoints(keyword, 0, kMaxSongKeywordLength);
    }
    selected.push_back(std::move(keyword));
    input.value.clear();
    input.cursor = 0;
    deactivate_text_input(input);
    return true;
}

void draw_chip_list(Rectangle rect, std::vector<std::string>& values, Color accent, int font_size) {
    float x = rect.x;
    float y = rect.y;
    for (size_t index = 0; index < values.size();) {
        const std::string& value = values[index];
        const float measured = ui::measure_text_size(value.c_str(), static_cast<float>(font_size)).x;
        const float width = std::min(std::max(82.0f, measured + 34.0f), 190.0f);
        if (x + width > rect.x + rect.width && x > rect.x) {
            x = rect.x;
            y += 30.0f;
        }
        if (y + 26.0f > rect.y + rect.height) {
            break;
        }
        const Rectangle chip = {x, y, width, 26.0f};
        const bool hovered = ui::is_hovered(chip, kLayer);
        ui::draw_rect_f(chip, with_alpha(g_theme->section, hovered ? 255 : 225));
        ui::draw_rect_lines(chip, 1.2f, accent);
        ui::draw_text_in_rect(value.c_str(), font_size,
                              {chip.x + 8.0f, chip.y, chip.width - 28.0f, chip.height},
                              g_theme->text, ui::text_align::left);
        ui::draw_text_in_rect("x", font_size, {chip.x + chip.width - 22.0f, chip.y, 16.0f, chip.height},
                              g_theme->text_muted);
        if (ui::is_clicked(chip, kLayer)) {
            values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        x += width + 8.0f;
        ++index;
    }
}

ui::button_state draw_button_on_layer(Rectangle rect, const char* label, int font_size,
                                      ui::draw_layer layer,
                                      Color bg = g_theme->row,
                                      Color bg_hover = g_theme->row_hover,
                                      Color text = g_theme->text,
                                      float border_width = 2.0f) {
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    const bool clicked = ui::is_clicked(rect, layer);
    ui::detail::draw_button_visual(rect, hovered, pressed, localization::tr_literal(label), font_size,
                                   bg, bg_hover, text, border_width);
    return {hovered, pressed, clicked};
}

ui::row_state draw_selectable_row_on_layer(Rectangle rect, bool selected, ui::draw_layer layer,
                                           float border_width = 2.0f) {
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    const bool clicked = ui::is_clicked(rect, layer);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::detail::draw_row_visual(rect, hovered, pressed,
                                selected ? g_theme->row_selected : g_theme->row,
                                selected ? g_theme->row_active : g_theme->row_hover,
                                selected ? g_theme->border_active : g_theme->border,
                                border_width);
    return {hovered, pressed, clicked, visual};
}

std::string key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

const char* timing_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Time Sig";
}

bool timing_event_less(const timing_event& left, const timing_event& right) {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    return left.type == timing_event_type::bpm && right.type != timing_event_type::bpm;
}

std::string timing_value_label(const timing_event& event) {
    if (event.type == timing_event_type::bpm) {
        return TextFormat("%.3g", event.bpm);
    }
    return std::to_string(event.numerator) + "/" + std::to_string(event.denominator);
}

bool parse_int_text(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_float_text(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

std::uint16_t read_be16(const std::vector<std::uint8_t>& bytes, size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t read_be32(const std::vector<std::uint8_t>& bytes, size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

bool read_vlq(const std::vector<std::uint8_t>& bytes, size_t& offset, size_t end, std::uint32_t& value) {
    value = 0;
    for (int count = 0; count < 4; ++count) {
        if (offset >= end) {
            return false;
        }
        const std::uint8_t byte = bytes[offset++];
        value = (value << 7) | static_cast<std::uint32_t>(byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return true;
        }
    }
    return false;
}

int midi_channel_data_length(std::uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0:
        case 0xD0:
            return 1;
        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        case 0xE0:
            return 2;
        default:
            return -1;
    }
}

midi_timing_import parse_midi_timing_file(const std::string& path) {
    midi_timing_import result;
    std::ifstream file(path_utils::from_utf8(path), std::ios::binary);
    if (!file) {
        result.message = "MIDI file could not be opened.";
        return result;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    if (bytes.size() < 14 || std::string_view(reinterpret_cast<const char*>(bytes.data()), 4) != "MThd") {
        result.message = "Selected file is not a Standard MIDI file.";
        return result;
    }

    const std::uint32_t header_length = read_be32(bytes, 4);
    if (header_length < 6 || bytes.size() < 8ULL + header_length) {
        result.message = "MIDI header is incomplete.";
        return result;
    }
    const std::uint16_t track_count = read_be16(bytes, 10);
    const std::uint16_t division = read_be16(bytes, 12);
    if ((division & 0x8000) != 0) {
        result.message = "SMPTE-based MIDI timing is not supported.";
        return result;
    }
    if (division == 0) {
        result.message = "MIDI timing resolution is invalid.";
        return result;
    }
    result.resolution = static_cast<int>(division);

    size_t offset = 8 + header_length;
    bool saw_bpm = false;
    bool saw_meter = false;
    for (std::uint16_t track_index = 0; track_index < track_count && offset + 8 <= bytes.size(); ++track_index) {
        if (std::string_view(reinterpret_cast<const char*>(bytes.data() + offset), 4) != "MTrk") {
            result.message = "MIDI track chunk is missing.";
            return result;
        }
        const std::uint32_t track_length = read_be32(bytes, offset + 4);
        offset += 8;
        if (offset + track_length > bytes.size()) {
            result.message = "MIDI track chunk is incomplete.";
            return result;
        }

        const size_t track_end = offset + track_length;
        std::uint32_t absolute_tick = 0;
        std::uint8_t running_status = 0;
        while (offset < track_end) {
            std::uint32_t delta = 0;
            if (!read_vlq(bytes, offset, track_end, delta)) {
                result.message = "MIDI delta time is invalid.";
                return result;
            }
            absolute_tick += delta;
            if (offset >= track_end) {
                break;
            }

            std::uint8_t status = bytes[offset++];
            if (status < 0x80) {
                if (running_status == 0) {
                    result.message = "MIDI running status is invalid.";
                    return result;
                }
                --offset;
                status = running_status;
            } else if (status < 0xF0) {
                running_status = status;
            }

            if (status == 0xFF) {
                if (offset >= track_end) {
                    result.message = "MIDI meta event is incomplete.";
                    return result;
                }
                const std::uint8_t meta_type = bytes[offset++];
                std::uint32_t meta_length = 0;
                if (!read_vlq(bytes, offset, track_end, meta_length) ||
                    offset + meta_length > track_end) {
                    result.message = "MIDI meta event length is invalid.";
                    return result;
                }
                if (meta_type == 0x51 && meta_length == 3) {
                    const std::uint32_t micros_per_quarter =
                        (static_cast<std::uint32_t>(bytes[offset]) << 16) |
                        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                        static_cast<std::uint32_t>(bytes[offset + 2]);
                    if (micros_per_quarter > 0) {
                        const float bpm = 60000000.0f / static_cast<float>(micros_per_quarter);
                        result.events.push_back({timing_event_type::bpm,
                                                 static_cast<int>(absolute_tick), bpm, 4, 4});
                        if (absolute_tick == 0) {
                            result.base_bpm = bpm;
                        }
                        saw_bpm = true;
                    }
                } else if (meta_type == 0x58 && meta_length >= 2) {
                    const int numerator = static_cast<int>(bytes[offset]);
                    const int denominator_power = static_cast<int>(bytes[offset + 1]);
                    if (numerator > 0 && denominator_power >= 0 && denominator_power <= 10) {
                        result.events.push_back({timing_event_type::meter,
                                                 static_cast<int>(absolute_tick), 0.0f, numerator,
                                                 1 << denominator_power});
                        saw_meter = true;
                    }
                }
                offset += meta_length;
                if (meta_type == 0x2F) {
                    break;
                }
                continue;
            }

            if (status == 0xF0 || status == 0xF7) {
                std::uint32_t sysex_length = 0;
                if (!read_vlq(bytes, offset, track_end, sysex_length) ||
                    offset + sysex_length > track_end) {
                    result.message = "MIDI SysEx event length is invalid.";
                    return result;
                }
                offset += sysex_length;
                running_status = 0;
                continue;
            }

            const int data_length = midi_channel_data_length(status);
            if (data_length < 0 || offset + static_cast<size_t>(data_length) > track_end) {
                result.message = "MIDI channel event is invalid.";
                return result;
            }
            offset += static_cast<size_t>(data_length);
        }
        offset = track_end;
    }

    if (!saw_bpm) {
        result.events.push_back({timing_event_type::bpm, 0, 120.0f, 4, 4});
        result.base_bpm = 120.0f;
    }
    if (!saw_meter) {
        result.events.push_back({timing_event_type::meter, 0, 0.0f, 4, 4});
    }
    std::stable_sort(result.events.begin(), result.events.end(), timing_event_less);
    result.events.erase(std::unique(result.events.begin(), result.events.end(),
                                    [](const timing_event& left, const timing_event& right) {
                                        return left.type == right.type && left.tick == right.tick;
                                    }),
                        result.events.end());
    result.ok = true;
    result.message = TextFormat("%s %d",
                                localization::tr_literal("Imported MIDI timing events:"),
                                static_cast<int>(result.events.size()));
    return result;
}

int normalized_midi_tick(int source_tick, int source_resolution) {
    if (source_resolution <= 0 || source_resolution == 480) {
        return source_tick;
    }
    return static_cast<int>(std::lround(static_cast<double>(source_tick) * 480.0 /
                                        static_cast<double>(source_resolution)));
}

std::optional<editor_meter_map::bar_beat_position> parse_bar_beat_text(const std::string& text) {
    const std::string trimmed = trim_ascii(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    const size_t colon = trimmed.find(':');
    int measure = 0;
    int beat = 1;
    if (colon == std::string::npos) {
        if (!parse_int_text(trimmed, measure)) {
            return std::nullopt;
        }
    } else if (!parse_int_text(trimmed.substr(0, colon), measure) ||
               !parse_int_text(trimmed.substr(colon + 1), beat)) {
        return std::nullopt;
    }
    if (measure <= 0 || beat <= 0) {
        return std::nullopt;
    }
    return editor_meter_map::bar_beat_position{measure, beat};
}

editor_meter_map build_song_timing_meter_map(const std::vector<timing_event>& timing_events, int timing_resolution) {
    chart_data data;
    data.meta.resolution = timing_resolution > 0 ? timing_resolution : 480;
    data.timing_events = timing_events;
    editor_meter_map meter_map;
    meter_map.rebuild(data);
    return meter_map;
}

int beat_step_ticks_at(const timing_engine& engine, int tick, int resolution) {
    const int denominator = std::max(1, engine.get_meter_denominator_at(std::max(0, tick)));
    return std::max(1, resolution * 4 / denominator);
}

void ensure_base_timing_events(song_meta& meta, float base_bpm) {
    const float bpm = base_bpm > 0.0f ? base_bpm : 120.0f;
    bool found_tick_zero_bpm = false;
    bool found_tick_zero_meter = false;
    for (timing_event& event : meta.timing_events) {
        if (event.tick != 0) {
            continue;
        }
        if (event.type == timing_event_type::bpm) {
            event.bpm = bpm;
            found_tick_zero_bpm = true;
        } else if (event.type == timing_event_type::meter) {
            found_tick_zero_meter = true;
        }
    }
    if (!found_tick_zero_bpm) {
        meta.timing_events.insert(meta.timing_events.begin(),
                                  {timing_event_type::bpm, 0, bpm, 4, 4});
    }
    if (!found_tick_zero_meter) {
        meta.timing_events.push_back({timing_event_type::meter, 0, 0.0f, 4, 4});
    }
}

bool paths_match(const fs::path& left, const fs::path& right) {
    std::error_code ec;
    if (fs::exists(left, ec) && fs::exists(right, ec)) {
        if (fs::equivalent(left, right, ec) && !ec) {
            return true;
        }
    }

    return left.lexically_normal() == right.lexically_normal();
}

void draw_field_label(Rectangle row, const char* label) {
    const Rectangle label_rect = {row.x + 12.0f, row.y, kTextInputLabelWidth - 12.0f, row.height};
    ui::draw_text_in_rect(label, 16, label_rect, g_theme->text_secondary, ui::text_align::left);
}

void draw_genre_selector(Rectangle row,
                         std::vector<std::string>& selected,
                         ui::text_input_state& search_input) {
    const bool maxed = selected.size() >= kMaxSongGenres;
    const Rectangle visual = row;
    ui::draw_rect_f(visual, maxed ? with_alpha(g_theme->row, 185) : g_theme->row);
    ui::draw_rect_lines(visual, 1.5f, search_input.active ? g_theme->border_active : g_theme->border);
    draw_field_label(visual, "Genres");

    const Rectangle content = {
        visual.x + kTextInputLabelWidth + 12.0f,
        visual.y + 8.0f,
        visual.width - kTextInputLabelWidth - 24.0f,
        visual.height - 16.0f,
    };
    draw_chip_list({content.x, content.y, content.width, 32.0f}, selected, g_theme->accent, 13);

    const Rectangle input_rect = {content.x, content.y + 36.0f, 300.0f, 38.0f};
    const ui::text_input_result input_result = ui::draw_text_input(
        input_rect, search_input, "", maxed ? "Up to 3 genres" : "Search genre...",
        nullptr, kLayer, 14, 40, wide_text_filter, 0.0f);

    const std::vector<std::string> suggestions = genre_suggestions(search_input.value, selected);
    float suggestion_x = input_rect.x + input_rect.width + 10.0f;
    float suggestion_y = input_rect.y;
    for (const std::string& suggestion : suggestions) {
        const float text_width = ui::measure_text_size(suggestion.c_str(), 13.0f).x;
        const float width = std::min(std::max(84.0f, text_width + 22.0f), 150.0f);
        if (suggestion_x + width > content.x + content.width) {
            suggestion_x = input_rect.x + input_rect.width + 10.0f;
            suggestion_y += 40.0f;
        }
        if (suggestion_y + 32.0f > content.y + content.height) {
            break;
        }
        const Rectangle button = {suggestion_x, suggestion_y, width, 32.0f};
        const Color base = maxed ? with_alpha(g_theme->section, 120) : with_alpha(g_theme->section, 235);
        const Color hover = maxed ? with_alpha(g_theme->section, 120) : with_alpha(g_theme->row_hover, 255);
        const ui::button_state state = ui::draw_button_colored(button, suggestion.c_str(), 13,
                                                               base, hover,
                                                               maxed ? g_theme->text_muted : g_theme->text,
                                                               1.2f);
        if (!maxed && state.clicked) {
            add_genre(selected, search_input, suggestion);
        }
        suggestion_x += width + 8.0f;
    }

    if (!maxed && input_result.submitted) {
        const std::vector<std::string> matches = genre_suggestions(search_input.value, selected);
        if (!matches.empty()) {
            add_genre(selected, search_input, matches.front());
        }
    }
}

void draw_keyword_editor(Rectangle row,
                         std::vector<std::string>& selected,
                         ui::text_input_state& keyword_input) {
    const bool maxed = selected.size() >= kMaxSongKeywords;
    ui::draw_rect_f(row, maxed ? with_alpha(g_theme->row, 185) : g_theme->row);
    ui::draw_rect_lines(row, 1.5f, keyword_input.active ? g_theme->border_active : g_theme->border);
    draw_field_label(row, "Keywords");

    const Rectangle content = {
        row.x + kTextInputLabelWidth + 12.0f,
        row.y + 8.0f,
        row.width - kTextInputLabelWidth - 24.0f,
        row.height - 16.0f,
    };
    draw_chip_list({content.x, content.y, content.width, 32.0f}, selected, g_theme->fast, 13);

    const Rectangle input_rect = {content.x, content.y + 38.0f, content.width - 112.0f, 38.0f};
    const Rectangle add_rect = {input_rect.x + input_rect.width + 10.0f, input_rect.y, 102.0f, 38.0f};
    const ui::text_input_result input_result = ui::draw_text_input(
        input_rect, keyword_input, "", maxed ? "Up to 5 keywords" : "Add keyword...",
        nullptr, kLayer, 14, kMaxSongKeywordLength, wide_text_filter, 0.0f);
    const ui::button_state add_button = ui::draw_button_colored(
        add_rect, "ADD", 14,
        maxed ? with_alpha(g_theme->section, 120) : g_theme->section,
        maxed ? with_alpha(g_theme->section, 120) : g_theme->row_hover,
        maxed ? g_theme->text_muted : g_theme->text,
        1.2f);
    if (!maxed && (input_result.submitted || add_button.clicked)) {
        add_keyword(selected, keyword_input);
    }
}

}  // namespace

song_create_scene::song_create_scene(scene_manager& manager)
    : scene(manager) {
    ensure_timing_events_initialized();
}

song_create_scene::song_create_scene(scene_manager& manager, song_data song_to_edit)
    : scene(manager), created_song_(song_to_edit), editing_song_(std::move(song_to_edit)) {
    const song_meta& meta = editing_song_->meta;
    title_input_.value = meta.title;
    artist_input_.value = meta.artist;
    selected_genres_ = normalize_genres_for_editor(meta);
    selected_keywords_ = normalize_keywords_for_editor(meta);
    bpm_input_.value = meta.base_bpm > 0.0f ? TextFormat("%.6g", meta.base_bpm) : "";
    offset_input_.value = std::to_string(meta.has_offset ? meta.offset : 0);
    preview_ms_input_.value = std::to_string(meta.preview_start_ms);
    timing_events_ = meta.timing_events;
    ensure_timing_events_initialized();

    if (!meta.audio_file.empty()) {
        audio_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.audio_file));
    }
    if (!meta.jacket_file.empty()) {
        jacket_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.jacket_file));
    }
}

void song_create_scene::ensure_timing_events_initialized() {
    float base_bpm = 120.0f;
    if (!bpm_input_.value.empty()) {
        float parsed = 0.0f;
        if (parse_float_text(bpm_input_.value, parsed) && parsed > 0.0f) {
            base_bpm = parsed;
        }
    }
    song_meta meta;
    meta.timing_events = timing_events_;
    ensure_base_timing_events(meta, base_bpm);
    timing_events_ = meta.timing_events;
    std::stable_sort(timing_events_.begin(), timing_events_.end(), timing_event_less);
    if (selected_timing_event_index_.has_value() && *selected_timing_event_index_ >= timing_events_.size()) {
        selected_timing_event_index_.reset();
    }
    if (!selected_timing_event_index_.has_value() && !timing_events_.empty()) {
        selected_timing_event_index_ = 0;
    }
    sync_selected_timing_inputs();
}

void song_create_scene::sync_selected_timing_inputs() {
    if (!selected_timing_event_index_.has_value() || *selected_timing_event_index_ >= timing_events_.size()) {
        timing_bar_input_.value.clear();
        timing_bpm_input_.value.clear();
        timing_numerator_input_.value.clear();
        timing_denominator_input_.value.clear();
        return;
    }

    const editor_meter_map meter_map = build_song_timing_meter_map(timing_events_, 480);
    const timing_event& event = timing_events_[*selected_timing_event_index_];
    timing_bar_input_.value = meter_map.bar_beat_label(event.tick);
    timing_bpm_input_.value = event.type == timing_event_type::bpm ? TextFormat("%.6g", event.bpm) : "";
    timing_numerator_input_.value = event.type == timing_event_type::meter ? std::to_string(event.numerator) : "";
    timing_denominator_input_.value = event.type == timing_event_type::meter ? std::to_string(event.denominator) : "";
}

void song_create_scene::add_timing_event(timing_event_type type) {
    if (!flush_selected_timing_event_inputs()) {
        return;
    }
    ensure_timing_events_initialized();
    timing_event event;
    event.type = type;
    event.tick = 1920;
    if (type == timing_event_type::bpm) {
        event.bpm = timing_events_.empty() ? 120.0f : std::max(1.0f, timing_events_.front().bpm);
    } else {
        event.numerator = 4;
        event.denominator = 4;
    }
    timing_events_.push_back(event);
    selected_timing_event_index_ = timing_events_.size() - 1;
    timing_event_scroll_offset_ = static_cast<float>(timing_events_.size()) * 34.0f;
    sync_selected_timing_inputs();
}

void song_create_scene::delete_selected_timing_event() {
    if (!flush_selected_timing_event_inputs()) {
        return;
    }
    if (!selected_timing_event_index_.has_value() || *selected_timing_event_index_ >= timing_events_.size()) {
        return;
    }
    const timing_event& selected = timing_events_[*selected_timing_event_index_];
    if (selected.tick == 0) {
        error_ = "The initial BPM/time signature cannot be deleted.";
        return;
    }
    timing_events_.erase(timing_events_.begin() + static_cast<std::ptrdiff_t>(*selected_timing_event_index_));
    if (timing_events_.empty()) {
        selected_timing_event_index_.reset();
    } else {
        selected_timing_event_index_ = std::min(*selected_timing_event_index_, timing_events_.size() - 1);
    }
    error_.clear();
    sync_selected_timing_inputs();
}

bool song_create_scene::apply_selected_timing_event() {
    if (!selected_timing_event_index_.has_value() || *selected_timing_event_index_ >= timing_events_.size()) {
        error_ = "Select a timing event first.";
        return false;
    }

    timing_event updated = timing_events_[*selected_timing_event_index_];
    const std::optional<editor_meter_map::bar_beat_position> position = parse_bar_beat_text(timing_bar_input_.value);
    if (!position.has_value()) {
        error_ = "Timing position must be in bar:beat format.";
        return false;
    }

    const editor_meter_map meter_map = build_song_timing_meter_map(timing_events_, 480);
    const std::optional<int> tick = meter_map.tick_from_bar_beat(position->measure, position->beat);
    if (!tick.has_value()) {
        error_ = "Timing position is outside the current time signature layout.";
        return false;
    }
    if (timing_events_[*selected_timing_event_index_].tick == 0 && *tick != 0) {
        error_ = "Initial BPM/time signature must stay at 1:1.";
        return false;
    }
    updated.tick = *tick;

    if (updated.type == timing_event_type::bpm) {
        float bpm = 0.0f;
        if (!parse_float_text(timing_bpm_input_.value, bpm) || bpm <= 0.0f) {
            error_ = "BPM must be greater than zero.";
            return false;
        }
        updated.bpm = bpm;
        if (updated.tick == 0) {
            bpm_input_.value = TextFormat("%.6g", bpm);
        }
    } else {
        int numerator = 0;
        int denominator = 0;
        if (!parse_int_text(timing_numerator_input_.value, numerator) || numerator <= 0 ||
            !parse_int_text(timing_denominator_input_.value, denominator) || denominator <= 0) {
            error_ = "Time signature must use positive numbers.";
            return false;
        }
        updated.numerator = numerator;
        updated.denominator = denominator;
    }

    timing_events_[*selected_timing_event_index_] = updated;
    std::stable_sort(timing_events_.begin(), timing_events_.end(), timing_event_less);
    for (size_t index = 0; index < timing_events_.size(); ++index) {
        if (timing_events_[index].type == updated.type && timing_events_[index].tick == updated.tick) {
            selected_timing_event_index_ = index;
            break;
        }
    }
    error_.clear();
    sync_selected_timing_inputs();
    return true;
}

bool song_create_scene::flush_selected_timing_event_inputs() {
    const bool timing_input_active = timing_bar_input_.active || timing_bpm_input_.active ||
                                     timing_numerator_input_.active || timing_denominator_input_.active;
    if (!timing_input_active || !selected_timing_event_index_.has_value()) {
        return true;
    }
    return apply_selected_timing_event();
}

void song_create_scene::close_timing_modal() {
    if (!flush_selected_timing_event_inputs()) {
        return;
    }
    timing_modal_open_ = false;
    stop_timing_preview();
}

bool song_create_scene::start_timing_preview() {
    if (audio_path_input_.value.empty()) {
        error_ = "Audio file is required.";
        return false;
    }
    const fs::path audio_source = path_utils::from_utf8(audio_path_input_.value);
    if (!fs::exists(audio_source)) {
        error_ = "Audio file not found: " + audio_path_input_.value;
        return false;
    }

    audio_manager& audio = audio_manager::instance();
    if (!audio.load_preview(audio_path_input_.value)) {
        error_ = "Failed to load audio preview.";
        return false;
    }
    audio.set_preview_volume(0.65f);
    audio.seek_preview(0.0);
    audio.play_preview(true);

    const std::filesystem::path tap = app_paths::audio_root() / "HitSound_RayTap.mp3";
    audio.play_se(path_utils::to_utf8(tap), 0.6f);
    metronome_elapsed_ms_ = 0.0;
    metronome_next_tick_ = 480;
    error_.clear();
    return true;
}

void song_create_scene::stop_timing_preview() {
    metronome_enabled_ = false;
    metronome_elapsed_ms_ = 0.0;
    metronome_next_tick_ = 0;
    audio_manager::instance().stop_preview();
}

bool song_create_scene::import_midi_timing(const std::string& midi_path) {
    if (!flush_selected_timing_event_inputs()) {
        return false;
    }
    const midi_timing_import imported = parse_midi_timing_file(midi_path);
    if (!imported.ok) {
        timing_import_status_ = imported.message;
        error_ = imported.message;
        return false;
    }

    bpm_input_.value = TextFormat("%.6g", imported.base_bpm);
    timing_events_ = imported.events;
    for (timing_event& event : timing_events_) {
        event.tick = normalized_midi_tick(event.tick, imported.resolution);
    }
    std::stable_sort(timing_events_.begin(), timing_events_.end(), timing_event_less);
    timing_events_.erase(std::unique(timing_events_.begin(), timing_events_.end(),
                                    [](const timing_event& left, const timing_event& right) {
                                        return left.type == right.type && left.tick == right.tick;
                                    }),
                         timing_events_.end());
    selected_timing_event_index_ = timing_events_.empty() ? std::nullopt : std::optional<size_t>(0);
    ensure_timing_events_initialized();
    timing_event_scroll_offset_ = 0.0f;
    timing_import_status_ = imported.resolution == 480
        ? imported.message
        : TextFormat("%s %s %d -> 480",
                     imported.message.c_str(),
                     localization::tr_literal("Normalized PPQ:"),
                     imported.resolution);
    error_.clear();
    return true;
}

std::vector<timing_event> song_create_scene::validated_timing_events(float base_bpm, bool& ok) {
    ok = true;
    ensure_timing_events_initialized();
    song_meta meta;
    meta.timing_events = timing_events_;
    ensure_base_timing_events(meta, base_bpm);
    std::stable_sort(meta.timing_events.begin(), meta.timing_events.end(), timing_event_less);
    for (const timing_event& event : meta.timing_events) {
        if (event.tick < 0 ||
            (event.type == timing_event_type::bpm && event.bpm <= 0.0f) ||
            (event.type == timing_event_type::meter && (event.numerator <= 0 || event.denominator <= 0))) {
            ok = false;
            error_ = "Song timing contains an invalid BPM or time signature.";
            return {};
        }
    }
    return meta.timing_events;
}

void song_create_scene::update_metronome(float dt) {
    if (!metronome_enabled_ || !selected_timing_event_index_.has_value() ||
        *selected_timing_event_index_ >= timing_events_.size()) {
        metronome_elapsed_ms_ = 0.0;
        return;
    }
    if (!audio_manager::instance().is_preview_playing()) {
        stop_timing_preview();
        return;
    }

    int offset_ms = 0;
    if (!offset_input_.value.empty()) {
        parse_int_text(offset_input_.value, offset_ms);
    }

    timing_engine engine;
    try {
        engine.init(timing_events_, 480, offset_ms);
    } catch (...) {
        stop_timing_preview();
        error_ = "Song timing contains an invalid BPM or time signature.";
        return;
    }

    const double current_ms = audio_manager::instance().get_preview_position_seconds() * 1000.0;
    const int current_tick = std::max(0, engine.ms_to_tick(current_ms));
    if (metronome_next_tick_ <= 0 || metronome_next_tick_ < current_tick - 480) {
        metronome_next_tick_ = current_tick;
    }

    const editor_meter_map meter_map = build_song_timing_meter_map(timing_events_, 480);
    int safety = 0;
    while (current_tick >= metronome_next_tick_ && safety++ < 16) {
        const editor_meter_map::bar_beat_position position = meter_map.bar_beat_at_tick(metronome_next_tick_);
        const bool downbeat = position.beat == 1;
        const std::filesystem::path tap = app_paths::audio_root() /
            (downbeat ? "HitSound_RayTap.mp3" : "HitSound_Tap.mp3");
        audio_manager::instance().play_se(path_utils::to_utf8(tap), downbeat ? 0.6f : 0.55f);
        metronome_next_tick_ += beat_step_ticks_at(engine, metronome_next_tick_, 480);
    }
}

void song_create_scene::update(float dt) {
    ui::begin_hit_regions();
    update_metronome(dt);

    switch (current_step_) {
        case step::song_metadata: update_song_metadata(); break;
        case step::song_saved: update_song_saved(); break;
    }
}

void song_create_scene::draw() {
    const auto& t = *g_theme;
    const char* content_title = is_edit_mode() ? "Edit Song" : "New Song";
    const char* content_subtitle = is_edit_mode() ? "Update song metadata" : "Enter song metadata";
    switch (current_step_) {
        case step::song_metadata:
            break;
        case step::song_saved:
            content_title = "Song Created";
            content_subtitle = "Choose the next action";
            break;
    }

    virtual_screen::begin_ui();
    if (timing_modal_open_) {
        ui::register_hit_region(kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(kTimingModalRect, kModalLayer);
    }
    draw_scene_background(t);
    ui::draw_header_block(kHeaderRect, content_title, content_subtitle);

    switch (current_step_) {
        case step::song_metadata:
            ui::draw_section(kFormCardRect);
            draw_song_metadata();
            break;
        case step::song_saved:
            ui::draw_section(kDecisionCardRect);
            draw_song_saved();
            break;
    }

    if (timing_modal_open_) {
        draw_timing_modal();
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void song_create_scene::update_song_metadata() {
    if (timing_modal_open_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            close_timing_modal();
        }
        return;
    }

    if (jacket_picker_.is_open()) {
        jacket_picker_.update();
        if (jacket_picker_.consume_accept()) {
            jacket_path_input_.value = jacket_picker_.source_path();
            jacket_crop_source_ = jacket_picker_.source_path();
            error_.clear();
        } else if (jacket_picker_.consume_cancel()) {
            jacket_crop_source_.clear();
            error_.clear();
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        go_back_to_song_select(editing_song_.has_value() ? editing_song_->meta.song_id : "");
        return;
    }
}

void song_create_scene::update_song_saved() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        go_back_to_song_select(created_song_.meta.song_id);
        return;
    }
}

void song_create_scene::draw_song_metadata() {
    float y = kFormStartY;

    ui::draw_text_input(make_custom_row(y, kRowHeight), title_input_, "Title", "Song title",
                        nullptr, kLayer, 16, 128, wide_text_filter, kTextInputLabelWidth);
    y += kRowHeight + kRowGap;

    ui::draw_text_input(make_custom_row(y, kRowHeight), artist_input_, "Artist", "Artist name",
                        nullptr, kLayer, 16, 128, wide_text_filter, kTextInputLabelWidth);
    y += kRowHeight + kRowGap;

    draw_genre_selector(make_custom_row(y, 126.0f), selected_genres_, genre_search_input_);
    y += 126.0f + kRowGap;

    draw_keyword_editor(make_custom_row(y, 104.0f), selected_keywords_, keyword_input_);
    y += 104.0f + kRowGap;

    draw_timing_summary(make_custom_row(y, kRowHeight));
    y += kRowHeight + kRowGap;

    {
        const Rectangle audio_row = make_custom_row(y, kRowHeight);
        const Rectangle input_rect = {audio_row.x, audio_row.y, audio_row.width - kBrowseWidth - kBrowseGap, audio_row.height};
        const Rectangle browse_rect = {audio_row.x + audio_row.width - kBrowseWidth, audio_row.y, kBrowseWidth, audio_row.height};

        ui::draw_text_input(input_rect, audio_path_input_, "Audio", "Select audio file...",
                            nullptr, kLayer, 16, 512, nullptr, kTextInputLabelWidth);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = file_dialog::open_audio_file();
            if (!path.empty()) {
                audio_path_input_.value = path;
            }
        }
    }
    y += kRowHeight + kRowGap;

    {
        const Rectangle jacket_row = make_custom_row(y, kRowHeight);
        const Rectangle input_rect = {jacket_row.x, jacket_row.y, jacket_row.width - kBrowseWidth - kBrowseGap, jacket_row.height};
        const Rectangle browse_rect = {jacket_row.x + jacket_row.width - kBrowseWidth, jacket_row.y, kBrowseWidth, jacket_row.height};

        ui::draw_text_input(input_rect, jacket_path_input_, "Jacket", "Select image file... (optional)",
                            nullptr, kLayer, 16, 512, nullptr, kTextInputLabelWidth);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = file_dialog::open_image_file();
            if (!path.empty()) {
                if (!jacket_picker_.open(path, error_)) {
                    jacket_path_input_.value.clear();
                    jacket_crop_source_.clear();
                }
            }
        }
    }
    y += kRowHeight + kRowGap;

    ui::draw_text_input(make_custom_row(y, kRowHeight), preview_ms_input_, "Preview (ms)", "0",
                        "0", kLayer, 16, 10, int_filter, kTextInputLabelWidth);
    y += kRowHeight + kRowGap;

    const float button_y = y + kButtonTopGap - kRowGap;
    const Rectangle create_rect = {kFormX + kFormWidth - kButtonWidth, button_y, kButtonWidth, kButtonHeight};
    const Rectangle cancel_rect = {kFormX + kFormWidth - kButtonWidth * 2.0f - kButtonGap, button_y, kButtonWidth, kButtonHeight};
    const char* submit_label = is_edit_mode() ? "SAVE" : "CREATE";
    const char* cancel_label = is_edit_mode() ? "BACK" : "CANCEL";

    if (ui::draw_button(create_rect, submit_label, 16).clicked) {
        const bool success = is_edit_mode() ? save_song_edits() : create_song();
        if (success && !is_edit_mode()) {
            current_step_ = step::song_saved;
            error_.clear();
        }
    }

    if (ui::draw_button(cancel_rect, cancel_label, 16).clicked) {
        go_back_to_song_select(editing_song_.has_value() ? editing_song_->meta.song_id : "");
        return;
    }

    if (!error_.empty()) {
        const Rectangle error_rect = {kFormX, button_y + kButtonHeight + kErrorTopGap, kFormWidth, kErrorHeight};
        ui::draw_text_in_rect(error_.c_str(), 14, error_rect, g_theme->error, ui::text_align::left);
    }

    if (jacket_picker_.is_open()) {
        jacket_picker_.draw();
    }
}

void song_create_scene::draw_timing_summary(Rectangle rect) {
    ensure_timing_events_initialized();
    float bpm = 0.0f;
    if (!parse_float_text(bpm_input_.value, bpm) || bpm <= 0.0f) {
        bpm = 120.0f;
    }
    int offset_ms = 0;
    if (!offset_input_.value.empty()) {
        parse_int_text(offset_input_.value, offset_ms);
    }

    ui::draw_rect_f(rect, g_theme->row);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border);
    draw_field_label(rect, "Song Timing");

    const Rectangle summary_rect = {
        rect.x + kTextInputLabelWidth + 12.0f,
        rect.y + 8.0f,
        rect.width - kTextInputLabelWidth - 180.0f,
        rect.height - 16.0f,
    };
    const std::string summary = TextFormat("BPM %.6g / Offset %d ms / %d events / 480 PPQ",
                                           bpm, offset_ms,
                                           static_cast<int>(timing_events_.size()));
    ui::draw_text_in_rect(summary.c_str(), 16, summary_rect, g_theme->text, ui::text_align::left);
    if (!timing_import_status_.empty()) {
        ui::draw_text_in_rect(timing_import_status_.c_str(), 12,
                              {summary_rect.x, summary_rect.y + 30.0f, summary_rect.width, 18.0f},
                              g_theme->text_muted, ui::text_align::left);
    }

    const Rectangle edit_rect = {rect.x + rect.width - 150.0f, rect.y + 10.0f, 138.0f, rect.height - 20.0f};
    if (ui::draw_button(edit_rect, "EDIT", 15).clicked) {
        timing_modal_open_ = true;
        timing_import_status_.clear();
        ensure_timing_events_initialized();
    }
}

void song_create_scene::draw_timing_modal() {
    ui::draw_rect_f(kScreenRect, with_alpha(BLACK, 150));
    ui::draw_panel(kTimingModalRect);

    const Rectangle title_rect = {kTimingModalRect.x + 30.0f, kTimingModalRect.y + 24.0f,
                                  kTimingModalRect.width - 180.0f, 34.0f};
    ui::draw_text_in_rect("Song Timing", 24, title_rect, g_theme->text, ui::text_align::left);

    const float content_x = kTimingModalRect.x + 30.0f;
    float y = kTimingModalRect.y + 80.0f;
    const float content_width = kTimingModalRect.width - 60.0f;
    const float gap = 12.0f;
    const float half_width = (content_width - gap) / 2.0f;
    ui::draw_text_input({content_x, y, half_width, 52.0f}, bpm_input_,
                        "Song BPM", "120.0", nullptr, kModalLayer, 15, 16,
                        numeric_filter, 100.0f);
    ui::draw_text_input({content_x + half_width + gap, y, half_width, 52.0f}, offset_input_,
                        "Song Offset", "0", "0", kModalLayer, 15, 10,
                        signed_int_filter, 118.0f);
    y += 64.0f;

    const Rectangle import_rect = {content_x, y, 188.0f, 38.0f};
    const Rectangle hint_rect = {import_rect.x + import_rect.width + 16.0f, y,
                                 content_width - import_rect.width - 16.0f, 38.0f};
    if (draw_button_on_layer(import_rect, "IMPORT MIDI", 13, kModalLayer,
                             g_theme->section, g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        const std::string path = file_dialog::open_midi_file();
        if (!path.empty()) {
            import_midi_timing(path);
        }
    }
    ui::draw_text_in_rect(
        timing_import_status_.empty() ? "Reads MIDI tempo and time signature events." : timing_import_status_.c_str(),
        13, hint_rect, timing_import_status_.empty() ? g_theme->text_muted : g_theme->text_secondary,
        ui::text_align::left);
    y += 52.0f;

    draw_timing_editor({content_x, y, content_width, 430.0f}, kModalLayer);

    const Rectangle done_rect = {kTimingModalRect.x + kTimingModalRect.width - 170.0f,
                                 kTimingModalRect.y + kTimingModalRect.height - 58.0f,
                                 140.0f, 38.0f};
    if (draw_button_on_layer(done_rect, "DONE", 14, kModalLayer,
                             g_theme->row_active, g_theme->row_hover, g_theme->text).clicked) {
        close_timing_modal();
    }
}

void song_create_scene::draw_timing_editor(Rectangle rect, ui::draw_layer layer) {
    if (timing_events_.empty()) {
        ensure_timing_events_initialized();
    }
    ui::draw_rect_f(rect, g_theme->row);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border);

    const Rectangle label_rect = {rect.x + 12.0f, rect.y + 10.0f, 188.0f, 28.0f};
    ui::draw_text_in_rect("Song Timing", 16, label_rect, g_theme->text_secondary, ui::text_align::left);

    const Rectangle list_rect = {rect.x + 200.0f, rect.y + 10.0f, 366.0f, rect.height - 20.0f};
    const Rectangle editor_rect = {list_rect.x + list_rect.width + 14.0f, rect.y + 10.0f,
                                   rect.x + rect.width - (list_rect.x + list_rect.width + 26.0f),
                                   rect.height - 20.0f};

    const float row_height = 28.0f;
    const float row_gap = 5.0f;
    const Rectangle list_view_rect = {list_rect.x, list_rect.y, list_rect.width - 14.0f, list_rect.height - 38.0f};
    const Rectangle scrollbar_rect = {list_view_rect.x + list_view_rect.width + 6.0f, list_view_rect.y,
                                      6.0f, list_view_rect.height};
    const float content_height = timing_events_.empty()
        ? list_view_rect.height
        : static_cast<float>(timing_events_.size()) * row_height +
              static_cast<float>(std::max<int>(0, static_cast<int>(timing_events_.size()) - 1)) * row_gap;
    const float max_scroll = std::max(0.0f, content_height - list_view_rect.height);
    timing_event_scroll_offset_ = std::clamp(timing_event_scroll_offset_, 0.0f, max_scroll);
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        scrollbar_rect, content_height, timing_event_scroll_offset_,
        timing_event_scrollbar_dragging_, timing_event_scrollbar_drag_offset_, layer, 28.0f);
    if (scrollbar.changed || scrollbar.dragging) {
        timing_event_scroll_offset_ = scrollbar.scroll_offset;
    }
    if (ui::is_hovered(list_view_rect, layer) && GetMouseWheelMove() != 0.0f) {
        timing_event_scroll_offset_ =
            std::clamp(timing_event_scroll_offset_ - GetMouseWheelMove() * 42.0f, 0.0f, max_scroll);
    }
    const editor_meter_map meter_map = build_song_timing_meter_map(timing_events_, 480);
    {
        ui::scoped_clip_rect clip_scope(list_view_rect);
        float row_y = list_view_rect.y - timing_event_scroll_offset_;
        for (size_t index = 0; index < timing_events_.size(); ++index) {
            const timing_event& event = timing_events_[index];
            const bool selected = selected_timing_event_index_.has_value() && *selected_timing_event_index_ == index;
            const Rectangle row = {list_view_rect.x, row_y, list_view_rect.width, row_height};
            const ui::row_state row_state = draw_selectable_row_on_layer(row, selected, layer, 1.2f);
            if (row_state.clicked) {
                if (selected || flush_selected_timing_event_inputs()) {
                    selected_timing_event_index_ = index;
                    sync_selected_timing_inputs();
                    error_.clear();
                }
            }
            const std::string label = std::string(timing_type_label(event.type)) + " " + meter_map.bar_beat_label(event.tick);
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 8.0f)),
                                 label.c_str(), timing_value_label(event).c_str(), 14,
                                 selected ? g_theme->text : g_theme->text_secondary,
                                 selected ? g_theme->text : g_theme->text_muted,
                                 160.0f);
            row_y += row_height + row_gap;
        }
    }
    ui::draw_scrollbar(scrollbar_rect, content_height, timing_event_scroll_offset_,
                       g_theme->scrollbar_track, g_theme->scrollbar_thumb, 28.0f);

    const Rectangle add_bpm = {list_rect.x, list_rect.y + list_rect.height - 30.0f, 104.0f, 28.0f};
    const Rectangle add_sig = {add_bpm.x + add_bpm.width + 8.0f, add_bpm.y, 124.0f, 28.0f};
    const Rectangle delete_rect = {add_sig.x + add_sig.width + 8.0f, add_bpm.y, 92.0f, 28.0f};
    if (draw_button_on_layer(add_bpm, "Add BPM", 13, layer).clicked) {
        add_timing_event(timing_event_type::bpm);
    }
    if (draw_button_on_layer(add_sig, "Add Time Sig", 13, layer).clicked) {
        add_timing_event(timing_event_type::meter);
    }
    if (draw_button_on_layer(delete_rect, "Delete", 13, layer).clicked) {
        delete_selected_timing_event();
    }

    const bool has_selection = selected_timing_event_index_.has_value() &&
                               *selected_timing_event_index_ < timing_events_.size();
    if (!has_selection) {
        ui::draw_text_in_rect("Select or add a timing event.", 15, editor_rect,
                              g_theme->text_hint, ui::text_align::center);
        return;
    }

    const timing_event& selected = timing_events_[*selected_timing_event_index_];
    ui::draw_label_value({editor_rect.x, editor_rect.y, editor_rect.width, 24.0f},
                         "Type", timing_type_label(selected.type), 15,
                         g_theme->text_secondary, g_theme->text, 72.0f);

    const float input_y = editor_rect.y + 32.0f;
    const float gap = 8.0f;
    const float small_width = (editor_rect.width - gap * 2.0f) / 3.0f;
    ui::draw_text_input({editor_rect.x, input_y, small_width, 34.0f},
                        timing_bar_input_, "Bar", "1:1", nullptr, layer, 14, 8,
                        bar_beat_filter, 42.0f);
    if (selected.type == timing_event_type::bpm) {
        ui::draw_text_input({editor_rect.x + small_width + gap, input_y, small_width, 34.0f},
                            timing_bpm_input_, "BPM", "120", nullptr, layer, 14, 12,
                            numeric_filter, 42.0f);
    } else {
        ui::draw_text_input({editor_rect.x + small_width + gap, input_y, small_width, 34.0f},
                            timing_numerator_input_, "Num", "4", nullptr, layer, 14, 4,
                            int_filter, 42.0f);
        ui::draw_text_input({editor_rect.x + (small_width + gap) * 2.0f, input_y, small_width, 34.0f},
                            timing_denominator_input_, "Den", "4", nullptr, layer, 14, 4,
                            int_filter, 42.0f);
    }

    const Rectangle metro_rect = {editor_rect.x, editor_rect.y + editor_rect.height - 34.0f, 132.0f, 30.0f};
    const Color metro_base = metronome_enabled_ ? g_theme->accent : g_theme->section;
    const Color metro_text = metronome_enabled_ ? g_theme->panel : g_theme->text;
    if (draw_button_on_layer(metro_rect,
                             metronome_enabled_ ? "Metronome On" : "Metronome",
                             13, layer, metro_base, g_theme->row_hover, metro_text, 1.2f).clicked) {
        if (metronome_enabled_) {
            stop_timing_preview();
        } else if (start_timing_preview()) {
            metronome_enabled_ = true;
        }
    }
}

void song_create_scene::draw_song_saved() {
    constexpr float kCenterY = 465.0f;
    constexpr float kSavedButtonWidth = 330.0f;
    constexpr float kSavedButtonHeight = 72.0f;
    constexpr float kSavedButtonGap = 24.0f;
    const float total_width = kSavedButtonWidth * 2.0f + kSavedButtonGap;
    const float start_x = (static_cast<float>(kScreenW) - total_width) * 0.5f;

    const Rectangle title_rect = {kDecisionCardRect.x + 54.0f, kDecisionCardRect.y + 42.0f, kDecisionCardRect.width - 108.0f, 51.0f};
    const Rectangle song_rect = {kDecisionCardRect.x + 54.0f, kDecisionCardRect.y + 108.0f, kDecisionCardRect.width - 108.0f, 45.0f};
    const Rectangle msg_rect = {kDecisionCardRect.x + 54.0f, kDecisionCardRect.y + 177.0f, kDecisionCardRect.width - 108.0f, 45.0f};
    ui::draw_text_in_rect("Song has been created.", 24, title_rect, g_theme->text, ui::text_align::center);
    ui::draw_text_in_rect(created_song_.meta.title.c_str(), 22, song_rect, g_theme->text_secondary, ui::text_align::center);
    ui::draw_text_in_rect("What would you like to do next?", 20, msg_rect, g_theme->text_muted, ui::text_align::center);

    const Rectangle add_chart_rect = {start_x, kCenterY + 67.5f, kSavedButtonWidth, kSavedButtonHeight};
    const Rectangle add_later_rect = {start_x + kSavedButtonWidth + kSavedButtonGap, kCenterY + 67.5f,
                                      kSavedButtonWidth, kSavedButtonHeight};

    if (ui::draw_button(add_chart_rect, "ADD CHART", 16).clicked) {
        manager_.change_scene(std::make_unique<editor_scene>(manager_, created_song_, 4));
        return;
    }

    if (ui::draw_button(add_later_rect, "ADD LATER", 16).clicked) {
        go_back_to_song_select(created_song_.meta.song_id);
    }
}


bool song_create_scene::create_song() {
    if (title_input_.value.empty()) {
        error_ = "Title is required.";
        return false;
    }
    if (artist_input_.value.empty()) {
        error_ = "Artist is required.";
        return false;
    }
    if (audio_path_input_.value.empty()) {
        error_ = "Audio file is required.";
        return false;
    }

    const fs::path audio_source = path_utils::from_utf8(audio_path_input_.value);
    if (!fs::exists(audio_source)) {
        error_ = "Audio file not found: " + audio_path_input_.value;
        return false;
    }

    float base_bpm = 0.0f;
    if (!bpm_input_.value.empty()) {
        try {
            base_bpm = std::stof(bpm_input_.value);
        } catch (...) {
            error_ = "Invalid BPM value.";
            return false;
        }
    }
    int preview_ms = 0;
    if (!preview_ms_input_.value.empty()) {
        try {
            preview_ms = std::stoi(preview_ms_input_.value);
        } catch (...) {
            error_ = "Invalid preview start value.";
            return false;
        }
    }

    int offset_ms = 0;
    if (!offset_input_.value.empty()) {
        try {
            offset_ms = std::stoi(offset_input_.value);
        } catch (...) {
            error_ = "Invalid song offset value.";
            return false;
        }
    }

    const bool timing_input_active = timing_bar_input_.active || timing_bpm_input_.active ||
                                     timing_numerator_input_.active || timing_denominator_input_.active;
    if (timing_input_active && selected_timing_event_index_.has_value() && !apply_selected_timing_event()) {
        return false;
    }
    bool timing_ok = false;
    const std::vector<timing_event> timing_events = validated_timing_events(base_bpm, timing_ok);
    if (!timing_ok) {
        return false;
    }

    const std::string song_id = generate_uuid();
    app_paths::ensure_directories();
    const fs::path song_dir = app_paths::song_dir(song_id);
    fs::create_directories(song_dir);

    const std::string audio_filename = path_utils::to_utf8(audio_source.filename());
    const fs::path audio_dest = song_dir / audio_filename;
    try {
        fs::copy_file(audio_source, audio_dest, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        error_ = std::string("Failed to copy audio file: ") + e.what();
        return false;
    }

    std::string jacket_filename;
    if (!jacket_path_input_.value.empty()) {
        const fs::path jacket_source = path_utils::from_utf8(jacket_path_input_.value);
        if (!fs::exists(jacket_source)) {
            error_ = "Jacket file not found: " + jacket_path_input_.value;
            return false;
        }
        if (!export_jacket_image(jacket_source, song_dir, jacket_filename)) {
            return false;
        }
    }

    song_meta meta;
    meta.song_id = song_id;
    meta.title = title_input_.value;
    meta.artist = artist_input_.value;
    meta.genres = selected_genres_;
    meta.genre = meta.genres.empty() ? "" : meta.genres.front();
    meta.keywords = selected_keywords_;
    meta.base_bpm = base_bpm;
    meta.offset = offset_ms;
    meta.has_offset = true;
    meta.timing_events = timing_events;
    meta.audio_file = audio_filename;
    meta.jacket_file = jacket_filename;
    meta.preview_start_ms = preview_ms;
    meta.preview_start_seconds = static_cast<float>(preview_ms) / 1000.0f;
    meta.song_version = 1;

    if (!song_writer::write_song_json(meta, path_utils::to_utf8(song_dir))) {
        error_ = "Failed to write song.json.";
        return false;
    }

    created_song_.meta = meta;
    created_song_.directory = path_utils::to_utf8(song_dir);
    created_song_.chart_paths.clear();

    return true;
}

bool song_create_scene::save_song_edits() {
    if (!editing_song_.has_value()) {
        error_ = "No song selected for editing.";
        return false;
    }
    if (title_input_.value.empty()) {
        error_ = "Title is required.";
        return false;
    }
    if (artist_input_.value.empty()) {
        error_ = "Artist is required.";
        return false;
    }
    if (audio_path_input_.value.empty()) {
        error_ = "Audio file is required.";
        return false;
    }

    const fs::path song_dir = path_utils::from_utf8(editing_song_->directory);
    const fs::path audio_source = path_utils::from_utf8(audio_path_input_.value);
    if (!fs::exists(audio_source)) {
        error_ = "Audio file not found: " + audio_path_input_.value;
        return false;
    }

    float base_bpm = 0.0f;
    if (!bpm_input_.value.empty()) {
        try {
            base_bpm = std::stof(bpm_input_.value);
        } catch (...) {
            error_ = "Invalid BPM value.";
            return false;
        }
    }
    int preview_ms = 0;
    if (!preview_ms_input_.value.empty()) {
        try {
            preview_ms = std::stoi(preview_ms_input_.value);
        } catch (...) {
            error_ = "Invalid preview start value.";
            return false;
        }
    }

    int offset_ms = 0;
    if (!offset_input_.value.empty()) {
        try {
            offset_ms = std::stoi(offset_input_.value);
        } catch (...) {
            error_ = "Invalid song offset value.";
            return false;
        }
    }

    const bool timing_input_active = timing_bar_input_.active || timing_bpm_input_.active ||
                                     timing_numerator_input_.active || timing_denominator_input_.active;
    if (timing_input_active && selected_timing_event_index_.has_value() && !apply_selected_timing_event()) {
        return false;
    }
    bool timing_ok = false;
    const std::vector<timing_event> timing_events = validated_timing_events(base_bpm, timing_ok);
    if (!timing_ok) {
        return false;
    }

    std::string audio_filename = editing_song_->meta.audio_file;
    const fs::path current_audio_path = path_utils::join_utf8(editing_song_->directory, editing_song_->meta.audio_file);
    if (!paths_match(audio_source, current_audio_path)) {
        audio_filename = path_utils::to_utf8(audio_source.filename());
        const fs::path audio_dest = song_dir / audio_filename;
        try {
            fs::copy_file(audio_source, audio_dest, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            error_ = std::string("Failed to copy audio file: ") + e.what();
            return false;
        }
    }

    std::string jacket_filename = editing_song_->meta.jacket_file;
    if (jacket_path_input_.value.empty()) {
        jacket_filename.clear();
    } else {
        const fs::path jacket_source = path_utils::from_utf8(jacket_path_input_.value);
        if (!fs::exists(jacket_source)) {
            error_ = "Jacket file not found: " + jacket_path_input_.value;
            return false;
        }

        const fs::path current_jacket_path = editing_song_->meta.jacket_file.empty()
            ? fs::path()
            : path_utils::join_utf8(editing_song_->directory, editing_song_->meta.jacket_file);
        if (!editing_song_->meta.jacket_file.empty() &&
            paths_match(jacket_source, current_jacket_path) &&
            jacket_crop_source_ != path_utils::to_utf8(jacket_source)) {
            jacket_filename = editing_song_->meta.jacket_file;
        } else {
            if (!export_jacket_image(jacket_source, song_dir, jacket_filename)) {
                return false;
            }
        }
    }

    song_meta meta = editing_song_->meta;
    meta.title = title_input_.value;
    meta.artist = artist_input_.value;
    meta.genres = selected_genres_;
    meta.genre = meta.genres.empty() ? "" : meta.genres.front();
    meta.keywords = selected_keywords_;
    meta.base_bpm = base_bpm;
    meta.offset = offset_ms;
    meta.has_offset = true;
    meta.timing_events = timing_events;
    meta.audio_file = audio_filename;
    meta.jacket_file = jacket_filename;
    meta.preview_start_ms = preview_ms;
    meta.preview_start_seconds = static_cast<float>(preview_ms) / 1000.0f;
    if (meta.song_version <= 0) {
        meta.song_version = 1;
    }

    if (!song_writer::write_song_json(meta, path_utils::to_utf8(song_dir))) {
        error_ = "Failed to write song.json.";
        return false;
    }

    editing_song_->meta = meta;
    created_song_ = *editing_song_;
    error_.clear();
    go_back_to_song_select(meta.song_id);
    return true;
}

bool song_create_scene::export_jacket_image(const fs::path& source_path,
                                            const fs::path& song_dir,
                                            std::string& jacket_filename) {
    jacket_filename = "jacket.png";
    const fs::path jacket_dest = song_dir / jacket_filename;
    const std::string source_utf8 = path_utils::to_utf8(source_path);
    const std::string dest_utf8 = path_utils::to_utf8(jacket_dest);

    square_image_picker::export_result result;
    if (jacket_crop_source_ == source_utf8 && jacket_picker_.source_path() == source_utf8) {
        result = jacket_picker_.export_png(dest_utf8, {.output_size = 512});
    } else {
        result = square_image_picker::export_center_square_png(source_utf8, dest_utf8, {.output_size = 512});
    }

    if (!result.success) {
        error_ = result.message.empty() ? "Failed to export jacket image." : result.message;
        jacket_filename.clear();
        return false;
    }
    return true;
}

void song_create_scene::go_back_to_song_select(const std::string& preferred_song_id) {
    manager_.change_scene(song_select::make_seamless_create_scene(manager_, preferred_song_id));
}

bool song_create_scene::is_edit_mode() const {
    return editing_song_.has_value();
}
