#include "song_create/song_create_timing_controller.h"

#include <algorithm>

#include "localization/localization.h"
#include "raylib.h"
#include "song_create/song_create_midi_importer.h"
#include "song_create/song_create_timing_service.h"

namespace song_create::timing_controller {
namespace {

bool parse_int_text(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_float_text(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

float base_bpm_from_input(const ui::text_input_state& input) {
    float base_bpm = 120.0f;
    if (!input.value.empty()) {
        float parsed = 0.0f;
        if (parse_float_text(input.value, parsed) && parsed > 0.0f) {
            base_bpm = parsed;
        }
    }
    return base_bpm;
}

}  // namespace

void ensure_initialized(context ctx) {
    song_create::timing_service::ensure_base_events(ctx.events, base_bpm_from_input(ctx.bpm_input));
    if (ctx.selected_event_index.has_value() && *ctx.selected_event_index >= ctx.events.size()) {
        ctx.selected_event_index.reset();
    }
    if (!ctx.selected_event_index.has_value() && !ctx.events.empty()) {
        ctx.selected_event_index = 0;
    }
    sync_selected_inputs(ctx);
}

void sync_selected_inputs(context ctx) {
    if (!ctx.selected_event_index.has_value() || *ctx.selected_event_index >= ctx.events.size()) {
        ctx.bar_input.value.clear();
        ctx.event_bpm_input.value.clear();
        ctx.numerator_input.value.clear();
        ctx.denominator_input.value.clear();
        return;
    }

    const editor_meter_map meter_map = song_create::timing_service::build_meter_map(ctx.events, 480);
    const timing_event& event = ctx.events[*ctx.selected_event_index];
    ctx.bar_input.value = meter_map.bar_beat_label(event.tick);
    ctx.event_bpm_input.value = event.type == timing_event_type::bpm ? TextFormat("%.6g", event.bpm) : "";
    ctx.numerator_input.value = event.type == timing_event_type::meter ? std::to_string(event.numerator) : "";
    ctx.denominator_input.value = event.type == timing_event_type::meter ? std::to_string(event.denominator) : "";
}

void add_event(context ctx, timing_event_type type) {
    if (!flush_selected_inputs(ctx)) {
        return;
    }
    ensure_initialized(ctx);
    timing_event event;
    event.type = type;
    event.tick = 1920;
    if (type == timing_event_type::bpm) {
        event.bpm = ctx.events.empty() ? 120.0f : std::max(1.0f, ctx.events.front().bpm);
    } else {
        event.numerator = 4;
        event.denominator = 4;
    }
    ctx.events.push_back(event);
    ctx.selected_event_index = ctx.events.size() - 1;
    ctx.event_scroll_offset = static_cast<float>(ctx.events.size()) * 34.0f;
    sync_selected_inputs(ctx);
}

void delete_selected_event(context ctx) {
    if (!flush_selected_inputs(ctx)) {
        return;
    }
    if (!ctx.selected_event_index.has_value() || *ctx.selected_event_index >= ctx.events.size()) {
        return;
    }
    const timing_event& selected = ctx.events[*ctx.selected_event_index];
    if (selected.tick == 0) {
        ctx.error = "The initial BPM/time signature cannot be deleted.";
        return;
    }
    ctx.events.erase(ctx.events.begin() + static_cast<std::ptrdiff_t>(*ctx.selected_event_index));
    if (ctx.events.empty()) {
        ctx.selected_event_index.reset();
    } else {
        ctx.selected_event_index = std::min(*ctx.selected_event_index, ctx.events.size() - 1);
    }
    ctx.error.clear();
    sync_selected_inputs(ctx);
}

bool apply_selected_event(context ctx) {
    if (!ctx.selected_event_index.has_value() || *ctx.selected_event_index >= ctx.events.size()) {
        ctx.error = "Select a timing event first.";
        return false;
    }

    timing_event updated = ctx.events[*ctx.selected_event_index];
    const std::optional<editor_meter_map::bar_beat_position> position =
        song_create::timing_service::parse_bar_beat_text(ctx.bar_input.value);
    if (!position.has_value()) {
        ctx.error = "Timing position must be in bar:beat format.";
        return false;
    }

    const editor_meter_map meter_map = song_create::timing_service::build_meter_map(ctx.events, 480);
    const std::optional<int> tick = meter_map.tick_from_bar_beat(position->measure, position->beat);
    if (!tick.has_value()) {
        ctx.error = "Timing position is outside the current time signature layout.";
        return false;
    }
    if (ctx.events[*ctx.selected_event_index].tick == 0 && *tick != 0) {
        ctx.error = "Initial BPM/time signature must stay at 1:1.";
        return false;
    }
    updated.tick = *tick;

    if (updated.type == timing_event_type::bpm) {
        float bpm = 0.0f;
        if (!parse_float_text(ctx.event_bpm_input.value, bpm) || bpm <= 0.0f) {
            ctx.error = "BPM must be greater than zero.";
            return false;
        }
        updated.bpm = bpm;
        if (updated.tick == 0) {
            ctx.bpm_input.value = TextFormat("%.6g", bpm);
        }
    } else {
        int numerator = 0;
        int denominator = 0;
        if (!parse_int_text(ctx.numerator_input.value, numerator) || numerator <= 0 ||
            !parse_int_text(ctx.denominator_input.value, denominator) || denominator <= 0) {
            ctx.error = "Time signature must use positive numbers.";
            return false;
        }
        updated.numerator = numerator;
        updated.denominator = denominator;
    }

    ctx.events[*ctx.selected_event_index] = updated;
    song_create::timing_service::sort_events(ctx.events);
    for (size_t index = 0; index < ctx.events.size(); ++index) {
        if (ctx.events[index].type == updated.type && ctx.events[index].tick == updated.tick) {
            ctx.selected_event_index = index;
            break;
        }
    }
    ctx.error.clear();
    sync_selected_inputs(ctx);
    return true;
}

bool flush_selected_inputs(context ctx) {
    const bool timing_input_active = ctx.bar_input.active || ctx.event_bpm_input.active ||
                                     ctx.numerator_input.active || ctx.denominator_input.active;
    if (!timing_input_active || !ctx.selected_event_index.has_value()) {
        return true;
    }
    return apply_selected_event(ctx);
}

bool import_midi_timing(context ctx, const std::string& midi_path) {
    if (!flush_selected_inputs(ctx)) {
        return false;
    }
    const song_create::midi_timing_import imported = song_create::import_midi_timing_file(midi_path);
    if (!imported.ok) {
        ctx.import_status = imported.message;
        ctx.error = imported.message;
        return false;
    }

    ctx.bpm_input.value = TextFormat("%.6g", imported.base_bpm);
    ctx.events = imported.events;
    for (timing_event& event : ctx.events) {
        event.tick = song_create::normalized_midi_tick(event.tick, imported.resolution);
    }
    song_create::timing_service::sort_events(ctx.events);
    ctx.events.erase(std::unique(ctx.events.begin(), ctx.events.end(),
                                 [](const timing_event& left, const timing_event& right) {
                                     return left.type == right.type && left.tick == right.tick;
                                 }),
                     ctx.events.end());
    ctx.selected_event_index = ctx.events.empty() ? std::nullopt : std::optional<size_t>(0);
    ensure_initialized(ctx);
    ctx.event_scroll_offset = 0.0f;
    ctx.import_status = imported.resolution == 480
        ? imported.message
        : TextFormat("%s %s %d -> 480",
                     imported.message.c_str(),
                     localization::tr_literal("Normalized PPQ:"),
                     imported.resolution);
    ctx.error.clear();
    return true;
}

std::vector<timing_event> validated_events(context ctx, float base_bpm, bool& ok) {
    ok = true;
    ensure_initialized(ctx);
    std::vector<timing_event> events =
        song_create::timing_service::validated_events(ctx.events, base_bpm, ok);
    if (!ok) {
        ctx.error = "Song timing contains an invalid BPM or time signature.";
        return {};
    }
    return events;
}

}  // namespace song_create::timing_controller
