#include "ranking_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <ctime>
#include <string_view>
#include <vector>

#include "app_paths.h"
#include "chart_fingerprint.h"
#include "network/auth_client.h"
#include "network/ranking_client.h"
#include "network/server_environment.h"
#include "path_utils.h"
#include "scoring_ruleset_runtime.h"
#include "song_fingerprint.h"
#include "title/local_content_index.h"
#include "updater/update_verify.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

constexpr char kLocalRankingFileHeader[] = "RAYTHM_LOCAL_RANKING_V6";
constexpr char kAuthoritativeAcceptedInput[] = "noteResultsV1";
constexpr char kLegacyAuthoritativeAcceptedInput[] = "note_results_v1";
constexpr wchar_t kEntropyLabel[] = L"raythm-local-ranking";

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string normalize_accepted_input(std::string_view value) {
    const std::string normalized = trim(value);
    return normalized == kLegacyAuthoritativeAcceptedInput
        ? std::string(kAuthoritativeAcceptedInput)
        : normalized;
}

std::string current_timestamp_utc() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t raw = clock::to_time_t(now);
    std::tm utc_tm{};
#ifdef _WIN32
    gmtime_s(&utc_tm, &raw);
#else
    utc_tm = *std::gmtime(&raw);
#endif
    std::ostringstream out;
    out << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string sanitize_player_display_name(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\t':
            case '\r':
            case '\n':
                sanitized += ' ';
                break;
            default:
                sanitized += ch;
                break;
        }
    }
    const std::string trimmed = trim(sanitized);
    return trimmed.empty() ? "Guest" : trimmed;
}

std::string current_local_player_display_name() {
    const auth::session_summary summary = auth::load_session_summary();
    if (summary.logged_in && !summary.display_name.empty()) {
        return sanitize_player_display_name(summary.display_name);
    }
    return "Guest";
}

std::string local_rank_label(rank clear_rank) {
    switch (clear_rank) {
        case rank::ss: return "ss";
        case rank::s: return "s";
        case rank::aa: return "aa";
        case rank::a: return "a";
        case rank::b: return "b";
        case rank::c: return "c";
        case rank::f: return "f";
    }

    return "f";
}

rank parse_rank_label_local(std::string_view value) {
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized == "ss") return rank::ss;
    if (normalized == "s") return rank::s;
    if (normalized == "aa") return rank::aa;
    if (normalized == "a") return rank::a;
    if (normalized == "b") return rank::b;
    if (normalized == "c") return rank::c;
    return rank::f;
}

std::string judge_result_label(judge_result result) {
    switch (result) {
        case judge_result::perfect: return "perfect";
        case judge_result::great: return "great";
        case judge_result::good: return "good";
        case judge_result::bad: return "bad";
        case judge_result::miss: return "miss";
    }

    return "miss";
}

std::optional<judge_result> parse_judge_result_label(std::string_view value) {
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized == "perfect") return judge_result::perfect;
    if (normalized == "great") return judge_result::great;
    if (normalized == "good") return judge_result::good;
    if (normalized == "bad") return judge_result::bad;
    if (normalized == "miss") return judge_result::miss;
    return std::nullopt;
}

std::string serialize_note_results(const std::vector<note_result_entry>& note_results) {
    std::ostringstream out;
    for (size_t i = 0; i < note_results.size(); ++i) {
        const note_result_entry& entry = note_results[i];
        if (i > 0) {
            out << ';';
        }
        out << entry.event_index << ','
            << judge_result_label(entry.result) << ','
            << std::fixed << std::setprecision(3) << entry.offset_ms;
    }
    return out.str();
}

std::optional<std::vector<note_result_entry>> parse_note_results(std::string_view value) {
    std::vector<note_result_entry> note_results;
    std::istringstream row{std::string(value)};
    std::string token;
    while (std::getline(row, token, ';')) {
        if (token.empty()) {
            continue;
        }

        std::istringstream event_row(token);
        std::string event_index_token;
        std::string result_token;
        std::string offset_token;
        if (!std::getline(event_row, event_index_token, ',') ||
            !std::getline(event_row, result_token, ',') ||
            !std::getline(event_row, offset_token)) {
            return std::nullopt;
        }

        const std::optional<judge_result> parsed_result = parse_judge_result_label(result_token);
        if (!parsed_result.has_value()) {
            return std::nullopt;
        }

        try {
            note_results.push_back(note_result_entry{
                .event_index = std::stoi(trim(event_index_token)),
                .result = *parsed_result,
                .offset_ms = std::stod(trim(offset_token)),
            });
        } catch (...) {
            return std::nullopt;
        }
    }

    return note_results;
}

