#pragma once

#include "data_models.h"

namespace play_chart_filter {

chart_data prepare_chart_for_playback(const chart_data& chart, int start_tick);

}  // namespace play_chart_filter
