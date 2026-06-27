#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "data_models.h"
#include "raylib.h"
#include "ui_draw.h"
#include "ui_text_input.h"

namespace song_create::tag_editor {

inline constexpr size_t kMaxSongGenres = 3;
inline constexpr size_t kMaxSongKeywords = 5;
inline constexpr size_t kMaxSongKeywordLength = 40;

std::string trim_ascii(std::string_view value);
std::vector<std::string> normalize_genres_for_editor(const song_meta& meta);
std::vector<std::string> normalize_keywords_for_editor(const song_meta& meta);

struct genre_selector_result {
    std::optional<size_t> remove_index;
    std::optional<std::string> add_label;
};

struct keyword_editor_result {
    std::optional<size_t> remove_index;
    bool add_requested = false;
};

[[nodiscard]] genre_selector_result draw_genre_selector(Rectangle row,
                                                        const std::vector<std::string>& selected,
                                                        ui::text_input_state& search_input,
                                                        ui::draw_layer layer,
                                                        float text_input_label_width);
[[nodiscard]] keyword_editor_result draw_keyword_editor(Rectangle row,
                                                        const std::vector<std::string>& selected,
                                                        ui::text_input_state& keyword_input,
                                                        ui::draw_layer layer,
                                                        float text_input_label_width);
void apply_genre_selector_result(std::vector<std::string>& selected,
                                 ui::text_input_state& search_input,
                                 const genre_selector_result& result);
void apply_keyword_editor_result(std::vector<std::string>& selected,
                                 ui::text_input_state& keyword_input,
                                 const keyword_editor_result& result);

}  // namespace song_create::tag_editor