struct stored_local_record {
    std::string recorded_at;
    std::string player_display_name;
    std::string scoring_ruleset_version;
    std::string scoring_accepted_input = std::string(kAuthoritativeAcceptedInput);
    std::vector<note_result_entry> note_results;
    std::optional<ranking_service::entry> legacy_entry;
};

ranking_service::entry resolve_record_entry(const stored_local_record& record,
                                           const scoring_ruleset_runtime::ruleset& ruleset) {
    if (record.legacy_entry.has_value()) {
        ranking_service::entry entry = *record.legacy_entry;
        entry.recorded_at = record.recorded_at;
        entry.player_display_name = sanitize_player_display_name(record.player_display_name);
        entry.verified = false;
        return entry;
    }

    const scoring_ruleset_runtime::computed_result computed =
        scoring_ruleset_runtime::compute_result_for(ruleset, record.note_results);
    return ranking_service::entry{
        .placement = 0,
        .player_display_name = sanitize_player_display_name(record.player_display_name),
        .accuracy = computed.accuracy,
        .is_full_combo = computed.is_full_combo,
        .max_combo = computed.max_combo,
        .score = computed.score,
        .recorded_at = record.recorded_at,
        .verified = false,
        .resolved_clear_rank = computed.clear_rank,
    };
}

std::string serialize_records(const std::vector<stored_local_record>& records) {
    std::ostringstream out;
    out << kLocalRankingFileHeader << '\n';
    for (const stored_local_record& record : records) {
        if (record.legacy_entry.has_value()) {
            const ranking_service::entry& entry = *record.legacy_entry;
            out << "legacy" << '\t'
                << record.recorded_at << '\t'
                << sanitize_player_display_name(record.player_display_name) << '\t'
                << std::fixed << std::setprecision(4) << entry.accuracy << '\t'
                << (entry.is_full_combo ? 1 : 0) << '\t'
                << entry.max_combo << '\t'
                << entry.score << '\t'
                << local_rank_label(entry.resolved_clear_rank) << '\n';
            continue;
        }

        out << "record" << '\t'
            << record.recorded_at << '\t'
            << sanitize_player_display_name(record.player_display_name) << '\t'
            << record.scoring_ruleset_version << '\t'
            << record.scoring_accepted_input << '\t'
            << serialize_note_results(record.note_results) << '\n';
    }
    return out.str();
}

std::vector<stored_local_record> parse_records(const std::string& content) {
    std::vector<stored_local_record> records;
    std::istringstream input(content);
    std::string line;
    if (!std::getline(input, line)) {
        return records;
    }
    const std::string header = trim(line);
    if (header != kLocalRankingFileHeader) {
        return records;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream row(line);

        try {
            stored_local_record record;
            std::string kind_token, recorded_at_token, player_token;
            if (!std::getline(row, kind_token, '\t') ||
                !std::getline(row, recorded_at_token, '\t') ||
                !std::getline(row, player_token, '\t')) {
                continue;
            }

            record.recorded_at = trim(recorded_at_token);
            record.player_display_name = sanitize_player_display_name(player_token);
            const std::string kind = trim(kind_token);

            if (kind == "record") {
                std::string ruleset_version_token, accepted_input_token;
                std::string note_results_token;
                if (!std::getline(row, ruleset_version_token, '\t') ||
                    !std::getline(row, accepted_input_token, '\t') ||
                    !std::getline(row, note_results_token)) {
                    continue;
                }
                record.scoring_ruleset_version = trim(ruleset_version_token);
                record.scoring_accepted_input = normalize_accepted_input(accepted_input_token);
                const std::optional<std::vector<note_result_entry>> note_results =
                    parse_note_results(note_results_token);
                if (!note_results.has_value()) {
                    continue;
                }
                record.note_results = *note_results;
            } else if (kind == "legacy") {
                std::string accuracy_token, fc_token, combo_token, score_token, rank_token;
                if (!std::getline(row, accuracy_token, '\t') ||
                    !std::getline(row, fc_token, '\t') ||
                    !std::getline(row, combo_token, '\t') ||
                    !std::getline(row, score_token, '\t') ||
                    !std::getline(row, rank_token)) {
                    continue;
                }

                ranking_service::entry entry;
                entry.accuracy = std::clamp(std::stof(trim(accuracy_token)), 0.0f, 100.0f);
                entry.is_full_combo = trim(fc_token) == "1";
                entry.max_combo = std::clamp(std::stoi(trim(combo_token)), 0, 999999);
                entry.score = std::clamp(std::stoi(trim(score_token)), 0, 1000000);
                entry.recorded_at = record.recorded_at;
                entry.player_display_name = record.player_display_name;
                entry.verified = false;
                entry.resolved_clear_rank = parse_rank_label_local(rank_token);
                record.legacy_entry = std::move(entry);
            } else {
                continue;
            }

            records.push_back(std::move(record));
        } catch (...) {
            continue;
        }
    }

    return records;
}

