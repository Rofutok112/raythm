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

    scoring_ruleset_runtime::ruleset custom_ruleset = scoring_ruleset_runtime::make_default_ruleset();
    custom_ruleset.ruleset_version = "smoke-ruleset";
    custom_ruleset.max_score = 123456;
    scoring_ruleset_runtime::apply_server_ruleset(custom_ruleset);
    score_system ruleset_score;
    ruleset_score.init(1);
    ruleset_score.on_judge({judge_result::perfect, 0.0, 0});
    const result_data ruleset_result = ruleset_score.get_result_data();
    if (ruleset_result.scoring_ruleset_version != "smoke-ruleset" ||
        ruleset_result.scoring_accepted_input != "noteResultsV1" ||
        ruleset_result.score != 123456) {
        std::cerr << "Score result should retain the ruleset used during play\n";
        return EXIT_FAILURE;
    }
    scoring_ruleset_runtime::apply_server_ruleset(scoring_ruleset_runtime::make_default_ruleset());

    score_system combo_heavy_score;
    combo_heavy_score.init(4);
    combo_heavy_score.on_judge({judge_result::perfect, 0.0, 0});
    combo_heavy_score.on_judge({judge_result::miss, 0.0, 0});
    combo_heavy_score.on_judge({judge_result::perfect, 0.0, 0});
    combo_heavy_score.on_judge({judge_result::perfect, 0.0, 0});
    const int combo_heavy_result_score = combo_heavy_score.get_score();

    scoring_ruleset_runtime::ruleset combo_light_ruleset = scoring_ruleset_runtime::make_default_ruleset();
    combo_light_ruleset.score_model = "combo-light-v1";
    combo_light_ruleset.ruleset_version = "combo-light-smoke";
    scoring_ruleset_runtime::apply_server_ruleset(combo_light_ruleset);
    score_system combo_light_score;
    combo_light_score.init(4);
    combo_light_score.on_judge({judge_result::perfect, 0.0, 0});
    combo_light_score.on_judge({judge_result::miss, 0.0, 0});
    combo_light_score.on_judge({judge_result::perfect, 0.0, 0});
    combo_light_score.on_judge({judge_result::perfect, 0.0, 0});
    if (combo_light_score.get_score() <= combo_heavy_result_score) {
        std::cerr << "Combo-light score model should reduce the combo break penalty\n";
        return EXIT_FAILURE;
    }
    scoring_ruleset_runtime::apply_server_ruleset(scoring_ruleset_runtime::make_default_ruleset());

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
    if (non_fc_result.accuracy < 99.0f || non_fc_result.is_full_combo || non_fc_result.clear_rank != rank::aa) {
        std::cerr << "99% without full combo should be rank AA\n";
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
