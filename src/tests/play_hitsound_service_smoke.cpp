#include <cmath>
#include <cstdlib>
#include <iostream>

#include "play/play_hitsound_service.h"

namespace {
bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 0.001f;
}
}

int main() {
    judge_event left;
    left.lane = 0;
    left.lane_width = 1;
    if (!near(play_hitsound_service::pan_for_event(left, 4, 1.0f), -0.75f)) {
        std::cerr << "Expected leftmost 4K lane to pan left without reaching the hard edge\n";
        return EXIT_FAILURE;
    }

    judge_event center_left;
    center_left.lane = 1;
    center_left.lane_width = 1;
    if (!near(play_hitsound_service::pan_for_event(center_left, 4, 1.0f), -0.25f)) {
        std::cerr << "Expected inner 4K lane to stay near center-left\n";
        return EXIT_FAILURE;
    }

    judge_event wide_center;
    wide_center.lane = 1;
    wide_center.lane_width = 2;
    if (!near(play_hitsound_service::pan_for_event(wide_center, 4, 1.0f), 0.0f)) {
        std::cerr << "Expected centered wide note to pan center\n";
        return EXIT_FAILURE;
    }

    judge_event right;
    right.lane = 5;
    right.lane_width = 1;
    if (!near(play_hitsound_service::pan_for_event(right, 6, 0.6f), 0.5f)) {
        std::cerr << "Expected pan strength to scale lane pan\n";
        return EXIT_FAILURE;
    }

    if (!near(play_hitsound_service::pan_for_event(right, 1, 1.0f), 0.0f)) {
        std::cerr << "Expected one-lane play to remain centered\n";
        return EXIT_FAILURE;
    }

    std::cout << "play_hitsound_service smoke test passed\n";
    return EXIT_SUCCESS;
}
