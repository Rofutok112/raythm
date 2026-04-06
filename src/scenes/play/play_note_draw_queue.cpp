#include "play_note_draw_queue.h"

#include <algorithm>

namespace {

const std::vector<size_t>& empty_active_indices() {
    static const std::vector<size_t> kEmpty;
    return kEmpty;
}

}  // namespace

void play_note_draw_queue::clear() {
    key_count_ = 0;
    note_target_ms_.clear();
    inactive_draw_notes_by_lane_.clear();
    active_draw_notes_by_lane_.clear();
}

void play_note_draw_queue::init_from_note_states(int key_count, const std::vector<note_state>& note_states) {
    clear();
    key_count_ = key_count;
    note_target_ms_.resize(note_states.size());
    inactive_draw_notes_by_lane_.assign(static_cast<size_t>(key_count_), {});
    active_draw_notes_by_lane_.assign(static_cast<size_t>(key_count_), {});

    for (size_t i = 0; i < note_states.size(); ++i) {
        note_target_ms_[i] = note_states[i].target_ms;
        const int lane = note_states[i].note_ref.lane;
        if (lane >= 0 && lane < key_count_) {
            inactive_draw_notes_by_lane_[static_cast<size_t>(lane)].push_back(i);
        }
    }

    for (int lane = 0; lane < key_count_; ++lane) {
        std::vector<size_t> sorted_indices(inactive_draw_notes_by_lane_[static_cast<size_t>(lane)].begin(),
                                           inactive_draw_notes_by_lane_[static_cast<size_t>(lane)].end());
        std::sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t left, size_t right) {
            if (note_target_ms_[left] != note_target_ms_[right]) {
                return note_target_ms_[left] < note_target_ms_[right];
            }
            return left < right;
        });
        inactive_draw_notes_by_lane_[static_cast<size_t>(lane)] =
            std::deque<size_t>(sorted_indices.begin(), sorted_indices.end());
    }
}

void play_note_draw_queue::update_visible_window(const std::vector<note_state>& note_states, float lane_speed,
                                                 float judgement_z, float lane_start_z, float lane_end_z,
                                                 double visual_ms) {
    for (int lane = 0; lane < key_count_; ++lane) {
        std::deque<size_t>& inactive = inactive_draw_notes_by_lane_[static_cast<size_t>(lane)];
        std::vector<size_t>& active = active_draw_notes_by_lane_[static_cast<size_t>(lane)];
        while (!inactive.empty()) {
            const size_t idx = inactive.front();
            const float head_z = static_cast<float>(judgement_z + lane_speed * (note_target_ms_[idx] - visual_ms));
            if (head_z > lane_end_z) {
                break;
            }
            inactive.pop_front();
            active.push_back(idx);
        }
    }

    for (int lane = 0; lane < key_count_; ++lane) {
        std::vector<size_t>& active = active_draw_notes_by_lane_[static_cast<size_t>(lane)];
        std::erase_if(active, [&](size_t idx) {
            const note_state& state = note_states[idx];
            if (state.note_ref.type == note_type::hold) {
                return state.is_completed();
            }
            if (state.is_completed() && !state.is_holding()) {
                return true;
            }
            if (state.is_holding()) {
                return false;
            }
            const float head_z = static_cast<float>(judgement_z + lane_speed * (note_target_ms_[idx] - visual_ms));
            return head_z < lane_start_z;
        });
    }
}

bool play_note_draw_queue::has_active_notes() const {
    for (const std::vector<size_t>& active : active_draw_notes_by_lane_) {
        if (!active.empty()) {
            return true;
        }
    }
    return false;
}

const std::vector<size_t>& play_note_draw_queue::active_indices_for_lane(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return empty_active_indices();
    }
    return active_draw_notes_by_lane_[static_cast<size_t>(lane)];
}
