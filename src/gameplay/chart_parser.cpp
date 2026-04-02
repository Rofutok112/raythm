#include "chart_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

#include "path_utils.h"

namespace {
using numbered_line = std::pair<int, std::string>;

std::string trim(std::string_view value) {
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

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream stream(line);
    std::string token;

    while (std::getline(stream, token, ',')) {
        tokens.push_back(trim(token));
    }

    return tokens;
}

std::optional<int> parse_int(const std::string& value) {
    try {
        size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> parse_float(const std::string& value) {
    try {
        size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::string format_line_error(int line_number, const std::string& message) {
    return "Line " + std::to_string(line_number) + ": " + message;
}

std::optional<std::pair<std::string, std::string>> parse_metadata_entry(const numbered_line& line) {
    const size_t equals_pos = line.second.find('=');
    if (equals_pos != std::string::npos) {
        const std::string key = trim(line.second.substr(0, equals_pos));
        const std::string value = trim(line.second.substr(equals_pos + 1));
        if (!key.empty() && !value.empty()) {
            return std::make_pair(key, value);
        }
    }

    const std::vector<std::string> tokens = split_csv_line(line.second);
    if (tokens.size() == 2 && !tokens[0].empty() && !tokens[1].empty()) {
        return std::make_pair(tokens[0], tokens[1]);
    }

    return std::nullopt;
}

std::optional<timing_event_type> parse_timing_type(const std::string& value) {
    if (value == "bpm") {
        return timing_event_type::bpm;
    }
    if (value == "meter") {
        return timing_event_type::meter;
    }
    return std::nullopt;
}

std::optional<note_type> parse_note_type(const std::string& value) {
    if (value == "tap") {
        return note_type::tap;
    }
    if (value == "hold") {
        return note_type::hold;
    }
    return std::nullopt;
}
}

chart_parse_result chart_parser::parse(const std::string& file_path) {
    std::ifstream input(path_utils::from_utf8(file_path));
    if (!input.is_open()) {
        chart_parse_result result;
        result.success = false;
        result.errors.push_back("Failed to open chart file: " + file_path);
        return result;
    }

    std::map<std::string, std::vector<numbered_line>> sections;
    std::string current_section;
    std::string line;
    int line_number = 0;
    std::vector<std::string> errors;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trimmed.substr(1, trimmed.size() - 2);
            if (sections.find(current_section) != sections.end()) {
                errors.push_back(format_line_error(line_number, "Duplicate section [" + current_section + "]"));
            } else {
                sections[current_section] = {};
            }
            continue;
        }

        if (current_section.empty()) {
            errors.push_back(format_line_error(line_number, "Entry exists before any section header"));
            continue;
        }

        sections[current_section].emplace_back(line_number, trimmed);
    }

    const std::array<std::string, 3> required_sections = {"Metadata", "Timing", "Notes"};
    for (const std::string& section : required_sections) {
        if (sections.find(section) == sections.end()) {
            errors.push_back("Missing required section [" + section + "]");
        }
    }

    if (!errors.empty()) {
        chart_parse_result result;
        result.success = false;
        result.errors = std::move(errors);
        return result;
    }

    chart_data data;
    data.meta = parse_metadata(sections["Metadata"], errors);
    data.timing_events = parse_timing(sections["Timing"], errors);
    data.notes = parse_notes(sections["Notes"], errors);

    if (errors.empty()) {
        std::vector<std::string> validation_errors = validate(data);
        errors.insert(errors.end(), validation_errors.begin(), validation_errors.end());
    }

    if (!errors.empty()) {
        chart_parse_result result;
        result.success = false;
        result.errors = std::move(errors);
        return result;
    }

    chart_parse_result result;
    result.success = true;
    result.data = std::move(data);
    return result;
}

chart_meta chart_parser::parse_metadata(const std::vector<numbered_line>& lines, std::vector<std::string>& errors) {
    chart_meta meta;
    std::set<std::string> seen_keys;

    for (const numbered_line& line : lines) {
        const std::optional<std::pair<std::string, std::string>> entry = parse_metadata_entry(line);
        if (!entry.has_value()) {
            errors.push_back(format_line_error(line.first, "Metadata entry must be 'key=value' or 'key,value'"));
            continue;
        }

        const std::string& key = entry->first;
        const std::string& value = entry->second;

        if (!seen_keys.insert(key).second) {
            errors.push_back(format_line_error(line.first, "Duplicate metadata key: " + key));
            continue;
        }

        if (key == "chartId") {
            meta.chart_id = value;
        } else if (key == "keyCount") {
            const std::optional<int> parsed = parse_int(value);
            if (!parsed.has_value()) {
                errors.push_back(format_line_error(line.first, "keyCount must be an integer"));
            } else {
                meta.key_count = *parsed;
            }
        } else if (key == "difficulty") {
            meta.difficulty = value;
        } else if (key == "level") {
            const std::optional<int> parsed = parse_int(value);
            if (!parsed.has_value()) {
                errors.push_back(format_line_error(line.first, "level must be an integer"));
            } else {
                meta.level = *parsed;
            }
        } else if (key == "chartAuthor") {
            meta.chart_author = value;
        } else if (key == "formatVersion") {
            const std::optional<int> parsed = parse_int(value);
            if (!parsed.has_value()) {
                errors.push_back(format_line_error(line.first, "formatVersion must be an integer"));
            } else {
                meta.format_version = *parsed;
            }
        } else if (key == "resolution") {
            const std::optional<int> parsed = parse_int(value);
            if (!parsed.has_value()) {
                errors.push_back(format_line_error(line.first, "resolution must be an integer"));
            } else {
                meta.resolution = *parsed;
            }
        } else if (key == "offset") {
            const std::optional<int> parsed = parse_int(value);
            if (!parsed.has_value()) {
                errors.push_back(format_line_error(line.first, "offset must be an integer"));
            } else {
                meta.offset = *parsed;
            }
        } else if (key == "songId") {
            meta.song_id = value;
        } else if (key == "chartName") {
            meta.chart_name = value;
        } else if (key == "isPublic") {
            meta.is_public = (value == "true");
        } else if (key == "description") {
            meta.description = value;
        }
    }

    const std::array<std::string, 7> required_fields = {
        "chartId",
        "keyCount",
        "difficulty",
        "level",
        "chartAuthor",
        "formatVersion",
        "resolution",
    };

    for (const std::string& name : required_fields) {
        if (seen_keys.find(name) == seen_keys.end()) {
            errors.push_back("Missing required metadata field: " + name);
        }
    }

    return meta;
}

std::vector<timing_event> chart_parser::parse_timing(const std::vector<numbered_line>& lines, std::vector<std::string>& errors) {
    std::vector<timing_event> events;

    for (const numbered_line& line : lines) {
        const std::vector<std::string> tokens = split_csv_line(line.second);
        if (tokens.empty()) {
            continue;
        }

        const std::optional<timing_event_type> type = parse_timing_type(tokens[0]);
        if (!type.has_value()) {
            errors.push_back(format_line_error(line.first, "Unknown timing event type: " + tokens[0]));
            continue;
        }

        if (tokens.size() != 3) {
            errors.push_back(format_line_error(line.first, "Timing entry must have exactly 3 fields"));
            continue;
        }

        const std::optional<int> tick = parse_int(tokens[1]);
        if (!tick.has_value()) {
            errors.push_back(format_line_error(line.first, "Timing tick must be an integer"));
            continue;
        }

        timing_event event;
        event.type = *type;
        event.tick = *tick;

        if (*type == timing_event_type::bpm) {
            const std::optional<float> bpm = parse_float(tokens[2]);
            if (!bpm.has_value()) {
                errors.push_back(format_line_error(line.first, "BPM value must be a number"));
                continue;
            }

            event.bpm = *bpm;
        } else {
            const size_t slash_pos = tokens[2].find('/');
            if (slash_pos == std::string::npos) {
                errors.push_back(format_line_error(line.first, "Meter must use numerator/denominator format"));
                continue;
            }

            const std::optional<int> numerator = parse_int(trim(tokens[2].substr(0, slash_pos)));
            const std::optional<int> denominator = parse_int(trim(tokens[2].substr(slash_pos + 1)));
            if (!numerator.has_value() || !denominator.has_value()) {
                errors.push_back(format_line_error(line.first, "Meter values must be integers"));
                continue;
            }

            event.numerator = *numerator;
            event.denominator = *denominator;
        }

        events.push_back(event);
    }

    return events;
}

std::vector<note_data> chart_parser::parse_notes(const std::vector<numbered_line>& lines, std::vector<std::string>& errors) {
    std::vector<note_data> notes;

    for (const numbered_line& line : lines) {
        const std::vector<std::string> tokens = split_csv_line(line.second);
        if (tokens.empty()) {
            continue;
        }

        const std::optional<note_type> type = parse_note_type(tokens[0]);
        if (!type.has_value()) {
            errors.push_back(format_line_error(line.first, "Unknown note type: " + tokens[0]));
            continue;
        }

        const size_t expected_fields = *type == note_type::tap ? 3 : 4;
        if (tokens.size() != expected_fields) {
            errors.push_back(format_line_error(line.first, "Note entry has an unexpected number of fields"));
            continue;
        }

        const std::optional<int> tick = parse_int(tokens[1]);
        const std::optional<int> lane = parse_int(tokens[2]);
        if (!tick.has_value() || !lane.has_value()) {
            errors.push_back(format_line_error(line.first, "Note tick and lane must be integers"));
            continue;
        }

        note_data note;
        note.type = *type;
        note.tick = *tick;
        note.lane = *lane;
        note.end_tick = note.tick;

        if (*type == note_type::hold) {
            const std::optional<int> end_tick = parse_int(tokens[3]);
            if (!end_tick.has_value()) {
                errors.push_back(format_line_error(line.first, "Hold endTick must be an integer"));
                continue;
            }
            note.end_tick = *end_tick;
        }

        notes.push_back(note);
    }

    return notes;
}

std::vector<std::string> chart_parser::validate(const chart_data& data) {
    std::vector<std::string> errors;

    if (data.meta.key_count != 4 && data.meta.key_count != 6) {
        errors.push_back("Metadata keyCount must be 4 or 6");
    }

    if (data.meta.level < 0) {
        errors.push_back("Metadata level must be zero or greater");
    }

    if (data.meta.format_version <= 0) {
        errors.push_back("Metadata formatVersion must be greater than zero");
    }

    if (data.meta.resolution <= 0) {
        errors.push_back("Metadata resolution must be greater than zero");
    }

    for (const timing_event& event : data.timing_events) {
        if (event.tick < 0) {
            errors.push_back("Timing event tick must be zero or greater");
        }

        if (event.type == timing_event_type::bpm && event.bpm <= 0.0f) {
            errors.push_back("BPM event must have bpm greater than zero");
        }

        if (event.type == timing_event_type::meter && (event.numerator <= 0 || event.denominator <= 0)) {
            errors.push_back("Meter event must have positive numerator and denominator");
        }
    }

    struct note_interval {
        int start_tick = 0;
        int end_tick_exclusive = 0;
    };

    std::vector<std::vector<note_interval>> lane_intervals(static_cast<size_t>(std::max(data.meta.key_count, 0)));

    for (const note_data& note : data.notes) {
        if (note.tick < 0) {
            errors.push_back("Note tick must be zero or greater");
            continue;
        }

        if (note.lane < 0 || note.lane >= data.meta.key_count) {
            errors.push_back("Note lane out of range for keyCount");
            continue;
        }

        int end_tick_exclusive = note.tick + 1;
        if (note.type == note_type::hold) {
            if (note.end_tick <= note.tick) {
                errors.push_back("Hold note endTick must be greater than tick");
                continue;
            }
            end_tick_exclusive = note.end_tick;
        }

        lane_intervals[static_cast<size_t>(note.lane)].push_back({note.tick, end_tick_exclusive});
    }

    for (std::vector<note_interval>& intervals : lane_intervals) {
        std::sort(intervals.begin(), intervals.end(), [](const note_interval& left, const note_interval& right) {
            if (left.start_tick != right.start_tick) {
                return left.start_tick < right.start_tick;
            }
            return left.end_tick_exclusive < right.end_tick_exclusive;
        });

        for (size_t index = 1; index < intervals.size(); ++index) {
            if (intervals[index].start_tick < intervals[index - 1].end_tick_exclusive) {
                errors.push_back("Detected overlapping notes on the same lane");
                break;
            }
        }
    }

    return errors;
}