bool is_authoritative_local_record(const stored_local_record& record) {
    return !record.legacy_entry.has_value() &&
           !record.note_results.empty() &&
           !record.scoring_ruleset_version.empty() &&
           record.scoring_accepted_input == kAuthoritativeAcceptedInput;
}

void retain_authoritative_local_records(std::vector<stored_local_record>& records) {
    records.erase(
        std::remove_if(records.begin(), records.end(), [](const stored_local_record& record) {
            return !is_authoritative_local_record(record);
        }),
        records.end());
}

bool ranking_entry_better(const ranking_service::entry& left, const ranking_service::entry& right) {
    if (left.score != right.score) {
        return left.score > right.score;
    }
    if (left.accuracy != right.accuracy) {
        return left.accuracy > right.accuracy;
    }
    return left.recorded_at < right.recorded_at;
}

bool chart_obviously_eligible_for_online_ranking(const chart_meta& chart) {
    return !chart.chart_id.empty();
}

std::optional<auth::session> load_online_session_for_ruleset() {
    const auth::session_summary summary = auth::load_session_summary();
    if (!summary.logged_in) {
        return std::nullopt;
    }

    return auth::load_saved_session();
}

std::string display_ruleset_server_url() {
    const auth::session_summary summary = auth::load_session_summary();
    if (!summary.server_url.empty()) {
        return auth::normalize_server_url(summary.server_url);
    }

    return server_environment::active_server_url();
}

struct cached_scoring_ruleset_state {
    std::mutex mutex;
    std::string server_url;
    std::optional<ranking_client::scoring_ruleset> ruleset;
};

cached_scoring_ruleset_state& scoring_ruleset_cache() {
    static cached_scoring_ruleset_state cache;
    return cache;
}

std::string rank_to_label(rank clear_rank) {
    switch (clear_rank) {
        case rank::ss: return "ss";
        case rank::s: return "s";
        case rank::aa: return "aa";
        case rank::a: return "a";
        case rank::b: return "b";
        case rank::c: return "c";
        case rank::f: return "f";
    }

    return "f";
}

rank parse_rank_label(std::string_view value) {
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized == "ss") return rank::ss;
    if (normalized == "s") return rank::s;
    if (normalized == "aa") return rank::aa;
    if (normalized == "a") return rank::a;
    if (normalized == "b") return rank::b;
    if (normalized == "c") return rank::c;
    return rank::f;
}

bool save_cached_ruleset_to_disk(const std::string& server_url,
                                 const ranking_client::scoring_ruleset& ruleset) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::scoring_ruleset_cache_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "server_url\t" << server_url << '\n';
    output << "active\t" << (ruleset.active ? 1 : 0) << '\n';
    output << "accepted_input\t" << ruleset.accepted_input << '\n';
    output << "ruleset_version\t" << ruleset.ruleset_version << '\n';
    output << "score_model\t" << ruleset.score_model << '\n';
    output << "max_score\t" << ruleset.max_score << '\n';
    output << "judge_values\t"
           << ruleset.judge_values[0] << ','
           << ruleset.judge_values[1] << ','
           << ruleset.judge_values[2] << ','
           << ruleset.judge_values[3] << ','
           << ruleset.judge_values[4] << '\n';
    for (const auto& threshold : ruleset.rank_thresholds) {
        output << "rank_threshold\t"
               << rank_to_label(threshold.clear_rank) << ','
               << threshold.min_accuracy << ','
               << (threshold.requires_full_combo ? 1 : 0) << '\n';
    }
    return output.good();
}

