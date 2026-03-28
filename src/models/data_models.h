#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

// 曲一覧で扱う楽曲メタデータ。
struct song_meta {
    std::string song_id;
    std::string title;
    std::string artist;
    float base_bpm = 0.0f;
    std::string audio_file;
    std::string jacket_file;
    float preview_start_seconds = 0.0f;
    int preview_start_ms = 0;
    int song_version = 0;
};

// 曲一覧で扱う読み込み済み楽曲データ。
struct song_data {
    song_meta meta;
    std::vector<std::string> chart_paths;
    std::string directory;
};

// 楽曲ローダーの結果。
struct song_load_result {
    std::vector<song_data> songs;
    std::vector<std::string> errors;
};

// 譜面ごとの差分情報を表すメタデータ。
struct chart_meta {
    std::string chart_id;
    int key_count = 0;
    std::string difficulty;
    int level = 0;
    std::string chart_author;
    int format_version = 0;
    int resolution = 0;
};

// タイミングイベントの種類。
enum class timing_event_type {
    bpm,
    meter
};

// BPM や拍子変更を表す譜面イベント。
struct timing_event {
    timing_event_type type = timing_event_type::bpm;
    int tick = 0;
    float bpm = 0.0f;
    int numerator = 4;
    int denominator = 4;
};

// ノート入力の種類。
enum class note_type {
    tap,
    hold
};

// 1 ノート分の譜面データ。
struct note_data {
    note_type type = note_type::tap;
    int tick = 0;
    int lane = 0;
    int end_tick = 0;
};

// 1 譜面分のパース済みデータ。
struct chart_data {
    chart_meta meta;
    std::vector<timing_event> timing_events;
    std::vector<note_data> notes;
};

// 譜面パーサーの結果。
struct chart_parse_result {
    bool success = false;
    std::optional<chart_data> data;
    std::vector<std::string> errors;
};

// 判定処理の結果種別。
enum class judge_result {
    perfect,
    great,
    good,
    bad,
    miss
};

// 判定処理用に保持する各ノートの状態。
struct note_state {
    note_data note_ref;
    double target_ms = 0.0;
    double end_target_ms = 0.0;
    bool judged = false;
    judge_result result = judge_result::miss;
    bool holding = false;
};

// 1 件の判定イベント。
struct judge_event {
    judge_result result = judge_result::miss;
    double offset_ms = 0.0;
    int lane = 0;
};

// 達成率に応じたランク種別。
enum class rank {
    ss,
    s,
    a,
    b,
    c,
    f
};

// リザルト画面で表示する集計結果。
struct result_data {
    int score = 0;
    float achievement = 0.0f;
    std::array<int, 5> judge_counts = {};
    int max_combo = 0;
    float avg_offset = 0.0f;
    int fast_count = 0;
    int slow_count = 0;
    rank clear_rank = rank::f;
    bool failed = false;
    bool is_full_combo = false;
    bool is_all_perfect = false;
};
