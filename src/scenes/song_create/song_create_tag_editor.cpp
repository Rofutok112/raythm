#include "song_create/song_create_tag_editor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include "theme.h"
#include "ui_text.h"

namespace song_create::tag_editor {
namespace {

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

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
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

void draw_field_label(Rectangle row, const char* label, float text_input_label_width) {
    const Rectangle label_rect = {row.x + 12.0f, row.y, text_input_label_width - 12.0f, row.height};
    ui::draw_text_in_rect(label, 16, label_rect, g_theme->text_secondary, ui::text_align::left);
}

void draw_chip_list(Rectangle rect,
                    std::vector<std::string>& values,
                    Color accent,
                    int font_size,
                    ui::draw_layer layer) {
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
        const bool hovered = ui::is_hovered(chip, layer);
        ui::draw_rect_f(chip, with_alpha(g_theme->section, hovered ? 255 : 225));
        ui::draw_rect_lines(chip, 1.2f, accent);
        ui::draw_text_in_rect(value.c_str(), font_size,
                              {chip.x + 8.0f, chip.y, chip.width - 28.0f, chip.height},
                              g_theme->text, ui::text_align::left);
        ui::draw_text_in_rect("x", font_size, {chip.x + chip.width - 22.0f, chip.y, 16.0f, chip.height},
                              g_theme->text_muted);
        if (ui::is_clicked(chip, layer)) {
            values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        x += width + 8.0f;
        ++index;
    }
}

}  // namespace

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

void draw_genre_selector(Rectangle row,
                         std::vector<std::string>& selected,
                         ui::text_input_state& search_input,
                         ui::draw_layer layer,
                         float text_input_label_width) {
    const bool maxed = selected.size() >= kMaxSongGenres;
    const Rectangle visual = row;
    ui::draw_rect_f(visual, maxed ? with_alpha(g_theme->row, 185) : g_theme->row);
    ui::draw_rect_lines(visual, 1.5f, search_input.active ? g_theme->border_active : g_theme->border);
    draw_field_label(visual, "Genres", text_input_label_width);

    const Rectangle content = {
        visual.x + text_input_label_width + 12.0f,
        visual.y + 8.0f,
        visual.width - text_input_label_width - 24.0f,
        visual.height - 16.0f,
    };
    draw_chip_list({content.x, content.y, content.width, 32.0f}, selected, g_theme->accent, 13, layer);

    const Rectangle input_rect = {content.x, content.y + 36.0f, 300.0f, 38.0f};
    const ui::text_input_result input_result = ui::draw_text_input(
        input_rect, search_input, "", maxed ? "Up to 3 genres" : "Search genre...",
        nullptr, layer, 14, 40, wide_text_filter, 0.0f);

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
                         ui::text_input_state& keyword_input,
                         ui::draw_layer layer,
                         float text_input_label_width) {
    const bool maxed = selected.size() >= kMaxSongKeywords;
    ui::draw_rect_f(row, maxed ? with_alpha(g_theme->row, 185) : g_theme->row);
    ui::draw_rect_lines(row, 1.5f, keyword_input.active ? g_theme->border_active : g_theme->border);
    draw_field_label(row, "Keywords", text_input_label_width);

    const Rectangle content = {
        row.x + text_input_label_width + 12.0f,
        row.y + 8.0f,
        row.width - text_input_label_width - 24.0f,
        row.height - 16.0f,
    };
    draw_chip_list({content.x, content.y, content.width, 32.0f}, selected, g_theme->fast, 13, layer);

    const Rectangle input_rect = {content.x, content.y + 38.0f, content.width - 112.0f, 38.0f};
    const Rectangle add_rect = {input_rect.x + input_rect.width + 10.0f, input_rect.y, 102.0f, 38.0f};
    const ui::text_input_result input_result = ui::draw_text_input(
        input_rect, keyword_input, "", maxed ? "Up to 5 keywords" : "Add keyword...",
        nullptr, layer, 14, kMaxSongKeywordLength, wide_text_filter, 0.0f);
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

}  // namespace song_create::tag_editor
