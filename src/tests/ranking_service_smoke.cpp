#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "app_paths.h"
#include "ranking_service.h"

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

    std::cout << "ranking_service smoke test passed\n";
    return EXIT_SUCCESS;
}
