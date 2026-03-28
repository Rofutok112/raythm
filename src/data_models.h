#pragma once

#include <array>
#include <string>

struct song_meta {
    std::string song_id;
    std::string title;
    std::string artist;
    float base_bpm = 0.0f;
    std::string audio_file;
    std::string jacket_file;
    int preview_start_ms = 0;
    int song_version = 0;
};

struct chart_meta {
    std::string chart_id;
    int key_count = 0;
    std::string difficulty;
    int level = 0;
    std::string chart_author;
    int format_version = 0;
    int resolution = 0;
};

enum class timing_event_type {
    bpm,
    meter
};

struct timing_event {
    timing_event_type type = timing_event_type::bpm;
    int tick = 0;
    float bpm = 0.0f;
    int numerator = 4;
    int denominator = 4;
};

enum class note_type {
    tap,
    hold
};

struct note_data {
    note_type type = note_type::tap;
    int tick = 0;
    int lane = 0;
    int end_tick = 0;
};

enum class judge_result {
    perfect,
    great,
    good,
    bad,
    miss
};

enum class rank {
    ss,
    s,
    a,
    b,
    c,
    f
};

struct result_data {
    int score = 0;
    float achievement = 0.0f;
    std::array<int, 5> judge_counts = {};
    int max_combo = 0;
    float avg_offset = 0.0f;
    int fast_count = 0;
    int slow_count = 0;
    rank rank = rank::f;
    bool is_full_combo = false;
    bool is_all_perfect = false;
};
