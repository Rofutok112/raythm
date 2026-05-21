#include "song_writer.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include "path_utils.h"

namespace {

std::string escape_json_string(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    return result;
}

std::string format_float(float value) {
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return stream.str();
}

void write_string_array(std::ofstream& out,
                        const char* key,
                        const std::vector<std::string>& values,
                        bool trailing_comma) {
    out << "  \"" << key << "\": [";
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            out << ", ";
        }
        out << "\"" << escape_json_string(values[index]) << "\"";
    }
    out << "]";
    if (trailing_comma) {
        out << ",";
    }
    out << "\n";
}

void write_timing_events(std::ofstream& out,
                         const std::vector<timing_event>& events,
                         bool trailing_comma) {
    out << "  \"timingEvents\": [\n";
    for (size_t index = 0; index < events.size(); ++index) {
        const timing_event& event = events[index];
        out << "    {\"type\": \"";
        if (event.type == timing_event_type::bpm) {
            out << "bpm\", \"tick\": " << event.tick << ", \"bpm\": " << format_float(event.bpm);
        } else {
            out << "meter\", \"tick\": " << event.tick << ", \"numerator\": " << event.numerator
                << ", \"denominator\": " << event.denominator;
        }
        out << "}";
        if (index + 1 < events.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]";
    if (trailing_comma) {
        out << ",";
    }
    out << "\n";
}

}

bool song_writer::write_song_json(const song_meta& meta, const std::string& directory) {
    if (meta.song_id.empty()) {
        return false;
    }

    std::filesystem::create_directories(path_utils::from_utf8(directory));

    const std::filesystem::path json_path = path_utils::from_utf8(directory) / "song.json";
    std::ofstream out(json_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"songId\": \"" << escape_json_string(meta.song_id) << "\",\n";
    out << "  \"title\": \"" << escape_json_string(meta.title) << "\",\n";
    out << "  \"artist\": \"" << escape_json_string(meta.artist) << "\",\n";
    std::vector<std::string> genres = meta.genres;
    if (genres.empty() && !meta.genre.empty()) {
        genres.push_back(meta.genre);
    }
    if (!genres.empty()) {
        write_string_array(out, "genres", genres, true);
    }
    if (!meta.keywords.empty()) {
        write_string_array(out, "keywords", meta.keywords, true);
    }
    out << "  \"baseBpm\": " << format_float(meta.base_bpm) << ",\n";
    if (meta.has_offset) {
        out << "  \"offset\": " << meta.offset << ",\n";
    }
    if (!meta.timing_events.empty()) {
        write_timing_events(out, meta.timing_events, true);
    }
    if (meta.duration_seconds > 0.0f) {
        out << "  \"durationSec\": " << format_float(meta.duration_seconds) << ",\n";
    }
    if (meta.chart_count > 0) {
        out << "  \"chartCount\": " << meta.chart_count << ",\n";
    }
    out << "  \"audioFile\": \"" << escape_json_string(meta.audio_file) << "\",\n";
    out << "  \"jacketFile\": \"" << escape_json_string(meta.jacket_file) << "\",\n";
    out << "  \"previewStartMs\": " << meta.preview_start_ms << ",\n";
    out << "  \"songVersion\": " << meta.song_version;

    out << "\n}\n";

    return out.good();
}
