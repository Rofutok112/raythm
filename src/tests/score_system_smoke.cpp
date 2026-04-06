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

    if (result.score <= 0 || result.accuracy != 57.5f) {
        std::cerr << "Score normalization failed\n";
        return EXIT_FAILURE;
    }

    if (score.get_live_accuracy() != 57.5f) {
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

    score_system fc_s_rank;
    fc_s_rank.init(20);
    for (int i = 0; i < 16; ++i) {
        fc_s_rank.on_judge({judge_result::perfect, 0.0, 0});
    }
    for (int i = 0; i < 4; ++i) {
        fc_s_rank.on_judge({judge_result::great, 0.0, 0});
    }
    if (fc_s_rank.get_result_data().clear_rank != rank::s) {
        std::cerr << "Full combo 95%+ should be rank S\n";
        return EXIT_FAILURE;
    }

    score_system non_fc_high_accuracy;
    non_fc_high_accuracy.init(100);
    for (int i = 0; i < 50; ++i) {
        non_fc_high_accuracy.on_judge({judge_result::perfect, 0.0, 0});
    }
    non_fc_high_accuracy.on_judge({judge_result::miss, 0.0, 0});
    for (int i = 0; i < 49; ++i) {
        non_fc_high_accuracy.on_judge({judge_result::perfect, 0.0, 0});
    }
    const result_data non_fc_result = non_fc_high_accuracy.get_result_data();
    if (non_fc_result.accuracy < 99.0f || non_fc_result.is_full_combo || non_fc_result.clear_rank != rank::a) {
        std::cerr << "99% without full combo should be rank A\n";
        return EXIT_FAILURE;
    }

    score_system fc_lower_accuracy;
    fc_lower_accuracy.init(100);
    for (int i = 0; i < 80; ++i) {
        fc_lower_accuracy.on_judge({judge_result::perfect, 0.0, 0});
    }
    for (int i = 0; i < 20; ++i) {
        fc_lower_accuracy.on_judge({judge_result::great, 0.0, 0});
    }

    if (!(fc_lower_accuracy.get_live_accuracy() < non_fc_high_accuracy.get_live_accuracy()) ||
        !(fc_lower_accuracy.get_score() > non_fc_high_accuracy.get_score())) {
        std::cerr << "Score should reward sustained combo more than raw accuracy alone\n";
        return EXIT_FAILURE;
    }

    std::cout << "score_system smoke test passed\n";
    return EXIT_SUCCESS;
}
