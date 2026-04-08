#include "mv_script_editor_style.h"

#include <cctype>

#include "scenes/theme.h"

namespace mv {

namespace {

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_in_list(const std::string& value, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (value == name) {
            return true;
        }
    }
    return false;
}

Color keyword_color() { return g_theme->accent; }
Color builtin_color() { return g_theme->success; }
Color node_color() { return g_theme->fast; }
Color string_color() { return g_theme->slow; }
Color comment_color() { return g_theme->text_hint; }
Color number_color() { return g_theme->fast; }
Color punctuation_color() { return g_theme->text_dim; }

Color identifier_color(const std::string& ident) {
    if (is_in_list(ident, {"def", "return", "if", "elif", "else", "for", "in", "and", "or", "not",
                           "True", "False", "None"})) {
        return keyword_color();
    }
    if (is_in_list(ident, {"Scene", "Point", "Background", "Rect", "Circle", "Line", "Text",
                           "Polyline"})) {
        return node_color();
    }
    if (is_in_list(ident, {"SpectrumBar", "BeatGrid", "PulseRing"})) {
        return g_theme->text_hint;
    }
    if (is_in_list(ident, {"sin", "cos", "abs", "floor", "ceil", "sqrt", "min", "max",
                           "clamp", "lerp", "smoothstep", "rgb", "len", "range",
                           "str", "int", "float", "pi"})) {
        return builtin_color();
    }
    if (ident == "ctx") {
        return g_theme->text_secondary;
    }
    return g_theme->text;
}

} // namespace

const ui::text_editor_style& mv_script_editor_style() {
    static const ui::text_editor_style style{
        .font_size = 20,
        .line_spacing = 4.0f,
        .letter_spacing = 2.0f,
    };
    return style;
}

std::vector<ui::text_editor_span> highlight_mv_script_line(const std::string& line) {
    std::vector<ui::text_editor_span> spans;
    size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];

        if (c == '#') {
            spans.push_back({line.substr(i), comment_color()});
            break;
        }

        if (c == '"' || c == '\'') {
            const char quote = c;
            size_t j = i + 1;
            while (j < line.size()) {
                if (line[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (line[j] == quote) {
                    ++j;
                    break;
                }
                ++j;
            }
            spans.push_back({line.substr(i, j - i), string_color()});
            i = j;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t j = i + 1;
            while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j]))) {
                ++j;
            }
            if (j < line.size() && line[j] == '.') {
                ++j;
                while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j]))) {
                    ++j;
                }
            }
            spans.push_back({line.substr(i, j - i), number_color()});
            i = j;
            continue;
        }

        if (is_ident_start(c)) {
            size_t j = i + 1;
            while (j < line.size() && is_ident_char(line[j])) {
                ++j;
            }
            const std::string ident = line.substr(i, j - i);
            spans.push_back({ident, identifier_color(ident)});
            i = j;
            continue;
        }

        const Color color = std::isspace(static_cast<unsigned char>(c)) ? g_theme->text : punctuation_color();
        spans.push_back({line.substr(i, 1), color});
        ++i;
    }
    return spans;
}

} // namespace mv
