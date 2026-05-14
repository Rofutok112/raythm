#pragma once

#include <string>
#include <vector>

#include "raylib.h"

namespace ui {

enum class font_locale_mode {
    automatic,
    japanese_ui,
};

enum class text_role {
    ui_body,
    display,
};

void set_font_locale_mode(font_locale_mode mode);
void initialize_text_font();
void shutdown_text_font();

Font text_font();
Font text_font_for_text(const char* text);
Font body_font();
Font display_font();
float text_layout_font_size(float font_size);
Font text_font(text_role role);
float text_font_size(text_role role, float font_size);
float text_spacing(text_role role, float font_size, float spacing = 0.0f);
float text_font_size_for_text(const char* text, float font_size);
float text_spacing_for_text(const char* text, float font_size, float spacing = 0.0f);
float body_font_size(float font_size);
float body_spacing(float spacing = 0.0f);
float display_font_size(float font_size);
float display_spacing(float font_size, float spacing = 0.0f);
void ensure_text_glyphs(const char* text);
void preload_text_glyphs(const std::vector<std::string>& texts);
Vector2 measure_text_size(text_role role, const char* text, float font_size, float spacing = 0.0f);
Vector2 measure_text_size(const char* text, float font_size, float spacing = 0.0f);
Vector2 measure_body_text_size(const char* text, float font_size, float spacing = 0.0f);
Vector2 measure_display_text_size(const char* text, float font_size, float spacing = 0.0f);
void draw_text(text_role role, const char* text, Vector2 position, float font_size, float spacing, Color color);
void draw_text_auto(const char* text, Vector2 position, float font_size, float spacing, Color color);
void draw_text_body(const char* text, Vector2 position, float font_size, float spacing, Color color);
void draw_text_display(const char* text, Vector2 position, float font_size, float spacing, Color color);

inline Vector2 measure_text_size(const std::string& text, float font_size, float spacing = 0.0f) {
    return measure_text_size(text.c_str(), font_size, spacing);
}

}  // namespace ui
