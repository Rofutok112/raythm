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

void draw_genre_selector(Rectangle row,
                         std::vector<std::string>& selected,
                         ui::text_input_state& search_input,
                         ui::draw_layer layer,
                         float text_input_label_width);
void draw_keyword_editor(Rectangle row,
                         std::vector<std::string>& selected,
                         ui::text_input_state& keyword_input,
                         ui::draw_layer layer,
                         float text_input_label_width);

}  // namespace song_create::tag_editor
