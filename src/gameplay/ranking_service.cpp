#include "ranking_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <string_view>
#include <vector>

#include "app_paths.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

constexpr std::string_view kFileHeaderV1 = "RAYTHM_LOCAL_RANKING_V1";
constexpr std::string_view kFileHeaderV2 = "RAYTHM_LOCAL_RANKING_V2";
constexpr std::string_view kFileHeaderV3 = "RAYTHM_LOCAL_RANKING_V3";
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

std::string serialize_entries(const std::vector<ranking_service::entry>& entries) {
    std::ostringstream out;
    out << kFileHeaderV3 << '\n';
    for (const ranking_service::entry& entry : entries) {
        out << std::fixed << std::setprecision(4) << entry.accuracy << '\t'
            << (entry.is_full_combo ? 1 : 0) << '\t'
            << entry.max_combo << '\t'
            << entry.score << '\t'
            << entry.recorded_at << '\n';
    }
    return out.str();
}

enum class file_version { v1, v2, v3 };

std::vector<ranking_service::entry> parse_entries(const std::string& content) {
    std::vector<ranking_service::entry> entries;
    std::istringstream input(content);
    std::string line;
    if (!std::getline(input, line)) {
        return entries;
    }
    const std::string header = trim(line);
    file_version version;
    if (header == kFileHeaderV3) {
        version = file_version::v3;
    } else if (header == kFileHeaderV2) {
        version = file_version::v2;
    } else if (header == kFileHeaderV1) {
        version = file_version::v1;
    } else {
        return entries;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream row(line);

        try {
            ranking_service::entry entry;

            if (version == file_version::v3) {
                // V3: accuracy \t is_full_combo \t max_combo \t score \t timestamp
                std::string accuracy_token, fc_token, combo_token, score_token, timestamp_token;
                if (!std::getline(row, accuracy_token, '\t') ||
                    !std::getline(row, fc_token, '\t') ||
                    !std::getline(row, combo_token, '\t') ||
                    !std::getline(row, score_token, '\t') ||
                    !std::getline(row, timestamp_token)) {
                    continue;
                }
                entry.accuracy = std::clamp(std::stof(trim(accuracy_token)), 0.0f, 100.0f);
                entry.is_full_combo = trim(fc_token) == "1";
                entry.max_combo = std::clamp(std::stoi(trim(combo_token)), 0, 999999);
                entry.score = std::clamp(std::stoi(trim(score_token)), 0, 1000000);
                entry.recorded_at = trim(timestamp_token);
            } else {
                // V1/V2: rank \t accuracy \t [max_combo \t] score \t timestamp
                std::string rank_token, accuracy_token, combo_token, score_token, timestamp_token;
                if (!std::getline(row, rank_token, '\t') ||
                    !std::getline(row, accuracy_token, '\t')) {
                    continue;
                }
                if (version == file_version::v2) {
                    if (!std::getline(row, combo_token, '\t') ||
                        !std::getline(row, score_token, '\t') ||
                        !std::getline(row, timestamp_token)) {
                        continue;
                    }
                } else {
                    if (!std::getline(row, score_token, '\t') ||
                        !std::getline(row, timestamp_token)) {
                        continue;
                    }
                }

                entry.accuracy = std::clamp(std::stof(trim(accuracy_token)), 0.0f, 100.0f);
                entry.max_combo = (version == file_version::v2) ? std::clamp(std::stoi(trim(combo_token)), 0, 999999) : 0;
                entry.score = std::clamp(std::stoi(trim(score_token)), 0, 1000000);
                entry.recorded_at = trim(timestamp_token);

                // V1/V2 にはフルコンボ情報がないので、保存された rank が SS/S ならフルコンボとみなす。
                const int stored_rank = std::clamp(std::stoi(trim(rank_token)), 0, 6);
                entry.is_full_combo = (stored_rank == static_cast<int>(rank::ss) || stored_rank == static_cast<int>(rank::s));
            }

            entry.verified = false;
            entries.push_back(std::move(entry));
        } catch (...) {
            continue;
        }
    }

    return entries;
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

std::vector<ranking_service::entry> load_local_entries(const std::string& chart_id) {
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
    return parse_entries(plaintext);
#else
    return parse_entries(std::string(bytes.begin(), bytes.end()));
#endif
}

bool save_local_entries(const std::string& chart_id, const std::vector<ranking_service::entry>& entries) {
    if (chart_id.empty()) {
        return false;
    }

    const std::string plaintext = serialize_entries(entries);
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
        result.available = false;
        result.message = "Online ranking coming soon.";
        return result;
    }

    result.entries = load_local_entries(chart_id);
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

bool submit_local_result(const chart_meta& chart, const result_data& result) {
    if (chart.chart_id.empty() || result.failed) {
        return false;
    }

    std::vector<entry> entries = load_local_entries(chart.chart_id);
    entries.push_back({
        .placement = 0,
        .accuracy = result.accuracy,
        .is_full_combo = result.is_full_combo,
        .max_combo = result.max_combo,
        .score = result.score,
        .recorded_at = current_timestamp_utc(),
        .verified = false,
    });

    std::sort(entries.begin(), entries.end(), ranking_entry_better);
    if (entries.size() > 50) {
        entries.resize(50);
    }

    return save_local_entries(chart.chart_id, entries);
}

}  // namespace ranking_service
