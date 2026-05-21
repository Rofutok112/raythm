#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "ui_text_input.h"

namespace song_create::timing_controller {

struct context {
    ui::text_input_state& bpm_input;
    ui::text_input_state& bar_input;
    ui::text_input_state& event_bpm_input;
    ui::text_input_state& numerator_input;
    ui::text_input_state& denominator_input;
    std::vector<timing_event>& events;
    std::optional<size_t>& selected_event_index;
    float& event_scroll_offset;
    std::string& import_status;
    std::string& error;
};

void ensure_initialized(context ctx);
void sync_selected_inputs(context ctx);
void add_event(context ctx, timing_event_type type);
void delete_selected_event(context ctx);
bool apply_selected_event(context ctx);
bool flush_selected_inputs(context ctx);
bool import_midi_timing(context ctx, const std::string& midi_path);
std::vector<timing_event> validated_events(context ctx, float base_bpm, bool& ok);

}  // namespace song_create::timing_controller
