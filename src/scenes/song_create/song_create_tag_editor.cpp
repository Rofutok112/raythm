#include "song_create/song_create_tag_editor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string_view>

#include "theme.h"
#include "ui_layout.h"
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

constexpr float kFieldLabelInset = 12.0f;
constexpr float kFieldColumnGap = 12.0f;
constexpr float kFieldContentPaddingY = 8.0f;
constexpr float kFieldContentPaddingRight = 12.0f;
constexpr float kChipRowHeight = 32.0f;
constexpr float kChipHeight = 26.0f;
constexpr float kChipMinWidth = 82.0f;
constexpr float kChipMaxWidth = 190.0f;
constexpr float kChipTextPadding = 8.0f;
constexpr float kChipRemoveWidth = 16.0f;
constexpr float kChipRemoveRight = 22.0f;
constexpr float kChipGap = 8.0f;
constexpr float kChipRowStep = 30.0f;
constexpr float kInputHeight = 38.0f;
constexpr float kGenreInputTop = 36.0f;
constexpr float kGenreInputWidth = 300.0f;
constexpr float kSuggestionGap = 10.0f;
constexpr float kSuggestionHeight = 32.0f;
constexpr float kSuggestionMinWidth = 84.0f;
constexpr float kSuggestionMaxWidth = 150.0f;
constexpr float kSuggestionTextPadding = 22.0f;
constexpr float kSuggestionRowStep = 40.0f;
constexpr float kKeywordInputTop = 38.0f;
constexpr float kKeywordAddWidth = 102.0f;
constexpr float kKeywordAddGap = 10.0f;

struct tag_field_layout {
    Rectangle label;
    Rectangle content;
    Rectangle chips;
};

struct genre_selector_layout {
    tag_field_layout field;
    Rectangle input;
    Rectangle suggestions;
};

struct keyword_editor_layout {
    tag_field_layout field;
    Rectangle input;
    Rectangle add_button;
};

tag_field_layout tag_field_layout_for(Rectangle row, float text_input_label_width) {
    const ui::rect_pair columns = ui::split_columns(row, text_input_label_width, kFieldColumnGap);
    const Rectangle label = ui::inset(
        columns.first, ui::edge_insets{0.0f, 0.0f, 0.0f, kFieldLabelInset});
    const Rectangle content = ui::inset(
        columns.second, ui::edge_insets{kFieldContentPaddingY,
                                        kFieldContentPaddingRight,
                                        kFieldContentPaddingY,
                                        0.0f});
    return {
        label,
        content,
        {content.x, content.y, content.width, kChipRowHeight},
    };
}

genre_selector_layout genre_selector_layout_for(Rectangle row, float text_input_label_width) {
    tag_field_layout field = tag_field_layout_for(row, text_input_label_width);
    const Rectangle input = {field.content.x, field.content.y + kGenreInputTop,
                             kGenreInputWidth, kInputHeight};
    const float suggestions_x = input.x + input.width + kSuggestionGap;
    const Rectangle suggestions = {
        suggestions_x,
        input.y,
        std::max(0.0f, field.content.x + field.content.width - suggestions_x),
        field.content.y + field.content.height - input.y,
    };
    return {field, input, suggestions};
}

keyword_editor_layout keyword_editor_layout_for(Rectangle row, float text_input_label_width) {
    tag_field_layout field = tag_field_layout_for(row, text_input_label_width);
    const Rectangle input_row = {field.content.x, field.content.y + kKeywordInputTop,
                                 field.content.width, kInputHeight};
    const ui::rect_pair controls = ui::split_trailing(input_row, kKeywordAddWidth, kKeywordAddGap);
    return {field, controls.first, controls.second};
}

std::optional<Rectangle> take_wrapped_rect(Rectangle bounds,
                                           float width,
                                           float height,
                                           float gap,
                                           float row_step,
                                           float& x,
                                           float& y) {
    if (x + width > bounds.x + bounds.width && x > bounds.x) {
        x = bounds.x;
        y += row_step;
    }
    if (y + height > bounds.y + bounds.height) {
        return std::nullopt;
    }
    Rectangle result = {x, y, width, height};
    x += width + gap;
    return result;
}

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

void draw_field_label(Rectangle label_rect, const char* label) {
    ui::draw_text_in_rect(label, 16, label_rect, g_theme->text_secondary, ui::text_align::left);
}

std::optional<size_t> draw_chip_list(Rectangle rect,
                                     const std::vector<std::string>& values,
                                     Color accent,
                                     int font_size,
                                     ui::draw_layer layer) {
    std::optional<size_t> remove_index;
    float x = rect.x;
    float y = rect.y;
    for (size_t index = 0; index < values.size(); ++index) {
        const std::string& value = values[index];
        const float measured = ui::measure_text_size(value.c_str(), static_cast<float>(font_size)).x;
        const float width = std::min(std::max(kChipMinWidth, measured + 34.0f), kChipMaxWidth);
        const std::optional<Rectangle> chip_rect =
            take_wrapped_rect(rect, width, kChipHeight, kChipGap, kChipRowStep, x, y);
        if (!chip_rect.has_value()) {
            break;
        }
        const Rectangle chip = *chip_rect;
        const bool hovered = ui::is_hovered(chip, layer);
        ui::surface(chip, with_alpha(g_theme->section, hovered ? 255 : 225), accent, 1.2f);
        ui::draw_text_in_rect(value.c_str(), font_size,
                              {chip.x + kChipTextPadding, chip.y,
                               chip.width - kChipRemoveRight - kChipTextPadding, chip.height},
                              g_theme->text, ui::text_align::left);
        ui::draw_text_in_rect("x", font_size,
                              {chip.x + chip.width - kChipRemoveRight, chip.y,
                               kChipRemoveWidth, chip.height},
                              g_theme->text_muted);
        if (!remove_index.has_value() && ui::is_clicked(chip, layer)) {
            remove_index = index;
        }
    }
    return remove_index;
}

