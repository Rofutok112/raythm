#include "play/play_chart_filter.h"

#include <algorithm>

namespace play_chart_filter {

chart_data prepare_chart_for_playback(const chart_data& chart, int start_tick) {
    chart_data prepared = chart;
    const int clamped_start_tick = std::max(0, start_tick);
    if (clamped_start_tick <= 0) {
        return prepared;
    }

    std::erase_if(prepared.notes, [clamped_start_tick](const note_data& note) {
        return note.tick < clamped_start_tick;
    });
    return prepared;
}

}  // namespace play_chart_filter
