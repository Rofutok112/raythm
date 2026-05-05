#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "chart_parser.h"

namespace {
std::string chart_path(const std::string& file_name) {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    return (repo_root / "assets" / "charts" / file_name).string();
}

bool expect_success(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Skipping missing fixture " << path << '\n';
        return true;
    }

    const chart_parse_result result = chart_parser::parse(path);
    if (!result.success || !result.data.has_value()) {
        std::cerr << "Expected success for " << path << '\n';
        for (const std::string& error : result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return false;
    }

    return true;
}

bool expect_failure(const std::string& path, const std::string& expected_fragment) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Skipping missing fixture " << path << '\n';
        return true;
    }

    const chart_parse_result result = chart_parser::parse(path);
    if (result.success) {
        std::cerr << "Expected failure for " << path << '\n';
        return false;
    }

    for (const std::string& error : result.errors) {
        if (error.find(expected_fragment) != std::string::npos) {
            return true;
        }
    }

    std::cerr << "Expected error containing '" << expected_fragment << "' for " << path << '\n';
    for (const std::string& error : result.errors) {
        std::cerr << "  " << error << '\n';
    }
    return false;
}

bool expect_missing_chart_id_failure() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_external_id.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "songId=parser-song\n"
           << "keyCount=4\n"
           << "difficulty=Fallback\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=1\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,0\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (result.success) {
        std::cerr << "Expected chart without chartId to fail\n";
        return false;
    }
    for (const std::string& error : result.errors) {
        if (error.find("Missing required metadata field: chartId") != std::string::npos) {
            return true;
        }
    }
    std::cerr << "Expected missing chartId error\n";
    return false;
}

bool expect_chart_without_song_id_success() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_no_song_id.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-no-song-id\n"
           << "keyCount=4\n"
           << "difficulty=No Song ID\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=1\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,0\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (!result.success || !result.data.has_value()) {
        std::cerr << "Expected chart without songId to parse\n";
        for (const std::string& error : result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return false;
    }
    return result.data->meta.song_id.empty();
}

bool expect_legacy_level_success() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_legacy_level.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-legacy-level\n"
           << "keyCount=4\n"
           << "difficulty=Legacy Level\n"
           << "level=12.5\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=1\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,0\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (!result.success || !result.data.has_value()) {
        std::cerr << "Expected chart with legacy level metadata to parse\n";
        for (const std::string& error : result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return false;
    }
    return result.data->meta.level == 0.0f;
}
}

int main() {
    bool ok = true;

    ok = expect_success(chart_path("parser_valid.rchart")) && ok;
    ok = expect_failure(chart_path("parser_invalid_overlap.rchart"), "overlapping notes") && ok;
    ok = expect_failure(chart_path("parser_invalid_metadata.rchart"), "keyCount must be 4 or 6") && ok;
    ok = expect_missing_chart_id_failure() && ok;
    ok = expect_chart_without_song_id_success() && ok;
    ok = expect_legacy_level_success() && ok;

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "chart_parser smoke test passed\n";
    return EXIT_SUCCESS;
}
