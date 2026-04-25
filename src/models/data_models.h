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
    std::string audio_url;
    std::string jacket_url;
    float duration_seconds = 0.0f;
    float preview_start_seconds = 0.0f;
    int preview_start_ms = 0;
    int song_version = 0;
    std::string sns_youtube;
    std::string sns_niconico;
    std::string sns_x;
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
    std::string song_id;
    int key_count = 0;
    std::string difficulty;
    float level = 0.0f;
    std::string chart_author;
    int format_version = 0;
    int resolution = 0;
    int offset = 0;
    bool is_public = false;
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

// 入力イベントの種別。
enum class input_event_type {
    press,
    release
};

// 1 件の入力イベント。
struct input_event {
    input_event_type type = input_event_type::press;
    int lane = 0;
    double timestamp_ms = 0.0;
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

enum class note_progress_state {
    pending,
    holding,
    completed
};

// 判定処理用に保持する各ノートの状態。
struct note_state {
    note_data note_ref;
    double target_ms = 0.0;
    double end_target_ms = 0.0;
    int head_event_index = -1;
    int tail_event_index = -1;
    note_progress_state progress = note_progress_state::pending;
    judge_result result = judge_result::miss;

    [[nodiscard]] bool is_judged() const {
        return progress != note_progress_state::pending;
    }

    [[nodiscard]] bool is_completed() const {
        return progress == note_progress_state::completed;
    }

    [[nodiscard]] bool is_holding() const {
        return progress == note_progress_state::holding;
    }
};

// 1 件の判定イベント。
struct judge_event {
    judge_result result = judge_result::miss;
    double offset_ms = 0.0;
    int lane = 0;
    bool play_hitsound = true;
    bool apply_gameplay_effects = true;
    bool show_feedback = true;
    int event_index = -1;
};

// 達成率に応じたランク種別。
enum class rank {
    ss,
    s,
    aa,
    a,
    b,
    c,
    f
};

// accuracy と full combo 情報からランクを算出する。
inline rank compute_rank(float accuracy, bool is_full_combo) {
    if (is_full_combo && accuracy >= 100.0f) return rank::ss;
    if (is_full_combo && accuracy >= 95.0f) return rank::s;
    if (accuracy >= 95.0f) return rank::aa;
    if (accuracy >= 90.0f) return rank::a;
    if (accuracy >= 80.0f) return rank::b;
    if (accuracy >= 70.0f) return rank::c;
    return rank::f;
}

struct note_result_entry {
    int event_index = -1;
    judge_result result = judge_result::miss;
    double offset_ms = 0.0;
};

// リザルト画面で表示する集計結果。
struct result_data {
    int score = 0;
    float accuracy = 0.0f;
    std::array<int, 5> judge_counts = {};
    int max_combo = 0;
    float avg_offset = 0.0f;
    int fast_count = 0;
    int slow_count = 0;
    rank clear_rank = rank::f;
    bool failed = false;
    bool is_full_combo = false;
    bool is_all_perfect = false;
    std::string scoring_ruleset_version;
    std::string scoring_accepted_input = "note_results_v1";
    std::vector<note_result_entry> note_results;
};
