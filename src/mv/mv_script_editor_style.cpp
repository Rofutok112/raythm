#include "mv_script_editor_style.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

#include "scenes/theme.h"

namespace mv {

namespace {

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_completion_char(char c) {
    return is_ident_char(c) || c == '.';
}

std::string trim_comment(const std::string& line) {
    const size_t comment = line.find('#');
    return comment == std::string::npos ? line : line.substr(0, comment);
}

int leading_indent_width(const std::string& line) {
    int width = 0;
    for (char c : line) {
        if (c == ' ') {
            ++width;
        } else if (c == '\t') {
            width += 4;
        } else {
            break;
        }
    }
    return width;
}

std::string ltrim_copy(const std::string& text) {
    size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
    return text.substr(i);
}

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string parse_leading_identifier(const std::string& text, size_t start = 0, size_t* end_out = nullptr) {
    if (start >= text.size() || !is_ident_start(text[start])) {
        return {};
    }
    size_t i = start + 1;
    while (i < text.size() && is_ident_char(text[i])) {
        ++i;
    }
    if (end_out != nullptr) {
        *end_out = i;
    }
    return text.substr(start, i - start);
}

std::pair<std::string, bool> parse_assignment_target(const std::string& trimmed) {
    size_t ident_end = 0;
    const std::string name = parse_leading_identifier(trimmed, 0, &ident_end);
    if (name.empty()) {
        return {"", false};
    }

    size_t i = ident_end;
    while (i < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[i]))) {
        ++i;
    }
    if (i >= trimmed.size()) {
        return {"", false};
    }

    std::string op;
    if (trimmed.compare(i, 2, "+=") == 0 || trimmed.compare(i, 2, "-=") == 0 ||
        trimmed.compare(i, 2, "*=") == 0 || trimmed.compare(i, 2, "/=") == 0 ||
        trimmed.compare(i, 2, "%=") == 0) {
        op = trimmed.substr(i, 2);
        i += 2;
    } else if (trimmed[i] == '=' && (i + 1 >= trimmed.size() || trimmed[i + 1] != '=')) {
        op = "=";
        ++i;
    } else {
        return {"", false};
    }

    while (i < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[i]))) {
        ++i;
    }
    const bool is_list_assignment = (op == "=") && trimmed.compare(i, 2, "[]") == 0;
    return {name, is_list_assignment};
}

std::vector<std::string> parse_def_params(const std::string& trimmed) {
    std::vector<std::string> params;
    if (!starts_with(trimmed, "def ")) {
        return params;
    }

    const size_t lparen = trimmed.find('(');
    const size_t rparen = trimmed.find(')', lparen == std::string::npos ? 0 : lparen + 1);
    if (lparen == std::string::npos || rparen == std::string::npos || rparen <= lparen + 1) {
        return params;
    }

    size_t i = lparen + 1;
    while (i < rparen) {
        while (i < rparen && (std::isspace(static_cast<unsigned char>(trimmed[i])) || trimmed[i] == ',')) {
            ++i;
        }
        size_t end = 0;
        const std::string ident = parse_leading_identifier(trimmed, i, &end);
        if (!ident.empty()) {
            params.push_back(ident);
            i = end;
        } else {
            ++i;
        }
    }
    return params;
}

std::string parse_def_name(const std::string& trimmed) {
    if (!starts_with(trimmed, "def ")) {
        return {};
    }
    return parse_leading_identifier(trimmed, 4);
}

std::string parse_for_var(const std::string& trimmed) {
    if (!starts_with(trimmed, "for ")) {
        return {};
    }
    return parse_leading_identifier(trimmed, 4);
}

struct completion_scope {
    int min_indent = 0;
    std::unordered_set<std::string> names;
    std::unordered_set<std::string> list_names;
};

struct discovered_symbols {
    std::vector<std::string> visible_names;
    std::unordered_set<std::string> list_names;
};

