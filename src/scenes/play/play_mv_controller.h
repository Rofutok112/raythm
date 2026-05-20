#pragma once

#include <memory>
#include <vector>

#include "play/play_session_types.h"

namespace mv {
class mv_runtime;
}

class play_mv_controller final {
public:
    play_mv_controller();
    ~play_mv_controller();

    play_mv_controller(const play_mv_controller&) = delete;
    play_mv_controller& operator=(const play_mv_controller&) = delete;

    void load_for_song(const std::optional<song_data>& song);
    void reset();
    void draw(const play_session_state& state, double visual_time_ms);

private:
    std::unique_ptr<mv::mv_runtime> runtime_;
    std::vector<float> spectrum_buffer_;
    std::vector<float> oscilloscope_buffer_;
};
