#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mv {

enum class ctx_attr_slot : uint32_t {
    ctx_time = 0,
    ctx_time_ms,
    ctx_time_sec,
    ctx_time_length_ms,
    ctx_time_bpm,
    ctx_time_beat,
    ctx_time_beat_phase,
    ctx_time_meter_numerator,
    ctx_time_meter_denominator,
    ctx_time_progress,
    ctx_audio,
    ctx_audio_analysis,
    ctx_audio_analysis_level,
    ctx_audio_analysis_rms,
    ctx_audio_analysis_peak,
    ctx_audio_bands,
    ctx_audio_bands_low,
    ctx_audio_bands_mid,
    ctx_audio_bands_high,
    ctx_audio_buffers,
    ctx_audio_buffers_spectrum,
    ctx_audio_buffers_spectrum_size,
    ctx_audio_buffers_waveform,
    ctx_audio_buffers_waveform_size,
    ctx_audio_buffers_waveform_index,
    ctx_audio_buffers_oscilloscope,
    ctx_audio_buffers_oscilloscope_size,
    ctx_song,
    ctx_song_song_id,
    ctx_song_title,
    ctx_song_artist,
    ctx_song_base_bpm,
    ctx_chart,
    ctx_chart_chart_id,
    ctx_chart_song_id,
    ctx_chart_difficulty,
    ctx_chart_level,
    ctx_chart_chart_author,
    ctx_chart_resolution,
    ctx_chart_offset,
    ctx_chart_total_notes,
    ctx_chart_combo,
    ctx_chart_accuracy,
    ctx_chart_key_count,
    ctx_screen,
    ctx_screen_w,
    ctx_screen_h,
};

enum class opcode : uint8_t {
    // Stack
    load_const,       // arg: constant index
    load_none,
    load_true,
    load_false,
    pop,

    // Variables
    load_local,       // arg: local slot
    store_local,      // arg: local slot
    load_global,      // arg: name index in constant pool (string)
    store_global,     // arg: name index

    // Arithmetic
    add,
    sub,
    mul,
    div_op,
    mod,
    power,
    negate,

    // Comparison
    cmp_eq,
    cmp_ne,
    cmp_lt,
    cmp_gt,
    cmp_le,
    cmp_ge,

    // Logic
    logical_not,

    // Control flow
    jump,             // arg: absolute offset
    jump_if_false,    // arg: absolute offset
    jump_if_true,     // arg: absolute offset

    // Functions
    call,             // arg: arg count
    return_op,

    // Objects
    load_attr,        // arg: name index
    store_attr,       // arg: name index
    load_ctx_attr,    // arg: ctx_attr_slot

    // Lists
    build_list,       // arg: element count
    append_list,      // pops list and value, appends value in place, pushes None
    load_index,
    store_index,

    // Kwargs call
    call_kwargs,      // arg: positional count, followed by kwarg count instruction
    kwarg_count,      // arg: kwarg count (always follows call_kwargs)
    load_kwarg_name,  // arg: name index (emitted before each kwarg value)

    // Builtin constructor fast path
    make_scene,
    make_point,
    make_draw_rect,
    make_draw_line,
    make_draw_text,
    make_draw_circle,
    make_draw_polyline,
    make_draw_background,
};

struct instruction {
    opcode op;
    uint32_t arg = 0;
    int source_line = 0;
};

using constant_value = std::variant<double, bool, std::string>;

struct function_chunk {
    std::string name;
    int param_count = 0;
    int local_count = 0;
    std::vector<instruction> code;
};

struct compiled_program {
    std::vector<constant_value> constants;
    std::vector<function_chunk> functions;      // index 0 = top-level (implicit __main__)
    std::unordered_map<std::string, int> function_map; // name → function index
};

} // namespace mv