std::optional<std::pair<std::string, ranking_client::scoring_ruleset>> load_cached_ruleset_from_disk() {
    std::ifstream input(app_paths::scoring_ruleset_cache_path(), std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    ranking_client::scoring_ruleset ruleset = scoring_ruleset_runtime::make_default_ruleset();
    std::string server_url;
    std::string line;
    std::vector<scoring_ruleset_runtime::rank_threshold> thresholds;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, tab));
        const std::string value = line.substr(tab + 1);
        if (key == "server_url") {
            server_url = trim(value);
        } else if (key == "active") {
            ruleset.active = trim(value) == "1";
        } else if (key == "accepted_input") {
            ruleset.accepted_input = normalize_accepted_input(value);
        } else if (key == "ruleset_version") {
            ruleset.ruleset_version = trim(value);
        } else if (key == "score_model") {
            ruleset.score_model = trim(value);
        } else if (key == "max_score") {
            try {
                ruleset.max_score = std::stoi(trim(value));
            } catch (...) {
                return std::nullopt;
            }
        } else if (key == "judge_values") {
            std::array<int, 5> judge_values = {};
            std::istringstream row(value);
            std::string token;
            for (size_t i = 0; i < judge_values.size(); ++i) {
                if (!std::getline(row, token, ',')) {
                    return std::nullopt;
                }
                try {
                    judge_values[i] = std::stoi(trim(token));
                } catch (...) {
                    return std::nullopt;
                }
            }
            ruleset.judge_values = judge_values;
        } else if (key == "rank_threshold") {
            std::istringstream row(value);
            std::string label_token, accuracy_token, fc_token;
            if (!std::getline(row, label_token, ',') ||
                !std::getline(row, accuracy_token, ',') ||
                !std::getline(row, fc_token, ',')) {
                return std::nullopt;
            }
            try {
                thresholds.push_back(scoring_ruleset_runtime::rank_threshold{
                    .clear_rank = parse_rank_label(label_token),
                    .min_accuracy = std::stof(trim(accuracy_token)),
                    .requires_full_combo = trim(fc_token) == "1",
                });
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    if (server_url.empty()) {
        return std::nullopt;
    }
    if (!thresholds.empty()) {
        ruleset.rank_thresholds = std::move(thresholds);
    }
    return std::make_pair(server_url, ruleset);
}

void store_cached_ruleset(const std::string& server_url,
                         const ranking_client::scoring_ruleset& ruleset) {
    auto& cache = scoring_ruleset_cache();
    std::scoped_lock lock(cache.mutex);
    cache.server_url = server_url;
    cache.ruleset = ruleset;
    scoring_ruleset_runtime::apply_server_ruleset(ruleset);
    save_cached_ruleset_to_disk(server_url, ruleset);
}

std::optional<ranking_client::scoring_ruleset> load_cached_ruleset_for_server(const std::string& server_url) {
    auto& cache = scoring_ruleset_cache();
    {
        std::scoped_lock lock(cache.mutex);
        if (cache.server_url == server_url && cache.ruleset.has_value()) {
            return cache.ruleset;
        }
    }

    const auto persisted = load_cached_ruleset_from_disk();
    if (!persisted.has_value() || persisted->first != server_url) {
        return std::nullopt;
    }

    {
        std::scoped_lock lock(cache.mutex);
        cache.server_url = persisted->first;
        cache.ruleset = persisted->second;
    }
    scoring_ruleset_runtime::apply_server_ruleset(persisted->second);
    return persisted->second;
}

std::optional<ranking_client::scoring_ruleset> fetch_and_cache_scoring_ruleset(const std::string& server_url) {
    const ranking_client::scoring_ruleset_operation_result ruleset_request =
        ranking_client::fetch_scoring_ruleset(server_url);
    if (!ruleset_request.success || !ruleset_request.ruleset.has_value()) {
        return std::nullopt;
    }

    store_cached_ruleset(server_url, *ruleset_request.ruleset);
    return ruleset_request.ruleset;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string expected_remote_song_id(const std::string& server_url,
                                    const std::string& local_song_id) {
    const std::optional<local_content_index::online_song_binding> binding =
        local_content_index::find_song_by_local(server_url, local_song_id);
    return binding.has_value() ? binding->remote_song_id : local_song_id;
}

std::string expected_remote_chart_id(const std::string& server_url,
                                     const std::string& local_chart_id) {
    const std::optional<local_content_index::online_chart_binding> binding =
        local_content_index::find_chart_by_local(server_url, local_chart_id);
    return binding.has_value() ? binding->remote_chart_id : local_chart_id;
}

struct local_manifest_hashes {
    std::string song_json_sha256;
    std::string song_json_fingerprint;
    std::string audio_sha256;
    std::string jacket_sha256;
    std::string chart_sha256;
    std::string chart_fingerprint;
};

struct verification_result {
    bool success = false;
    std::string message;
};

verification_result compare_hash(const std::string& label,
                                 const std::string& local_hash,
                                 const std::string& server_hash) {
    if (local_hash.empty() || server_hash.empty()) {
        return {
            .success = false,
            .message = "Online chart verification is missing required hash data.",
        };
    }

    if (lowercase(local_hash) != lowercase(server_hash)) {
        return {
            .success = false,
            .message = "Online chart verification failed for " + label + ".",
        };
    }

    return {
        .success = true,
        .message = {},
    };
}

std::optional<local_manifest_hashes> compute_local_manifest_hashes(const song_data& song,
                                                                   const std::string& chart_path,
                                                                   std::string& error_message) {
    const std::filesystem::path song_dir = path_utils::from_utf8(song.directory);
    const std::filesystem::path song_json_path = song_dir / "song.json";
    const std::filesystem::path audio_path = song_dir / path_utils::from_utf8(song.meta.audio_file);
    const std::filesystem::path jacket_path = song_dir / path_utils::from_utf8(song.meta.jacket_file);
    const std::filesystem::path local_chart_path = path_utils::from_utf8(chart_path);

    const std::optional<std::string> song_json_sha256 = updater::compute_sha256_hex(song_json_path);
    if (!song_json_sha256.has_value()) {
        error_message = "Failed to hash local song.json for online verification.";
        return std::nullopt;
    }
    const std::optional<std::string> song_json_fingerprint_sha256 =
        song_fingerprint::compute_sha256_hex(song_json_path);
    if (!song_json_fingerprint_sha256.has_value()) {
        error_message = "Failed to fingerprint local song.json for online verification.";
        return std::nullopt;
    }

    const std::optional<std::string> audio_sha256 = updater::compute_sha256_hex(audio_path);
    if (!audio_sha256.has_value()) {
        error_message = "Failed to hash local audio for online verification.";
        return std::nullopt;
    }

    const std::optional<std::string> jacket_sha256 = updater::compute_sha256_hex(jacket_path);
    if (!jacket_sha256.has_value()) {
        error_message = "Failed to hash local jacket for online verification.";
        return std::nullopt;
    }

    const std::optional<std::string> chart_sha256 = updater::compute_sha256_hex(local_chart_path);
    if (!chart_sha256.has_value()) {
        error_message = "Failed to hash local chart for online verification.";
        return std::nullopt;
    }

    const std::optional<std::string> chart_fingerprint_sha256 =
        chart_fingerprint::compute_sha256_hex(local_chart_path);
    if (!chart_fingerprint_sha256.has_value()) {
        error_message = "Failed to fingerprint local chart for online verification.";
        return std::nullopt;
    }

    return local_manifest_hashes{
        .song_json_sha256 = *song_json_sha256,
        .song_json_fingerprint = *song_json_fingerprint_sha256,
        .audio_sha256 = *audio_sha256,
        .jacket_sha256 = *jacket_sha256,
        .chart_sha256 = *chart_sha256,
        .chart_fingerprint = *chart_fingerprint_sha256,
    };
}

verification_result verify_chart_manifest(const song_data& song,
                                          const std::string& chart_path,
                                          const chart_meta& chart,
                                          const std::string& server_url) {
    const std::string remote_song_id = expected_remote_song_id(server_url, song.meta.song_id);
    const std::string remote_chart_id = expected_remote_chart_id(server_url, chart.chart_id);
    const ranking_client::manifest_operation_result manifest_result =
        ranking_client::fetch_chart_manifest(server_url, remote_chart_id);
    if (!manifest_result.success || !manifest_result.manifest.has_value()) {
        return {
            .success = false,
            .message = manifest_result.message.empty()
                ? "Failed to fetch online verification manifest."
                : manifest_result.message,
        };
    }

    const ranking_client::chart_manifest& manifest = *manifest_result.manifest;
    if (!manifest.available) {
        return {
            .success = false,
            .message = manifest.message.empty()
                ? "This chart is not eligible for online ranking verification."
                : manifest.message,
        };
    }

    if (manifest.chart_id != remote_chart_id ||
        manifest.song_id != remote_song_id) {
        return {
            .success = false,
            .message = "Online chart verification failed because the manifest IDs do not match local content.",
        };
    }

    std::string hash_error;
    const std::optional<local_manifest_hashes> local_hashes =
        compute_local_manifest_hashes(song, chart_path, hash_error);
    if (!local_hashes.has_value()) {
        return {
            .success = false,
            .message = hash_error,
        };
    }

    for (const verification_result& result : {
             compare_hash("song.json",
                          manifest.song_json_fingerprint.empty()
                              ? local_hashes->song_json_sha256
                              : local_hashes->song_json_fingerprint,
                          manifest.song_json_fingerprint.empty()
                              ? manifest.song_json_sha256
                              : manifest.song_json_fingerprint),
             compare_hash("audio", local_hashes->audio_sha256, manifest.audio_sha256),
             compare_hash("jacket", local_hashes->jacket_sha256, manifest.jacket_sha256),
             compare_hash("chart",
                          manifest.chart_fingerprint.empty()
                              ? local_hashes->chart_sha256
                              : local_hashes->chart_fingerprint,
                          manifest.chart_fingerprint.empty()
                              ? manifest.chart_sha256
                              : manifest.chart_fingerprint),
         }) {
        if (!result.success) {
            return result;
        }
    }

    return {
        .success = true,
        .message = {},
    };
}

#ifdef _WIN32
bool crypt_protect_utf8(const std::string& plaintext, std::vector<std::byte>& ciphertext) {
    DATA_BLOB input_blob{};
    input_blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input_blob.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB entropy_blob{};
    entropy_blob.pbData = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(kEntropyLabel));
    entropy_blob.cbData = static_cast<DWORD>(sizeof(kEntropyLabel));

    DATA_BLOB output_blob{};
    if (!CryptProtectData(&input_blob, L"raythm local ranking", &entropy_blob, nullptr, nullptr, 0, &output_blob)) {
        return false;
    }

    ciphertext.resize(output_blob.cbData);
    std::memcpy(ciphertext.data(), output_blob.pbData, output_blob.cbData);
    LocalFree(output_blob.pbData);
    return true;
}

bool crypt_unprotect_utf8(const std::vector<std::byte>& ciphertext, std::string& plaintext) {
    DATA_BLOB input_blob{};
    input_blob.pbData = reinterpret_cast<BYTE*>(const_cast<std::byte*>(ciphertext.data()));
    input_blob.cbData = static_cast<DWORD>(ciphertext.size());

    DATA_BLOB entropy_blob{};
    entropy_blob.pbData = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(kEntropyLabel));
    entropy_blob.cbData = static_cast<DWORD>(sizeof(kEntropyLabel));

    DATA_BLOB output_blob{};
    if (!CryptUnprotectData(&input_blob, nullptr, &entropy_blob, nullptr, nullptr, 0, &output_blob)) {
        return false;
    }

    plaintext.assign(reinterpret_cast<const char*>(output_blob.pbData), output_blob.cbData);
    LocalFree(output_blob.pbData);
    return true;
}
#endif