discovered_symbols collect_visible_symbols(const std::vector<std::string>& lines, int cursor_line, int cursor_col) {
    std::vector<completion_scope> scopes;
    scopes.push_back({});

    for (int line_index = 0; line_index <= cursor_line && line_index < static_cast<int>(lines.size()); ++line_index) {
        std::string line = lines[static_cast<size_t>(line_index)];
        if (line_index == cursor_line) {
            line = line.substr(0, static_cast<size_t>(std::clamp(cursor_col, 0, static_cast<int>(line.size()))));
        }
        line = trim_comment(line);
        const int indent = leading_indent_width(line);

        while (scopes.size() > 1 && indent < scopes.back().min_indent) {
            scopes.pop_back();
        }

        std::string trimmed = ltrim_copy(line);
        if (trimmed.empty()) {
            continue;
        }

        std::vector<std::string> next_scope_names;
        std::vector<std::string> next_scope_list_names;

        const std::string def_name = parse_def_name(trimmed);
        if (!def_name.empty()) {
            scopes.back().names.insert(def_name);
            next_scope_names = parse_def_params(trimmed);
        }

        const std::string for_var = parse_for_var(trimmed);
        if (!for_var.empty()) {
            scopes.back().names.insert(for_var);
            next_scope_names.push_back(for_var);
        }

        const auto [assigned_name, is_list_assignment] = parse_assignment_target(trimmed);
        if (!assigned_name.empty()) {
            scopes.back().names.insert(assigned_name);
            if (is_list_assignment) {
                scopes.back().list_names.insert(assigned_name);
            }
        }

        if (!trimmed.empty() && trimmed.back() == ':') {
            completion_scope child_scope;
            child_scope.min_indent = indent + 1;
            for (const auto& name : next_scope_names) {
                child_scope.names.insert(name);
            }
            for (const auto& name : next_scope_list_names) {
                child_scope.list_names.insert(name);
            }
            scopes.push_back(std::move(child_scope));
        }
    }

    std::unordered_set<std::string> seen_names;
    discovered_symbols symbols;
    for (const auto& scope : scopes) {
        for (const auto& name : scope.names) {
            if (seen_names.insert(name).second) {
                symbols.visible_names.push_back(name);
            }
        }
        for (const auto& name : scope.list_names) {
            symbols.list_names.insert(name);
        }
    }
    std::sort(symbols.visible_names.begin(), symbols.visible_names.end());
    return symbols;
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

void append_matches(std::vector<ui::text_editor_completion_item>& items,
                    const std::vector<ui::text_editor_completion_item>& source,
                    const std::string& prefix,
                    std::unordered_set<std::string>& seen_labels) {
    for (const auto& item : source) {
        if ((prefix.empty() || item.label.rfind(prefix, 0) == 0) &&
            !(item.label == prefix && item.insert_text == prefix) &&
            seen_labels.insert(item.label).second) {
            items.push_back(item);
        }
    }
}

Color identifier_color(const std::string& ident) {
    if (is_in_list(ident, {"def", "return", "if", "elif", "else", "for", "in", "and", "or", "not",
                           "True", "False", "None"})) {
        return keyword_color();
    }
    if (is_in_list(ident, {"Scene", "Point", "DrawBackground", "DrawRect", "DrawCircle", "DrawLine", "DrawText",
                           "DrawPolyline"})) {
        return node_color();
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

ui::text_editor_completion_result complete_mv_script_line(const std::vector<std::string>& lines,
                                                          int cursor_line, int cursor_col) {
    ui::text_editor_completion_result result;
    if (cursor_line < 0 || cursor_line >= static_cast<int>(lines.size())) {
        return result;
    }

    const std::string& line = lines[static_cast<size_t>(cursor_line)];
    const int clamped_col = std::clamp(cursor_col, 0, static_cast<int>(line.size()));

    int start = clamped_col;
    while (start > 0 && is_completion_char(line[start - 1])) {
        --start;
    }
    int end = clamped_col;
    while (end < static_cast<int>(line.size()) && is_completion_char(line[end])) {
        ++end;
    }

    const std::string token = line.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
    if (token.empty()) {
        return result;
    }

    static const std::vector<ui::text_editor_completion_item> kRootItems = {
        {"def", "def"},
        {"return", "return"},
        {"if", "if"},
        {"elif", "elif"},
        {"else", "else"},
        {"for", "for"},
        {"in", "in"},
        {"and", "and"},
        {"or", "or"},
        {"not", "not"},
        {"Scene", "Scene("},
        {"Point", "Point("},
        {"DrawBackground", "DrawBackground("},
        {"DrawRect", "DrawRect("},
        {"DrawCircle", "DrawCircle("},
        {"DrawLine", "DrawLine("},
        {"DrawText", "DrawText("},
        {"DrawPolyline", "DrawPolyline("},
        {"sin", "sin("},
        {"cos", "cos("},
        {"abs", "abs("},
        {"floor", "floor("},
        {"ceil", "ceil("},
        {"sqrt", "sqrt("},
        {"min", "min("},
        {"max", "max("},
        {"clamp", "clamp("},
        {"lerp", "lerp("},
        {"smoothstep", "smoothstep("},
        {"rgb", "rgb("},
        {"len", "len("},
        {"range", "range("},
        {"str", "str("},
        {"int", "int("},
        {"float", "float("},
        {"pi", "pi()"},
        {"ctx", "ctx"},
    };

    static const std::vector<ui::text_editor_completion_item> kCtxItems = {
        {"ctx.time", "ctx.time"},
        {"ctx.audio", "ctx.audio"},
        {"ctx.song", "ctx.song"},
        {"ctx.chart", "ctx.chart"},
        {"ctx.screen", "ctx.screen"},
    };

    static const std::vector<ui::text_editor_completion_item> kTimeItems = {
        {"ctx.time.ms", "ctx.time.ms"},
        {"ctx.time.sec", "ctx.time.sec"},
        {"ctx.time.length_ms", "ctx.time.length_ms"},
        {"ctx.time.bpm", "ctx.time.bpm"},
        {"ctx.time.beat", "ctx.time.beat"},
        {"ctx.time.beat_phase", "ctx.time.beat_phase"},
        {"ctx.time.meter_numerator", "ctx.time.meter_numerator"},
        {"ctx.time.meter_denominator", "ctx.time.meter_denominator"},
        {"ctx.time.progress", "ctx.time.progress"},
    };

    static const std::vector<ui::text_editor_completion_item> kAudioItems = {
        {"ctx.audio.analysis", "ctx.audio.analysis"},
        {"ctx.audio.bands", "ctx.audio.bands"},
        {"ctx.audio.buffers", "ctx.audio.buffers"},
    };

    static const std::vector<ui::text_editor_completion_item> kAudioAnalysisItems = {
        {"ctx.audio.analysis.level", "ctx.audio.analysis.level"},
        {"ctx.audio.analysis.rms", "ctx.audio.analysis.rms"},
        {"ctx.audio.analysis.peak", "ctx.audio.analysis.peak"},
    };

    static const std::vector<ui::text_editor_completion_item> kAudioBandsItems = {
        {"ctx.audio.bands.low", "ctx.audio.bands.low"},
        {"ctx.audio.bands.mid", "ctx.audio.bands.mid"},
        {"ctx.audio.bands.high", "ctx.audio.bands.high"},
    };

    static const std::vector<ui::text_editor_completion_item> kAudioBufferItems = {
        {"ctx.audio.buffers.spectrum", "ctx.audio.buffers.spectrum"},
        {"ctx.audio.buffers.spectrum_size", "ctx.audio.buffers.spectrum_size"},
        {"ctx.audio.buffers.oscilloscope", "ctx.audio.buffers.oscilloscope"},
        {"ctx.audio.buffers.oscilloscope_size", "ctx.audio.buffers.oscilloscope_size"},
        {"ctx.audio.buffers.waveform", "ctx.audio.buffers.waveform"},
        {"ctx.audio.buffers.waveform_size", "ctx.audio.buffers.waveform_size"},
        {"ctx.audio.buffers.waveform_index", "ctx.audio.buffers.waveform_index"},
    };

    static const std::vector<ui::text_editor_completion_item> kSongItems = {
        {"ctx.song.song_id", "ctx.song.song_id"},
        {"ctx.song.title", "ctx.song.title"},
        {"ctx.song.artist", "ctx.song.artist"},
        {"ctx.song.base_bpm", "ctx.song.base_bpm"},
    };

    static const std::vector<ui::text_editor_completion_item> kChartItems = {
        {"ctx.chart.chart_id", "ctx.chart.chart_id"},
        {"ctx.chart.song_id", "ctx.chart.song_id"},
        {"ctx.chart.difficulty", "ctx.chart.difficulty"},
        {"ctx.chart.level", "ctx.chart.level"},
        {"ctx.chart.chart_author", "ctx.chart.chart_author"},
        {"ctx.chart.resolution", "ctx.chart.resolution"},
        {"ctx.chart.offset", "ctx.chart.offset"},
        {"ctx.chart.total_notes", "ctx.chart.total_notes"},
        {"ctx.chart.combo", "ctx.chart.combo"},
        {"ctx.chart.accuracy", "ctx.chart.accuracy"},
        {"ctx.chart.key_count", "ctx.chart.key_count"},
    };

    static const std::vector<ui::text_editor_completion_item> kScreenItems = {
        {"ctx.screen.w", "ctx.screen.w"},
        {"ctx.screen.h", "ctx.screen.h"},
    };

    static const std::vector<ui::text_editor_completion_item> kListItems = {
        {"append", "append("},
    };

    const discovered_symbols symbols = collect_visible_symbols(lines, cursor_line, cursor_col);
    std::vector<ui::text_editor_completion_item> matches;
    std::unordered_set<std::string> seen_labels;
    if (token.rfind("ctx.time.", 0) == 0) {
        append_matches(matches, kTimeItems, token, seen_labels);
    } else if (token.rfind("ctx.audio.analysis.", 0) == 0) {
        append_matches(matches, kAudioAnalysisItems, token, seen_labels);
    } else if (token.rfind("ctx.audio.bands.", 0) == 0) {
        append_matches(matches, kAudioBandsItems, token, seen_labels);
    } else if (token.rfind("ctx.audio.buffers.", 0) == 0) {
        append_matches(matches, kAudioBufferItems, token, seen_labels);
    } else if (token.rfind("ctx.audio.", 0) == 0) {
        append_matches(matches, kAudioItems, token, seen_labels);
    } else if (token.rfind("ctx.song.", 0) == 0) {
        append_matches(matches, kSongItems, token, seen_labels);
    } else if (token.rfind("ctx.chart.", 0) == 0) {
        append_matches(matches, kChartItems, token, seen_labels);
    } else if (token.rfind("ctx.screen.", 0) == 0) {
        append_matches(matches, kScreenItems, token, seen_labels);
    } else if (token.rfind("ctx.", 0) == 0 || token == "ctx") {
        append_matches(matches, kCtxItems, token, seen_labels);
    } else if (const size_t dot = token.rfind('.'); dot != std::string::npos) {
        const std::string base = token.substr(0, dot);
        const std::string member_prefix = token.substr(dot + 1);
        if (symbols.list_names.contains(base)) {
            std::vector<ui::text_editor_completion_item> list_matches;
            append_matches(list_matches, kListItems, member_prefix, seen_labels);
            for (const auto& item : list_matches) {
                matches.push_back({base + "." + item.label, base + "." + item.insert_text});
            }
        }
    } else {
        for (const auto& name : symbols.visible_names) {
            if ((token.empty() || name.rfind(token, 0) == 0) && name != token) {
                if (seen_labels.insert(name).second) {
                    matches.push_back({name, name});
                }
            }
        }
        append_matches(matches, kRootItems, token, seen_labels);
    }

    if (matches.empty()) {
        return result;
    }

    result.replace_start = start;
    result.replace_end = end;
    result.items = std::move(matches);
    return result;
}

} // namespace mv
