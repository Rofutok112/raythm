#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "app_paths.h"
#include "ranking_service.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

bool write_local_ranking_plaintext(const std::string& chart_id, const std::string& plaintext) {
    app_paths::ensure_directories();
    const std::filesystem::path path = app_paths::local_ranking_path(chart_id);

#ifdef _WIN32
    constexpr wchar_t kEntropyLabel[] = L"raythm-local-ranking";
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

    std::vector<std::byte> encrypted(output_blob.cbData);
    std::memcpy(encrypted.data(), output_blob.pbData, output_blob.cbData);
    LocalFree(output_blob.pbData);

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

int main() {
    namespace fs = std::filesystem;

    const fs::path temp_root = fs::temp_directory_path() / "raythm-ranking-smoke";
    std::error_code ec;
    fs::remove_all(temp_root, ec);
    fs::create_directories(temp_root, ec);
    if (ec) {
        std::cerr << "Failed to prepare temp ranking directory\n";
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    _putenv_s("LOCALAPPDATA", temp_root.string().c_str());
#endif

    chart_meta chart;
    chart.chart_id = "smoke-chart";

    result_data lower_result;
    lower_result.note_results = {
        {.event_index = 0, .result = judge_result::perfect, .offset_ms = -4.0},
        {.event_index = 1, .result = judge_result::great, .offset_ms = 7.0},
        {.event_index = 2, .result = judge_result::good, .offset_ms = 12.0},
        {.event_index = 3, .result = judge_result::perfect, .offset_ms = -2.0},
    };

    result_data higher_result;
    higher_result.note_results = {
        {.event_index = 0, .result = judge_result::perfect, .offset_ms = -1.0},
        {.event_index = 1, .result = judge_result::perfect, .offset_ms = 2.0},
        {.event_index = 2, .result = judge_result::perfect, .offset_ms = -3.0},
        {.event_index = 3, .result = judge_result::perfect, .offset_ms = 1.0},
    };

    const ranking_service::local_submit_result lower_submission =
        ranking_service::submit_local_result_detailed(chart, lower_result);
    const ranking_service::local_submit_result higher_submission =
        ranking_service::submit_local_result_detailed(chart, higher_result);
    const ranking_service::local_submit_result duplicate_lower_submission =
        ranking_service::submit_local_result_detailed(chart, lower_result);

    if (!lower_submission.success ||
        !higher_submission.success ||
        !duplicate_lower_submission.success) {
        std::cerr << "Failed to save encrypted local ranking\n";
        return EXIT_FAILURE;
    }

    if (!lower_submission.best_updated ||
        !higher_submission.best_updated ||
        duplicate_lower_submission.best_updated) {
        std::cerr << "Local best update detection failed\n";
        return EXIT_FAILURE;
    }

    if (!ranking_service::should_attempt_online_submit(lower_submission) ||
        !ranking_service::should_attempt_online_submit(higher_submission) ||
        !ranking_service::should_attempt_online_submit(duplicate_lower_submission)) {
        std::cerr << "Online submit eligibility should not depend on local best updates\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing local_listing =
        ranking_service::load_chart_ranking(chart.chart_id, ranking_service::source::local, 50);
    if (local_listing.entries.size() != 3 ||
        local_listing.entries[0].score <= local_listing.entries[1].score ||
        local_listing.entries[0].max_combo != 4 ||
        local_listing.entries[0].placement != 1 ||
        local_listing.entries[1].placement != 2 ||
        local_listing.entries[2].placement != 3) {
        std::cerr << "Local ranking ordering failed\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing online_listing =
        ranking_service::load_chart_ranking(chart.chart_id, ranking_service::source::online, 50);
    if (online_listing.available || online_listing.message.empty()) {
        std::cerr << "Online unavailable listing failed\n";
        return EXIT_FAILURE;
    }

    chart_meta legacy_chart;
    legacy_chart.chart_id = "legacy-chart";
    const std::string legacy_content =
        "RAYTHM_LOCAL_RANKING_V6\n"
        "legacy\t2026-04-20T00:00:00Z\tGuest\t100.0000\t1\t999\t999999\tss\n";
    if (!write_local_ranking_plaintext(legacy_chart.chart_id, legacy_content)) {
        std::cerr << "Failed to prepare legacy local ranking\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing hidden_legacy_listing =
        ranking_service::load_chart_ranking(legacy_chart.chart_id, ranking_service::source::local, 50);
    if (!hidden_legacy_listing.entries.empty()) {
        std::cerr << "Legacy local rankings should be hidden\n";
        return EXIT_FAILURE;
    }

    result_data migrated_result;
    migrated_result.note_results = {
        {.event_index = 0, .result = judge_result::great, .offset_ms = 3.0},
        {.event_index = 1, .result = judge_result::good, .offset_ms = 8.0},
    };

    const ranking_service::local_submit_result migrated_submission =
        ranking_service::submit_local_result_detailed(legacy_chart, migrated_result);
    if (!migrated_submission.success || !migrated_submission.best_updated) {
        std::cerr << "Legacy local rankings should not block migrated records\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing migrated_listing =
        ranking_service::load_chart_ranking(legacy_chart.chart_id, ranking_service::source::local, 50);
    if (migrated_listing.entries.size() != 1 ||
        migrated_listing.entries.front().score == 999999) {
        std::cerr << "Migrated local ranking listing failed\n";
        return EXIT_FAILURE;
    }

    chart_meta old_v5_chart;
    old_v5_chart.chart_id = "old-v5-chart";
    const std::string old_v5_content =
        "RAYTHM_LOCAL_RANKING_V5\n"
        "record\t2026-04-20T00:00:00Z\tGuest\t0,perfect,0.000;1,perfect,0.000\n";
    if (!write_local_ranking_plaintext(old_v5_chart.chart_id, old_v5_content)) {
        std::cerr << "Failed to prepare old V5 local ranking\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing hidden_old_v5_listing =
        ranking_service::load_chart_ranking(old_v5_chart.chart_id, ranking_service::source::local, 50);
    if (!hidden_old_v5_listing.entries.empty()) {
        std::cerr << "Old V5 local rankings should be hidden\n";
        return EXIT_FAILURE;
    }

    const ranking_service::local_submit_result old_v5_migrated_submission =
        ranking_service::submit_local_result_detailed(old_v5_chart, migrated_result);
    if (!old_v5_migrated_submission.success || !old_v5_migrated_submission.best_updated) {
        std::cerr << "Old V5 local rankings should not block new V6 records\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing old_v5_migrated_listing =
        ranking_service::load_chart_ranking(old_v5_chart.chart_id, ranking_service::source::local, 50);
    if (old_v5_migrated_listing.entries.size() != 1 ||
        old_v5_migrated_listing.entries.front().score == 1000000) {
        std::cerr << "Old V5 local ranking migration failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "ranking_service smoke test passed\n";
    return EXIT_SUCCESS;
}