std::vector<stored_local_record> load_local_records(const std::string& chart_id) {
    if (chart_id.empty()) {
        return {};
    }

    const std::filesystem::path path = app_paths::local_ranking_path(chart_id);
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        return {};
    }

#ifdef _WIN32
    std::vector<std::byte> encrypted(bytes.size());
    std::memcpy(encrypted.data(), bytes.data(), bytes.size());
    std::string plaintext;
    if (!crypt_unprotect_utf8(encrypted, plaintext)) {
        return {};
    }
    return parse_records(plaintext);
#else
    return parse_records(std::string(bytes.begin(), bytes.end()));
#endif
}

bool save_local_records(const std::string& chart_id, const std::vector<stored_local_record>& records) {
    if (chart_id.empty()) {
        return false;
    }

    const std::string plaintext = serialize_records(records);
    app_paths::ensure_directories();
    const std::filesystem::path path = app_paths::local_ranking_path(chart_id);

#ifdef _WIN32
    std::vector<std::byte> encrypted;
    if (!crypt_protect_utf8(plaintext, encrypted)) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
    return output.good();
#else
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << plaintext;
    return output.good();
#endif
}

}  // namespace

namespace ranking_service {

listing load_chart_ranking(const std::string& chart_id, source ranking_source, int limit) {
    listing result;
    result.ranking_source = ranking_source;

    if (chart_id.empty()) {
        result.available = false;
        result.message = "No chart selected.";
        return result;
    }

    if (ranking_source == source::online) {
        const std::optional<auth::session> stored = auth::load_saved_session();
        if (!stored.has_value()) {
            result.available = false;
            result.message = "Sign in to view online rankings.";
            return result;
        }

        const std::string remote_chart_id = expected_remote_chart_id(stored->server_url, chart_id);
        ranking_client::operation_result online_result =
            ranking_client::fetch_chart_ranking(stored->server_url, stored->access_token, remote_chart_id, limit);

        if (online_result.unauthorized) {
            const auth::operation_result restored = auth::restore_saved_session();
            if (!restored.success || !restored.session_data.has_value()) {
                result.available = false;
                result.message = restored.message.empty()
                    ? "Sign in to view online rankings."
                    : restored.message;
                return result;
            }

            online_result = ranking_client::fetch_chart_ranking(
                restored.session_data->server_url,
                restored.session_data->access_token,
                expected_remote_chart_id(restored.session_data->server_url, chart_id),
                limit);
        }

        if (!online_result.success || !online_result.listing.has_value()) {
            result.available = false;
            result.message = online_result.message.empty()
                ? "Failed to load online rankings."
                : online_result.message;
            return result;
        }

        result.available = online_result.listing->available;
        result.entries = std::move(online_result.listing->entries);
        result.message = online_result.listing->message;
        return result;
    }

    const std::vector<stored_local_record> records = load_local_records(chart_id);
    const scoring_ruleset_runtime::ruleset ruleset = scoring_ruleset_runtime::current_ruleset();
    result.entries.reserve(records.size());
    for (const stored_local_record& record : records) {
        if (!is_authoritative_local_record(record)) {
            continue;
        }
        result.entries.push_back(resolve_record_entry(record, ruleset));
    }
    std::sort(result.entries.begin(), result.entries.end(), ranking_entry_better);
    if (limit > 0 && static_cast<int>(result.entries.size()) > limit) {
        result.entries.resize(static_cast<size_t>(limit));
    }

    for (size_t i = 0; i < result.entries.size(); ++i) {
        result.entries[i].placement = static_cast<int>(i) + 1;
        result.entries[i].verified = false;
    }

    if (result.entries.empty()) {
        result.message = "No local ranking records yet.";
    }

    return result;
}

local_submit_result submit_local_result_detailed(const chart_meta& chart, const result_data& result) {
    local_submit_result submission;
    if (chart.chart_id.empty() || result.failed) {
        return submission;
    }

    std::vector<stored_local_record> records = load_local_records(chart.chart_id);
    retain_authoritative_local_records(records);

    const scoring_ruleset_runtime::ruleset ruleset = scoring_ruleset_runtime::current_ruleset();
    std::vector<entry> entries;
    entries.reserve(records.size());
    for (const stored_local_record& record : records) {
        entries.push_back(resolve_record_entry(record, ruleset));
    }
    std::sort(entries.begin(), entries.end(), ranking_entry_better);

    const std::string recorded_at = current_timestamp_utc();
    const std::string player_display_name = current_local_player_display_name();
    if (result.note_results.empty()) {
        return submission;
    }

    stored_local_record new_record;
    new_record.recorded_at = recorded_at;
    new_record.player_display_name = player_display_name;
    new_record.scoring_ruleset_version =
        result.scoring_ruleset_version.empty()
            ? ruleset.ruleset_version
            : result.scoring_ruleset_version;
    new_record.scoring_accepted_input =
        result.scoring_accepted_input.empty()
            ? std::string(kAuthoritativeAcceptedInput)
            : normalize_accepted_input(result.scoring_accepted_input);
    new_record.note_results = result.note_results;
    entry new_entry = resolve_record_entry(new_record, ruleset);

    submission.best_updated =
        entries.empty() || ranking_entry_better(new_entry, entries.front());

    records.push_back(new_record);
    entries.push_back(new_entry);
    std::sort(entries.begin(), entries.end(), ranking_entry_better);
    if (entries.size() > 50) {
        entries.resize(50);
    }
    std::sort(records.begin(), records.end(), [&ruleset](const stored_local_record& left, const stored_local_record& right) {
        return ranking_entry_better(resolve_record_entry(left, ruleset), resolve_record_entry(right, ruleset));
    });
    if (records.size() > 50) {
        records.resize(50);
    }

    submission.success = save_local_records(chart.chart_id, records);
    if (submission.success) {
        submission.submitted_entry = std::move(new_entry);
    }
    return submission;
}

bool submit_local_result(const chart_meta& chart, const result_data& result) {
    return submit_local_result_detailed(chart, result).success;
}

bool should_attempt_online_submit(const local_submit_result& local_result) {
    return local_result.success && local_result.submitted_entry.has_value();
}

bool warm_scoring_ruleset_cache(bool force_refresh) {
    const std::string server_url = display_ruleset_server_url();
    if (server_url.empty()) {
        return false;
    }

    if (!force_refresh) {
        if (const std::optional<ranking_client::scoring_ruleset> cached =
                load_cached_ruleset_for_server(server_url);
            cached.has_value()) {
            scoring_ruleset_runtime::apply_server_ruleset(*cached);
            return true;
        }
    }

    return fetch_and_cache_scoring_ruleset(server_url).has_value();
}

bool refresh_scoring_ruleset_cache_for_chart_start(const chart_meta& chart, bool force_refresh) {
    if (!chart_obviously_eligible_for_online_ranking(chart)) {
        return false;
    }

    return warm_scoring_ruleset_cache(force_refresh);
}

online_submit_result submit_online_result(const song_data& song,
                                          const std::string& chart_path,
                                          const chart_meta& chart,
                                          const result_data& result,
                                          const std::string& recorded_at) {
    online_submit_result submission;
    if (!chart_obviously_eligible_for_online_ranking(chart)) {
        return submission;
    }

    const auth::session_summary summary = auth::load_session_summary();
    if (!summary.logged_in) {
        return submission;
    }

    if (!summary.email_verified) {
        submission.message = "Verify your email on the Web to submit online rankings.";
        return submission;
    }

    const std::optional<auth::session> stored = auth::load_saved_session();
    if (!stored.has_value()) {
        submission.message = "Sign in to submit online rankings.";
        return submission;
    }

    const verification_result verification =
        verify_chart_manifest(song, chart_path, chart, stored->server_url);
    if (!verification.success) {
        submission.message = verification.message;
        return submission;
    }

    std::optional<ranking_client::scoring_ruleset> ruleset =
        load_cached_ruleset_for_server(stored->server_url);
    if (!ruleset.has_value()) {
        ruleset = fetch_and_cache_scoring_ruleset(stored->server_url);
    }

    if (!ruleset.has_value()) {
        submission.message = "Failed to fetch scoring ruleset.";
        return submission;
    }

    ruleset->accepted_input = normalize_accepted_input(ruleset->accepted_input);
    if (!ruleset->active ||
        ruleset->accepted_input != kAuthoritativeAcceptedInput) {
        submission.message = "The server does not accept this ranking submission format.";
        return submission;
    }

    const std::string submission_ruleset_version =
        result.scoring_ruleset_version.empty()
            ? ruleset->ruleset_version
            : result.scoring_ruleset_version;
    const std::string submission_input_format =
        result.scoring_accepted_input.empty()
            ? std::string(kAuthoritativeAcceptedInput)
            : normalize_accepted_input(result.scoring_accepted_input);
    if (submission_input_format != ruleset->accepted_input) {
        submission.message = "The score input format changed. Start the chart again to submit online ranking.";
        return submission;
    }
    if (submission_ruleset_version != ruleset->ruleset_version) {
        fetch_and_cache_scoring_ruleset(stored->server_url);
        submission.message = "The scoring ruleset changed. Start the chart again to submit online ranking.";
        return submission;
    }

    submission.attempted = true;

    ranking_client::submit_operation_result request =
        ranking_client::submit_chart_ranking(
            stored->server_url,
            stored->access_token,
            expected_remote_chart_id(stored->server_url, chart.chart_id),
            result,
            recorded_at,
            submission_ruleset_version);

    if (request.unauthorized) {
        const auth::operation_result restored = auth::restore_saved_session();
        if (!restored.success || !restored.session_data.has_value()) {
            submission.success = false;
            submission.message = restored.message.empty()
                ? "Sign in to submit online rankings."
                : restored.message;
            return submission;
        }

        request = ranking_client::submit_chart_ranking(
            restored.session_data->server_url,
            restored.session_data->access_token,
            expected_remote_chart_id(restored.session_data->server_url, chart.chart_id),
            result,
            recorded_at,
            submission_ruleset_version);
    }

    submission.success = request.success;
    submission.message = request.message;
    if (request.submission.has_value()) {
        submission.updated = request.submission->updated;
        submission.entry = request.submission->entry;
        if (submission.message.empty()) {
            submission.message = request.submission->message;
        }
    }

    return submission;
}

}  // namespace ranking_service
