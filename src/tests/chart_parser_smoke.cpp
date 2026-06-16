#include <algorithm>
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

bool expect_server_managed_metadata_success() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_server_managed_metadata.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-server-managed-metadata\n"
           << "keyCount=4\n"
           << "difficulty=Server Managed\n"
           << "level=12.5\n"
           << "calculatedLevel=12.5\n"
           << "isPublic=true\n"
           << "contentSource=official\n"
           << "metadataSchemaVersion=2\n"
           << "clientChartId=local-chart\n"
           << "clientSongId=local-song\n"
           << "songId=legacy-song\n"
           << "difficultyRulesetId=raythm-local\n"
           << "difficultyRulesetVersion=1\n"
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
        std::cerr << "Expected chart with server-managed metadata to parse\n";
        for (const std::string& error : result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return false;
    }
    return result.data->meta.level == 0.0f && result.data->meta.song_id.empty();
}

bool expect_wide_note_success() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_wide_note.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-wide-note\n"
           << "keyCount=4\n"
           << "difficulty=Wide\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=2\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,1,width=2\n"
           << "release,480,0,width=3,ray\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (!result.success || !result.data.has_value() || result.data->notes.size() != 2) {
        std::cerr << "Expected wide notes to parse\n";
        return false;
    }
    return result.data->notes[0].lane_width == 2 &&
           result.data->notes[1].lane_width == 3 &&
           result.data->notes[1].is_ray;
}

bool expect_wide_note_bounds_failure() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_wide_note_bounds.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-wide-note-bounds\n"
           << "keyCount=4\n"
           << "difficulty=Wide\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=2\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,3,width=2\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (result.success) {
        std::cerr << "Expected out-of-bounds wide note to fail\n";
        return false;
    }
    return std::any_of(result.errors.begin(), result.errors.end(), [](const std::string& error) {
        return error.find("width extends beyond keyCount") != std::string::npos;
    });
}

bool expect_decorative_hold_success() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_decorative_hold.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-decorative-hold\n"
           << "keyCount=4\n"
           << "difficulty=Decorative\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=5\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "hold,0,1,960\n"
           << "decorativeHold,0,1,960,width=2\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (!result.success || !result.data.has_value() || result.data->notes.size() != 2) {
        std::cerr << "Expected decorative hold to parse and overlap gameplay notes\n";
        if (!result.success) {
            for (const std::string& error : result.errors) {
                std::cerr << "  " << error << '\n';
            }
        }
        return false;
    }
    const note_data& note = result.data->notes[1];
    return note.type == note_type::decorative_hold &&
           note.tick == 0 &&
           note.end_tick == 960 &&
           note.lane_width == 2;
}

bool expect_scroll_automation_success() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_parser_scroll_automation.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=parser-scroll-automation\n"
           << "keyCount=4\n"
           << "difficulty=Scroll Automation\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=4\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[ScrollAutomation]\n"
           << "point,0,1.0,hold\n"
           << "point,960,2.0,linear\n"
           << "point,1440,0.5,easeInOut\n\n"
           << "[ScrollAutomationGuides]\n"
           << "guides,0.25,0.75,2.5,100\n\n"
           << "[Notes]\n"
           << "tap,0,0\n";
    output.close();

    const chart_parse_result result = chart_parser::parse(path.string());
    std::filesystem::remove(path);
    if (!result.success || !result.data.has_value() || result.data->scroll_automation.size() != 3) {
        std::cerr << "Expected scroll automation to parse\n";
        return false;
    }
    return result.data->scroll_automation[1].curve_to_next == scroll_automation_curve::linear &&
           result.data->scroll_automation[2].curve_to_next == scroll_automation_curve::ease_in_out &&
           result.data->scroll_guides.values[0] == 0.25f &&
           result.data->scroll_guides.values[1] == 0.75f &&
           result.data->scroll_guides.values[2] == 2.5f &&
           result.data->scroll_guides.values[3] == 100.0f;
}

}

int main() {
    bool ok = true;

    ok = expect_success(chart_path("parser_valid.rchart")) && ok;
    ok = expect_failure(chart_path("parser_invalid_overlap.rchart"), "overlapping notes") && ok;
    ok = expect_failure(chart_path("parser_invalid_metadata.rchart"), "keyCount must be 4 or 6") && ok;
    ok = expect_missing_chart_id_failure() && ok;
    ok = expect_chart_without_song_id_success() && ok;
    ok = expect_server_managed_metadata_success() && ok;
    ok = expect_wide_note_success() && ok;
    ok = expect_wide_note_bounds_failure() && ok;
    ok = expect_decorative_hold_success() && ok;
    ok = expect_scroll_automation_success() && ok;

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "chart_parser smoke test passed\n";
    return EXIT_SUCCESS;
}
