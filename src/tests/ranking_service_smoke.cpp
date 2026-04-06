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
    lower_result.clear_rank = rank::a;
    lower_result.accuracy = 92.5f;
    lower_result.max_combo = 321;
    lower_result.score = 654321;

    result_data higher_result;
    higher_result.clear_rank = rank::s;
    higher_result.accuracy = 97.25f;
    higher_result.max_combo = 654;
    higher_result.score = 765432;

    if (!ranking_service::submit_local_result(chart, lower_result) ||
        !ranking_service::submit_local_result(chart, higher_result)) {
        std::cerr << "Failed to save encrypted local ranking\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing local_listing =
        ranking_service::load_chart_ranking(chart.chart_id, ranking_service::source::local, 50);
    if (local_listing.entries.size() != 2 ||
        local_listing.entries[0].score != higher_result.score ||
        local_listing.entries[0].max_combo != higher_result.max_combo ||
        local_listing.entries[0].placement != 1 ||
        local_listing.entries[1].placement != 2) {
        std::cerr << "Local ranking ordering failed\n";
        return EXIT_FAILURE;
    }

    const ranking_service::listing online_listing =
        ranking_service::load_chart_ranking(chart.chart_id, ranking_service::source::online, 50);
    if (online_listing.available || online_listing.message.empty()) {
        std::cerr << "Online placeholder listing failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "ranking_service smoke test passed\n";
    return EXIT_SUCCESS;
}
