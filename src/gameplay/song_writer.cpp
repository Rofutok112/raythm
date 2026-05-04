#include "song_writer.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

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

}

bool song_writer::write_song_json(const song_meta& meta, const std::string& directory) {
    std::filesystem::create_directories(path_utils::from_utf8(directory));

    const std::filesystem::path json_path = path_utils::from_utf8(directory) / "song.json";
    std::ofstream out(json_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    if (!meta.song_id.empty()) {
        out << "  \"songId\": \"" << escape_json_string(meta.song_id) << "\",\n";
    }
    out << "  \"title\": \"" << escape_json_string(meta.title) << "\",\n";
    out << "  \"artist\": \"" << escape_json_string(meta.artist) << "\",\n";
    out << "  \"baseBpm\": " << format_float(meta.base_bpm) << ",\n";
    out << "  \"audioFile\": \"" << escape_json_string(meta.audio_file) << "\",\n";
    out << "  \"jacketFile\": \"" << escape_json_string(meta.jacket_file) << "\",\n";
    out << "  \"previewStartMs\": " << meta.preview_start_ms << ",\n";
    out << "  \"songVersion\": " << meta.song_version;

    if (!meta.sns_youtube.empty()) {
        out << ",\n  \"snsYoutube\": \"" << escape_json_string(meta.sns_youtube) << "\"";
    }
    if (!meta.sns_niconico.empty()) {
        out << ",\n  \"snsNiconico\": \"" << escape_json_string(meta.sns_niconico) << "\"";
    }
    if (!meta.sns_x.empty()) {
        out << ",\n  \"snsX\": \"" << escape_json_string(meta.sns_x) << "\"";
    }

    out << "\n}\n";

    return out.good();
}
