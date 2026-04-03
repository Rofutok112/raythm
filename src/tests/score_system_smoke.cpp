#include <cstdlib>
#include <iostream>

#include "score_system.h"

int main() {
    score_system score;
    score.init(4);

    score.on_judge({judge_result::perfect, -10.0, 0});
    score.on_judge({judge_result::great, 12.0, 1});
    score.on_judge({judge_result::good, 40.0, 2});
    score.on_judge({judge_result::miss, 150.0, 3});

    if (score.get_combo() != 0) {
        std::cerr << "Combo reset failed\n";
        return EXIT_FAILURE;
    }

    const result_data result = score.get_result_data();
    if (result.max_combo != 3) {
        std::cerr << "Max combo aggregation failed\n";
        return EXIT_FAILURE;
    }

    if (result.judge_counts[0] != 1 || result.judge_counts[1] != 1 || result.judge_counts[2] != 1 ||
        result.judge_counts[4] != 1) {
        std::cerr << "Judge counts aggregation failed\n";
        return EXIT_FAILURE;
    }

    if (result.fast_count != 1 || result.slow_count != 3) {
        std::cerr << "Fast/slow aggregation failed\n";
        return EXIT_FAILURE;
    }

    if (result.is_full_combo || result.is_all_perfect) {
        std::cerr << "Result flag aggregation failed\n";
        return EXIT_FAILURE;
    }

    if (result.score <= 0 || result.accuracy <= 0.0f) {
        std::cerr << "Score normalization failed\n";
        return EXIT_FAILURE;
    }

    if (score.get_live_accuracy() != 62.5f) {
        std::cerr << "Live accuracy aggregation failed\n";
        return EXIT_FAILURE;
    }

    gauge life_gauge;
    life_gauge.on_judge(judge_result::perfect);
    life_gauge.on_judge(judge_result::great);
    life_gauge.on_judge(judge_result::bad);
    life_gauge.on_judge(judge_result::miss);

    if (life_gauge.get_value() != 84.0f) {
        std::cerr << "Gauge accumulation failed\n";
        return EXIT_FAILURE;
    }

    if (!life_gauge.is_cleared()) {
        std::cerr << "Gauge clear threshold failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "score_system smoke test passed\n";
    return EXIT_SUCCESS;
}
