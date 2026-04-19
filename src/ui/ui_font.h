#pragma once

#include <string>

#include "raylib.h"

namespace ui {

void initialize_text_font();
void shutdown_text_font();

Font text_font();
Font text_font_for_text(const char* text);
float text_font_size_for_text(const char* text, float font_size);
float text_spacing_for_text(const char* text, float font_size, float spacing = 0.0f);
void ensure_text_glyphs(const char* text);
Vector2 measure_text_size(const char* text, float font_size, float spacing = 0.0f);
void draw_text_auto(const char* text, Vector2 position, float font_size, float spacing, Color color);

inline Vector2 measure_text_size(const std::string& text, float font_size, float spacing = 0.0f) {
    return measure_text_size(text.c_str(), font_size, spacing);
}

}  // namespace ui
