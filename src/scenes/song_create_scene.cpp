#include "song_create_scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_paths.h"

#include "path_utils.h"
#include "editor_scene.h"
#include "file_dialog.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "song_writer.h"
#include "theme.h"
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
constexpr Rectangle kFormCardRect = {kFormX - kFormCardPaddingX, 177.0f, kFormWidth + kFormCardPaddingX * 2.0f, 780.0f};
constexpr Rectangle kDecisionCardRect = {525.0f, 315.0f, 870.0f, 330.0f};
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

std::string key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
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
}

song_create_scene::song_create_scene(scene_manager& manager, song_data song_to_edit)
    : scene(manager), created_song_(song_to_edit), editing_song_(std::move(song_to_edit)) {
    const song_meta& meta = editing_song_->meta;
    title_input_.value = meta.title;
    artist_input_.value = meta.artist;
    selected_genres_ = normalize_genres_for_editor(meta);
    selected_keywords_ = normalize_keywords_for_editor(meta);
    bpm_input_.value = meta.base_bpm > 0.0f ? TextFormat("%.6g", meta.base_bpm) : "";
    preview_ms_input_.value = std::to_string(meta.preview_start_ms);

    if (!meta.audio_file.empty()) {
        audio_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.audio_file));
    }
    if (!meta.jacket_file.empty()) {
        jacket_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.jacket_file));
    }
}

void song_create_scene::update(float dt) {
    ui::begin_hit_regions();

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

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void song_create_scene::update_song_metadata() {
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

    ui::draw_text_input(make_custom_row(y, kRowHeight), bpm_input_, "BPM", "120.0",
                        nullptr, kLayer, 16, 16, numeric_filter, kTextInputLabelWidth);
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
