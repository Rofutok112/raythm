#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>

#include "chart_parser.h"
#include "chart_fingerprint.h"
#include "chart_serializer.h"
#include "updater/update_verify.h"

namespace {
bool almost_equal(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

bool equal_chart_meta(const chart_meta& left, const chart_meta& right) {
    return left.chart_id == right.chart_id &&
           left.key_count == right.key_count &&
           left.difficulty == right.difficulty &&
           left.chart_author == right.chart_author &&
           left.format_version == right.format_version &&
           left.resolution == right.resolution &&
           left.offset == right.offset;
}

bool equal_timing_event(const timing_event& left, const timing_event& right) {
    return left.type == right.type &&
           left.tick == right.tick &&
           almost_equal(left.bpm, right.bpm) &&
           left.numerator == right.numerator &&
           left.denominator == right.denominator;
}

bool equal_note(const note_data& left, const note_data& right) {
    return left.type == right.type &&
           left.tick == right.tick &&
           left.lane == right.lane &&
           left.end_tick == right.end_tick;
}

bool equal_chart_data(const chart_data& left, const chart_data& right) {
    if (!equal_chart_meta(left.meta, right.meta) ||
        left.timing_events.size() != right.timing_events.size() ||
        left.notes.size() != right.notes.size()) {
        return false;
    }

    for (size_t i = 0; i < left.timing_events.size(); ++i) {
        if (!equal_timing_event(left.timing_events[i], right.timing_events[i])) {
            return false;
        }
    }

    for (size_t i = 0; i < left.notes.size(); ++i) {
        if (!equal_note(left.notes[i], right.notes[i])) {
            return false;
        }
    }

    return true;
}

chart_data normalized_chart(chart_data data) {
    std::stable_sort(data.timing_events.begin(), data.timing_events.end(), [](const timing_event& left, const timing_event& right) {
        return left.tick < right.tick;
    });

    std::sort(data.notes.begin(), data.notes.end(), [](const note_data& left, const note_data& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        return left.lane < right.lane;
    });

    return data;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool expect_contains_in_order(const std::string& content, const std::string& first, const std::string& second) {
    const size_t first_pos = content.find(first);
    const size_t second_pos = content.find(second);
    return first_pos != std::string::npos && second_pos != std::string::npos && first_pos < second_pos;
}
}

int main() {
    const std::filesystem::path output_path =
        std::filesystem::temp_directory_path() / "raythm_chart_serializer_smoke.rchart";

    chart_data source;
    source.meta.chart_id = output_path.stem().string();
    source.meta.key_count = 4;
    source.meta.difficulty = "Hyper";
    source.meta.level = 9;
    source.meta.chart_author = "Codex";
    source.meta.format_version = 1;
    source.meta.resolution = 480;
    source.meta.offset = -35;

    source.timing_events = {
        {.type = timing_event_type::bpm, .tick = 960, .bpm = 180.5f, .numerator = 4, .denominator = 4},
        {.type = timing_event_type::meter, .tick = 0, .bpm = 0.0f, .numerator = 4, .denominator = 4},
        {.type = timing_event_type::bpm, .tick = 0, .bpm = 150.0f, .numerator = 4, .denominator = 4},
    };

    source.notes = {
        {.type = note_type::tap, .tick = 960, .lane = 3, .end_tick = 960},
        {.type = note_type::hold, .tick = 480, .lane = 2, .end_tick = 840},
        {.type = note_type::tap, .tick = 480, .lane = 0, .end_tick = 480},
        {.type = note_type::tap, .tick = 0, .lane = 1, .end_tick = 0},
    };

    const bool serialized = chart_serializer::serialize(source, output_path.string());
    if (!serialized) {
        std::cerr << "Failed to serialize chart to " << output_path << '\n';
        return EXIT_FAILURE;
    }

    const std::string content = read_text_file(output_path);
    bool ok = true;

    ok = content.find("offset=-35") != std::string::npos && ok;
    ok = content.find("chartId=") == std::string::npos && ok;
    ok = content.find("songId=") == std::string::npos && ok;
    ok = content.find("level=") == std::string::npos && ok;
    ok = expect_contains_in_order(content, "meter,0,4/4", "bpm,960,180.5") && ok;
    ok = expect_contains_in_order(content, "tap,480,0", "hold,480,2,840") && ok;

    const std::string content_with_ids =
        "[Metadata]\nchartId=online-chart\nsongId=online-song\n" +
        content.substr(content.find("keyCount="));
    const std::string content_with_other_ids =
        "[Metadata]\nchartId=other-chart\nsongId=other-song\n" +
        content.substr(content.find("keyCount="));
    const std::string fingerprint_with_ids = chart_fingerprint::build(content_with_ids);
    const std::string fingerprint_with_other_ids = chart_fingerprint::build(content_with_other_ids);
    ok = updater::compute_sha256_hex(std::string_view(fingerprint_with_ids)) ==
         updater::compute_sha256_hex(std::string_view(fingerprint_with_other_ids)) && ok;

    const chart_parse_result reparsed = chart_parser::parse(output_path.string());
    if (!reparsed.success || !reparsed.data.has_value()) {
        std::cerr << "Failed to parse serialized chart\n";
        for (const std::string& error : reparsed.errors) {
            std::cerr << "  " << error << '\n';
        }
        std::remove(output_path.string().c_str());
        return EXIT_FAILURE;
    }

    chart_data expected = normalized_chart(source);
    if (!equal_chart_data(expected, *reparsed.data)) {
        std::cerr << "Round-trip chart data mismatch\n";
        ok = false;
    }

    std::remove(output_path.string().c_str());

    if (!ok) {
        std::cerr << "chart_serializer smoke test failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "chart_serializer smoke test passed\n";
    return EXIT_SUCCESS;
}
