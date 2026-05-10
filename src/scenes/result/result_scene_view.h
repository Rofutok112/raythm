#pragma once

#include <optional>
#include <string>

#include "data_models.h"
#include "ranking_service.h"
#include "raylib.h"

namespace result_scene_view {

enum class action {
    none,
    retry,
    song_select,
    replay,
};

struct model {
    const result_data& result;
    const song_data& song;
    const chart_meta& chart;
    int key_count = 4;
    const Texture2D* jacket_texture = nullptr;
    const ranking_service::local_submit_result* local_submit = nullptr;
    const ranking_service::online_submit_result* online_submit = nullptr;
    std::string online_status_message;
    bool online_status_is_error = false;
    float reveal_t = 1.0f;
    double now = 0.0;
};

action hit_test_action(Vector2 point);
void draw(const model& data);

}  // namespace result_scene_view
