#pragma once

#include <cstddef>
#include <deque>
#include <vector>

#include "data_models.h"
#include "play_scroll_map.h"

class play_note_draw_queue final {
public:
    void clear();
    void init_from_note_states(int key_count, const std::vector<note_state>& note_states,
                               const play_scroll_map* scroll_map = nullptr);
    void update_visible_window(const std::vector<note_state>& note_states, float lane_speed, float judgement_z,
                               float lane_start_z, float lane_end_z, double visual_ms);

    [[nodiscard]] bool has_active_notes() const;
    [[nodiscard]] const std::vector<size_t>& active_indices_for_lane(int lane) const;
    [[nodiscard]] double visual_target_ms(size_t note_index) const;
    [[nodiscard]] double visual_end_target_ms(size_t note_index) const;

private:
    int key_count_ = 0;
    std::vector<double> note_visual_target_ms_;
    std::vector<double> note_visual_end_target_ms_;
    std::vector<std::deque<size_t>> inactive_draw_notes_by_lane_;
    std::vector<std::vector<size_t>> active_draw_notes_by_lane_;
};