std::optional<std::string> draw_genre_suggestions(Rectangle suggestions_rect,
                                                  const std::vector<std::string>& suggestions,
                                                  bool maxed,
                                                  ui::draw_layer layer) {
    std::optional<std::string> add_label;
    float x = suggestions_rect.x;
    float y = suggestions_rect.y;
    for (const std::string& suggestion : suggestions) {
        const float text_width = ui::measure_text_size(suggestion.c_str(), 13.0f).x;
        const float width =
            std::min(std::max(kSuggestionMinWidth, text_width + kSuggestionTextPadding),
                     kSuggestionMaxWidth);
        const std::optional<Rectangle> button_rect =
            take_wrapped_rect(suggestions_rect, width, kSuggestionHeight, kChipGap,
                              kSuggestionRowStep, x, y);
        if (!button_rect.has_value()) {
            break;
        }
        const Color base = maxed ? with_alpha(g_theme->section, 120) : with_alpha(g_theme->section, 235);
        const Color hover = maxed ? with_alpha(g_theme->section, 120) : with_alpha(g_theme->row_hover, 255);
        const ui::button_state state = ui::button(*button_rect, suggestion.c_str(), {
            .layer = layer,
            .font_size = 13,
            .border_width = 1.2f,
            .bg = base,
            .bg_hover = hover,
            .text_color = maxed ? g_theme->text_muted : g_theme->text,
            .custom_colors = true,
        });
        if (!maxed && state.clicked && !add_label.has_value()) {
            add_label = suggestion;
        }
    }
    return add_label;
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

genre_selector_result draw_genre_selector(Rectangle row,
                                          const std::vector<std::string>& selected,
                                          ui::text_input_state& search_input,
                                          ui::draw_layer layer,
                                          float text_input_label_width) {
    genre_selector_result result;
    const bool maxed = selected.size() >= kMaxSongGenres;
    const genre_selector_layout layout = genre_selector_layout_for(row, text_input_label_width);
    ui::surface(row,
                maxed ? with_alpha(g_theme->row, 185) : g_theme->row,
                search_input.active ? g_theme->border_active : g_theme->border,
                1.5f);
    draw_field_label(layout.field.label, "Genres");
    result.remove_index = draw_chip_list(layout.field.chips, selected, g_theme->accent, 13, layer);

    const ui::text_input_result input_result = ui::text_input(
        layout.input, search_input, "", maxed ? "Up to 3 genres" : "Search genre...", {
            .layer = layer,
            .font_size = 14,
            .max_length = 40,
            .filter = wide_text_filter,
            .label_width = 0.0f,
        });

    const std::vector<std::string> suggestions = genre_suggestions(search_input.value, selected);
    result.add_label = draw_genre_suggestions(layout.suggestions, suggestions, maxed, layer);

    if (!maxed && input_result.submitted) {
        const std::vector<std::string> matches = genre_suggestions(search_input.value, selected);
        if (!matches.empty()) {
            result.add_label = matches.front();
        }
    }
    return result;
}

keyword_editor_result draw_keyword_editor(Rectangle row,
                                          const std::vector<std::string>& selected,
                                          ui::text_input_state& keyword_input,
                                          ui::draw_layer layer,
                                          float text_input_label_width) {
    keyword_editor_result result;
    const bool maxed = selected.size() >= kMaxSongKeywords;
    const keyword_editor_layout layout = keyword_editor_layout_for(row, text_input_label_width);
    ui::surface(row,
                maxed ? with_alpha(g_theme->row, 185) : g_theme->row,
                keyword_input.active ? g_theme->border_active : g_theme->border,
                1.5f);
    draw_field_label(layout.field.label, "Keywords");
    result.remove_index = draw_chip_list(layout.field.chips, selected, g_theme->fast, 13, layer);

    const ui::text_input_result input_result = ui::text_input(
        layout.input, keyword_input, "", maxed ? "Up to 5 keywords" : "Add keyword...", {
            .layer = layer,
            .font_size = 14,
            .max_length = kMaxSongKeywordLength,
            .filter = wide_text_filter,
            .label_width = 0.0f,
        });
    const ui::button_state add_button = ui::button(layout.add_button, "ADD", {
        .layer = layer,
        .font_size = 14,
        .border_width = 1.2f,
        .bg = maxed ? with_alpha(g_theme->section, 120) : g_theme->section,
        .bg_hover = maxed ? with_alpha(g_theme->section, 120) : g_theme->row_hover,
        .text_color = maxed ? g_theme->text_muted : g_theme->text,
        .custom_colors = true,
    });
    if (!maxed && (input_result.submitted || add_button.clicked)) {
        result.add_requested = true;
    }
    return result;
}

void apply_genre_selector_result(std::vector<std::string>& selected,
                                 ui::text_input_state& search_input,
                                 const genre_selector_result& result) {
    if (result.remove_index.has_value() && *result.remove_index < selected.size()) {
        selected.erase(selected.begin() + static_cast<std::ptrdiff_t>(*result.remove_index));
    }
    if (result.add_label.has_value()) {
        add_genre(selected, search_input, *result.add_label);
    }
}

void apply_keyword_editor_result(std::vector<std::string>& selected,
                                 ui::text_input_state& keyword_input,
                                 const keyword_editor_result& result) {
    if (result.remove_index.has_value() && *result.remove_index < selected.size()) {
        selected.erase(selected.begin() + static_cast<std::ptrdiff_t>(*result.remove_index));
    }
    if (result.add_requested) {
        add_keyword(selected, keyword_input);
    }
}

}  // namespace song_create::tag_editor
