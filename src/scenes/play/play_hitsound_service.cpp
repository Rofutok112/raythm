#include "play/play_hitsound_service.h"

#include <algorithm>

#include "audio_manager.h"

float play_hitsound_service::pan_for_event(const judge_event& event, int key_count, float pan_strength) {
    if (key_count <= 1) {
        return 0.0f;
    }

    const int clamped_lane = std::clamp(event.lane, 0, key_count - 1);
    const int clamped_width = std::clamp(event.lane_width, 1, key_count - clamped_lane);
    const float note_center = static_cast<float>(clamped_lane) + static_cast<float>(clamped_width) * 0.5f;
    const float normalized_pan = note_center / static_cast<float>(key_count) * 2.0f - 1.0f;
    return std::clamp(normalized_pan * std::clamp(pan_strength, 0.0f, 1.0f), -1.0f, 1.0f);
}

void play_hitsound_service::play(const play_hitsound_paths& hitsounds,
                                 const judge_event& event,
                                 int key_count,
                                 float pan_strength) {
    const std::string& path = hitsounds.path_for(event);
    if (path.empty()) {
        return;
    }

    audio_manager::instance().play_se(path, 1.0f, pan_for_event(event, key_count, pan_strength));
}
