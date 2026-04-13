#include "mv_compiler.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <unordered_map>

namespace mv {

namespace {

struct constant_value_hash {
    std::size_t operator()(const constant_value& value) const {
        const std::size_t variant_index = value.index();
        return std::visit([variant_index](const auto& item) -> std::size_t {
            using T = std::decay_t<decltype(item)>;
            std::size_t seed = 0;
            if constexpr (std::is_same_v<T, double>) {
                seed = std::hash<double>{}(item);
            } else if constexpr (std::is_same_v<T, bool>) {
                seed = std::hash<bool>{}(item);
            } else {
                seed = std::hash<std::string>{}(item);
            }
            return seed ^ (variant_index + 0x9e3779b9u + (seed << 6) + (seed >> 2));
        }, value);
    }
};

struct kwarg_lookup {
    std::unordered_map<std::string_view, const expr*> values;

    const expr* get(std::string_view name) const {
        if (const auto it = values.find(name); it != values.end()) {
            return it->second;
        }
        return nullptr;
    }
};

struct compiler_state {
    compiled_program output;
    std::vector<std::string> errors;
    bool had_error = false;
    std::unordered_map<constant_value, int, constant_value_hash> constant_indices;

    // Current function being compiled (index into output.functions)
    int current_func_idx = -1;
    std::vector<std::string> local_names;
    std::unordered_map<std::string, std::vector<int>> local_slots;
    int scope_depth = 0;
    std::string current_func_name;
    int draw_nodes_slot = -1;

    function_chunk& current_chunk() { return output.functions[current_func_idx]; }

    void error(int line, const std::string& msg) {
        std::ostringstream oss;
        oss << "compile error line " << line << ": " << msg;
        errors.push_back(oss.str());
        had_error = true;
    }

    int add_constant(const constant_value& val) {
        if (const auto it = constant_indices.find(val); it != constant_indices.end()) {
            return it->second;
        }

        auto& consts = output.constants;
        consts.push_back(val);
        const int index = static_cast<int>(consts.size()) - 1;
        constant_indices.emplace(consts.back(), index);
        return index;
    }

    int add_string_constant(const std::string& s) {
        return add_constant(constant_value{s});
    }

    void emit(opcode op, uint32_t arg, int line) {
        current_chunk().code.push_back({op, arg, line});
    }

    void emit(opcode op, int line) {
        current_chunk().code.push_back({op, 0, line});
    }

    int emit_jump(opcode op, int line) {
        current_chunk().code.push_back({op, 0, line});
        return static_cast<int>(current_chunk().code.size()) - 1;
    }

    void patch_jump(int offset) {
        current_chunk().code[offset].arg = static_cast<uint32_t>(current_chunk().code.size());
    }

    int resolve_local(const std::string& name) const {
        if (const auto it = local_slots.find(name); it != local_slots.end() && !it->second.empty()) {
            return it->second.back();
        }
        return -1;
    }

    int declare_local(const std::string& name) {
        int slot = static_cast<int>(local_names.size());
        local_names.push_back(name);
        local_slots[name].push_back(slot);
        if (slot + 1 > current_chunk().local_count) {
            current_chunk().local_count = slot + 1;
        }
        return slot;
    }

    bool in_draw_function() const {
        return current_func_name == "draw" && draw_nodes_slot >= 0;
    }

    bool is_named_identifier(const expr& e, const std::string& name) const {
        if (auto* id = std::get_if<identifier>(&e.kind)) {
            return id->name == name;
        }
        return false;
    }

    bool is_named_call(const expr& e, const std::vector<std::string>& names) const {
        return std::visit([&](const auto& node) -> bool {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, call_expr>) {
                return std::any_of(names.begin(), names.end(), [&](const std::string& name) {
                    return is_named_identifier(*node.callee, name);
                });
            }
            else if constexpr (std::is_same_v<T, call_with_kwargs_expr>) {
                return std::any_of(names.begin(), names.end(), [&](const std::string& name) {
                    return is_named_identifier(*node.callee, name);
                });
            }
            return false;
        }, e.kind);
    }

    bool is_draw_node_expr(const expr& e) const {
        static const std::vector<std::string> kNodeNames = {
            "DrawBackground", "DrawRect", "DrawCircle", "DrawLine", "DrawText", "DrawPolyline"
        };
        return is_named_call(e, kNodeNames);
    }

    void compile_draw_node_append(const expr& e, int line) {
        emit(opcode::load_local, draw_nodes_slot, line);
        compile_expr(e);
        emit(opcode::append_list, line);
    }

    bool is_append_call(const call_expr& node) const {
        if (node.args.size() != 1) {
            return false;
        }
        const auto* attr = std::get_if<attr_expr>(&node.callee->kind);
        return attr != nullptr && attr->attr == "append";
    }

    void compile_append_call_expr(const call_expr& node, int line) {
        const auto* attr = std::get_if<attr_expr>(&node.callee->kind);
        compile_expr(*attr->object);
        compile_expr(*node.args[0]);
        emit(opcode::append_list, line);
        emit(opcode::load_none, line);
    }

    bool is_builtin_ctor_name(std::string_view name) const {
        return name == "Scene" || name == "Point" ||
               name == "DrawRect" || name == "DrawLine" || name == "DrawText" ||
               name == "DrawCircle" || name == "DrawPolyline" || name == "DrawBackground";
    }

    bool is_unshadowed_builtin_identifier(const expr& e, std::string_view name) const {
        const auto* id = std::get_if<identifier>(&e.kind);
        return id != nullptr && id->name == name && resolve_local(id->name) < 0;
    }

    std::optional<kwarg_lookup> build_kwarg_lookup(
        const std::vector<keyword_arg>& kwargs,
        std::initializer_list<std::string_view> allowed) const {
        kwarg_lookup lookup;
        lookup.values.reserve(kwargs.size());
        for (const auto& kw : kwargs) {
            bool ok = false;
            for (auto allowed_name : allowed) {
                if (kw.name == allowed_name) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                return std::nullopt;
            }
            lookup.values.emplace(kw.name, kw.value.get());
        }
        return lookup;
    }

    void compile_expr_or_none(const expr* value, int line) {
        if (value != nullptr) {
            compile_expr(*value);
        } else {
            emit(opcode::load_none, line);
        }
    }

    bool try_map_ctx_child_slot(ctx_attr_slot parent_slot, std::string_view attr, ctx_attr_slot& slot_out) const {
        switch (parent_slot) {
            case ctx_attr_slot::ctx_time:
                if (attr == "ms") { slot_out = ctx_attr_slot::ctx_time_ms; return true; }
                if (attr == "sec") { slot_out = ctx_attr_slot::ctx_time_sec; return true; }
                if (attr == "length_ms") { slot_out = ctx_attr_slot::ctx_time_length_ms; return true; }
                if (attr == "bpm") { slot_out = ctx_attr_slot::ctx_time_bpm; return true; }
                if (attr == "beat") { slot_out = ctx_attr_slot::ctx_time_beat; return true; }
                if (attr == "beat_phase") { slot_out = ctx_attr_slot::ctx_time_beat_phase; return true; }
                if (attr == "meter_numerator") { slot_out = ctx_attr_slot::ctx_time_meter_numerator; return true; }
                if (attr == "meter_denominator") { slot_out = ctx_attr_slot::ctx_time_meter_denominator; return true; }
                if (attr == "progress") { slot_out = ctx_attr_slot::ctx_time_progress; return true; }
                return false;
            case ctx_attr_slot::ctx_audio:
                if (attr == "analysis") { slot_out = ctx_attr_slot::ctx_audio_analysis; return true; }
                if (attr == "bands") { slot_out = ctx_attr_slot::ctx_audio_bands; return true; }
                if (attr == "buffers") { slot_out = ctx_attr_slot::ctx_audio_buffers; return true; }
                return false;
            case ctx_attr_slot::ctx_audio_analysis:
                if (attr == "level") { slot_out = ctx_attr_slot::ctx_audio_analysis_level; return true; }
                if (attr == "rms") { slot_out = ctx_attr_slot::ctx_audio_analysis_rms; return true; }
                if (attr == "peak") { slot_out = ctx_attr_slot::ctx_audio_analysis_peak; return true; }
                return false;
            case ctx_attr_slot::ctx_audio_bands:
                if (attr == "low") { slot_out = ctx_attr_slot::ctx_audio_bands_low; return true; }
                if (attr == "mid") { slot_out = ctx_attr_slot::ctx_audio_bands_mid; return true; }
                if (attr == "high") { slot_out = ctx_attr_slot::ctx_audio_bands_high; return true; }
                return false;
            case ctx_attr_slot::ctx_audio_buffers:
                if (attr == "spectrum") { slot_out = ctx_attr_slot::ctx_audio_buffers_spectrum; return true; }
                if (attr == "spectrum_size") { slot_out = ctx_attr_slot::ctx_audio_buffers_spectrum_size; return true; }
                if (attr == "waveform") { slot_out = ctx_attr_slot::ctx_audio_buffers_waveform; return true; }
                if (attr == "waveform_size") { slot_out = ctx_attr_slot::ctx_audio_buffers_waveform_size; return true; }
                if (attr == "waveform_index") { slot_out = ctx_attr_slot::ctx_audio_buffers_waveform_index; return true; }
                if (attr == "oscilloscope") { slot_out = ctx_attr_slot::ctx_audio_buffers_oscilloscope; return true; }
                if (attr == "oscilloscope_size") { slot_out = ctx_attr_slot::ctx_audio_buffers_oscilloscope_size; return true; }
                return false;
            case ctx_attr_slot::ctx_song:
                if (attr == "song_id") { slot_out = ctx_attr_slot::ctx_song_song_id; return true; }
                if (attr == "title") { slot_out = ctx_attr_slot::ctx_song_title; return true; }
                if (attr == "artist") { slot_out = ctx_attr_slot::ctx_song_artist; return true; }
                if (attr == "base_bpm") { slot_out = ctx_attr_slot::ctx_song_base_bpm; return true; }
                return false;
            case ctx_attr_slot::ctx_chart:
                if (attr == "chart_id") { slot_out = ctx_attr_slot::ctx_chart_chart_id; return true; }
                if (attr == "song_id") { slot_out = ctx_attr_slot::ctx_chart_song_id; return true; }
                if (attr == "difficulty") { slot_out = ctx_attr_slot::ctx_chart_difficulty; return true; }
                if (attr == "level") { slot_out = ctx_attr_slot::ctx_chart_level; return true; }
                if (attr == "chart_author") { slot_out = ctx_attr_slot::ctx_chart_chart_author; return true; }
                if (attr == "resolution") { slot_out = ctx_attr_slot::ctx_chart_resolution; return true; }
                if (attr == "offset") { slot_out = ctx_attr_slot::ctx_chart_offset; return true; }
                if (attr == "total_notes") { slot_out = ctx_attr_slot::ctx_chart_total_notes; return true; }
                if (attr == "combo") { slot_out = ctx_attr_slot::ctx_chart_combo; return true; }
                if (attr == "accuracy") { slot_out = ctx_attr_slot::ctx_chart_accuracy; return true; }
                if (attr == "key_count") { slot_out = ctx_attr_slot::ctx_chart_key_count; return true; }
                return false;
            case ctx_attr_slot::ctx_screen:
                if (attr == "w") { slot_out = ctx_attr_slot::ctx_screen_w; return true; }
                if (attr == "h") { slot_out = ctx_attr_slot::ctx_screen_h; return true; }
                return false;
            default:
                return false;
        }
    }

    bool try_get_ctx_attr_slot(const expr& expression, int& ctx_slot_out, ctx_attr_slot& slot_out) const {
        if (const auto* id = std::get_if<identifier>(&expression.kind)) {
            if (id->name != "ctx") {
                return false;
            }
            ctx_slot_out = resolve_local(id->name);
            if (ctx_slot_out < 0) {
                return false;
            }
            slot_out = ctx_attr_slot::ctx_time;
            return false;
        }

        const auto* attr = std::get_if<attr_expr>(&expression.kind);
        if (attr == nullptr) {
            return false;
        }

        ctx_attr_slot parent_slot = ctx_attr_slot::ctx_time;
        if (const auto* parent_id = std::get_if<identifier>(&attr->object->kind)) {
            if (parent_id->name != "ctx") {
                return false;
            }
            ctx_slot_out = resolve_local(parent_id->name);
            if (ctx_slot_out < 0) {
                return false;
            }
            if (attr->attr == "time") { slot_out = ctx_attr_slot::ctx_time; return true; }
            if (attr->attr == "audio") { slot_out = ctx_attr_slot::ctx_audio; return true; }
            if (attr->attr == "song") { slot_out = ctx_attr_slot::ctx_song; return true; }
            if (attr->attr == "chart") { slot_out = ctx_attr_slot::ctx_chart; return true; }
            if (attr->attr == "screen") { slot_out = ctx_attr_slot::ctx_screen; return true; }
            return false;
        }

        if (!try_get_ctx_attr_slot(*attr->object, ctx_slot_out, parent_slot)) {
            return false;
        }
        return try_map_ctx_child_slot(parent_slot, attr->attr, slot_out);
    }

    bool try_compile_fast_scene_call(const call_expr& node, int line) {
        if (!is_unshadowed_builtin_identifier(*node.callee, "Scene") || node.args.size() > 1) {
            return false;
        }
        compile_expr_or_none(node.args.empty() ? nullptr : node.args[0].get(), line);
        emit(opcode::load_none, line);
        emit(opcode::make_scene, line);
        return true;
    }

    bool try_compile_fast_scene_call(const call_with_kwargs_expr& node, int line) {
        const auto kwargs = build_kwarg_lookup(node.keyword_args, {"nodes", "clear_color"});
        if (!is_unshadowed_builtin_identifier(*node.callee, "Scene") || !kwargs.has_value() ||
            node.positional_args.size() > 1) {
            return false;
        }
        const expr* nodes_expr = !node.positional_args.empty() ? node.positional_args[0].get() : kwargs->get("nodes");
        compile_expr_or_none(nodes_expr, line);
        compile_expr_or_none(kwargs->get("clear_color"), line);
        emit(opcode::make_scene, line);
        return true;
    }

    bool try_compile_fast_node_ctor(const call_with_kwargs_expr& node, int line) {
        const auto* callee = std::get_if<identifier>(&node.callee->kind);
        if (callee == nullptr || resolve_local(callee->name) >= 0) {
            return false;
        }

        if (callee->name == "Point" &&
            node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"x", "y"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("x"), line);
            compile_expr_or_none(kwargs->get("y"), line);
            emit(opcode::make_point, line);
            return true;
        }
        if (callee->name == "DrawRect" && node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"x", "y", "w", "h", "rotation", "opacity", "fill"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("x"), line);
            compile_expr_or_none(kwargs->get("y"), line);
            compile_expr_or_none(kwargs->get("w"), line);
            compile_expr_or_none(kwargs->get("h"), line);
            compile_expr_or_none(kwargs->get("rotation"), line);
            compile_expr_or_none(kwargs->get("opacity"), line);
            compile_expr_or_none(kwargs->get("fill"), line);
            emit(opcode::make_draw_rect, line);
            return true;
        }
        if (callee->name == "DrawLine" && node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"x1", "y1", "x2", "y2", "thickness", "opacity", "stroke"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("x1"), line);
            compile_expr_or_none(kwargs->get("y1"), line);
            compile_expr_or_none(kwargs->get("x2"), line);
            compile_expr_or_none(kwargs->get("y2"), line);
            compile_expr_or_none(kwargs->get("thickness"), line);
            compile_expr_or_none(kwargs->get("opacity"), line);
            compile_expr_or_none(kwargs->get("stroke"), line);
            emit(opcode::make_draw_line, line);
            return true;
        }
        if (callee->name == "DrawText" && node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"text", "x", "y", "font_size", "opacity", "fill"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("text"), line);
            compile_expr_or_none(kwargs->get("x"), line);
            compile_expr_or_none(kwargs->get("y"), line);
            compile_expr_or_none(kwargs->get("font_size"), line);
            compile_expr_or_none(kwargs->get("opacity"), line);
            compile_expr_or_none(kwargs->get("fill"), line);
            emit(opcode::make_draw_text, line);
            return true;
        }
        if (callee->name == "DrawCircle" && node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"cx", "cy", "radius", "opacity", "fill"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("cx"), line);
            compile_expr_or_none(kwargs->get("cy"), line);
            compile_expr_or_none(kwargs->get("radius"), line);
            compile_expr_or_none(kwargs->get("opacity"), line);
            compile_expr_or_none(kwargs->get("fill"), line);
            emit(opcode::make_draw_circle, line);
            return true;
        }
        if (callee->name == "DrawPolyline" && node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"points", "thickness", "opacity", "stroke"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("points"), line);
            compile_expr_or_none(kwargs->get("thickness"), line);
            compile_expr_or_none(kwargs->get("opacity"), line);
            compile_expr_or_none(kwargs->get("stroke"), line);
            emit(opcode::make_draw_polyline, line);
            return true;
        }
        if (callee->name == "DrawBackground" && node.positional_args.empty()) {
            const auto kwargs = build_kwarg_lookup(node.keyword_args, {"fill", "opacity"});
            if (!kwargs.has_value()) {
                return false;
            }
            compile_expr_or_none(kwargs->get("fill"), line);
            compile_expr_or_none(kwargs->get("opacity"), line);
            emit(opcode::make_draw_background, line);
            return true;
        }
        return false;
    }

    bool try_compile_range_loop(const for_stmt& node, int line) {
        const auto* call = std::get_if<call_expr>(&node.iterable->kind);
        if (call == nullptr || !is_unshadowed_builtin_identifier(*call->callee, "range")) {
            return false;
        }

        int start_slot = declare_local("__range_start__");
        int end_slot = declare_local("__range_end__");
        int step_slot = declare_local("__range_step__");
        int current_slot = declare_local("__range_current__");
        int var_slot = resolve_local(node.var_name);
        if (var_slot < 0) var_slot = declare_local(node.var_name);

        int zero_const = add_constant(0.0);
        int one_const = add_constant(1.0);

        auto emit_number_const = [&](int const_idx) {
            emit(opcode::load_const, static_cast<uint32_t>(const_idx), line);
        };

        if (call->args.empty()) {
            emit_number_const(zero_const);
            emit(opcode::store_local, start_slot, line);
            emit_number_const(zero_const);
            emit(opcode::store_local, end_slot, line);
            emit_number_const(one_const);
            emit(opcode::store_local, step_slot, line);
        } else if (call->args.size() == 1) {
            emit_number_const(zero_const);
            emit(opcode::store_local, start_slot, line);
            compile_expr(*call->args[0]);
            emit(opcode::store_local, end_slot, line);
            emit_number_const(one_const);
            emit(opcode::store_local, step_slot, line);
        } else {
            compile_expr(*call->args[0]);
            emit(opcode::store_local, start_slot, line);
            compile_expr(*call->args[1]);
            emit(opcode::store_local, end_slot, line);
            if (call->args.size() >= 3) {
                compile_expr(*call->args[2]);
            } else {
                emit_number_const(one_const);
            }
            emit(opcode::store_local, step_slot, line);
        }

        emit(opcode::load_local, step_slot, line);
        emit_number_const(zero_const);
        emit(opcode::cmp_eq, line);
        int step_nonzero = emit_jump(opcode::jump_if_false, line);
        emit(opcode::pop, line);
        emit_number_const(one_const);
        emit(opcode::store_local, step_slot, line);
        int after_step_fix = emit_jump(opcode::jump, line);
        patch_jump(step_nonzero);
        emit(opcode::pop, line);
        patch_jump(after_step_fix);

        emit(opcode::load_local, start_slot, line);
        emit(opcode::store_local, current_slot, line);

        emit(opcode::load_local, step_slot, line);
        emit_number_const(zero_const);
        emit(opcode::cmp_gt, line);
        int negative_branch = emit_jump(opcode::jump_if_false, line);
        emit(opcode::pop, line);

        int positive_loop_start = static_cast<int>(current_chunk().code.size());
        emit(opcode::load_local, current_slot, line);
        emit(opcode::load_local, end_slot, line);
        emit(opcode::cmp_lt, line);
        int positive_exit = emit_jump(opcode::jump_if_false, line);
        emit(opcode::pop, line);
        emit(opcode::load_local, current_slot, line);
        emit(opcode::store_local, var_slot, line);
        for (auto& stmt : node.body) {
            compile_stmt(*stmt);
        }
        emit(opcode::load_local, current_slot, line);
        emit(opcode::load_local, step_slot, line);
        emit(opcode::add, line);
        emit(opcode::store_local, current_slot, line);
        emit(opcode::jump, static_cast<uint32_t>(positive_loop_start), line);

        patch_jump(negative_branch);
        emit(opcode::pop, line);

        int negative_loop_start = static_cast<int>(current_chunk().code.size());
        emit(opcode::load_local, current_slot, line);
        emit(opcode::load_local, end_slot, line);
        emit(opcode::cmp_gt, line);
        int negative_exit = emit_jump(opcode::jump_if_false, line);
        emit(opcode::pop, line);
        emit(opcode::load_local, current_slot, line);
        emit(opcode::store_local, var_slot, line);
        for (auto& stmt : node.body) {
            compile_stmt(*stmt);
        }
        emit(opcode::load_local, current_slot, line);
        emit(opcode::load_local, step_slot, line);
        emit(opcode::add, line);
        emit(opcode::store_local, current_slot, line);
        emit(opcode::jump, static_cast<uint32_t>(negative_loop_start), line);

        patch_jump(positive_exit);
        patch_jump(negative_exit);
        emit(opcode::pop, line);
        return true;
    }

    // ---- Compile expressions ----

    void compile_expr(const expr& e) {
        int line = e.loc.line;

        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, number_literal>) {
                int idx = add_constant(node.value);
                emit(opcode::load_const, idx, line);
            }
            else if constexpr (std::is_same_v<T, bool_literal>) {
                emit(node.value ? opcode::load_true : opcode::load_false, line);
            }
            else if constexpr (std::is_same_v<T, string_literal>) {
                int idx = add_constant(constant_value{node.value});
                emit(opcode::load_const, idx, line);
            }
            else if constexpr (std::is_same_v<T, none_literal>) {
                emit(opcode::load_none, line);
            }
            else if constexpr (std::is_same_v<T, identifier>) {
                int slot = resolve_local(node.name);
                if (slot >= 0) {
                    emit(opcode::load_local, slot, line);
                } else {
                    int name_idx = add_string_constant(node.name);
                    emit(opcode::load_global, name_idx, line);
                }
            }
            else if constexpr (std::is_same_v<T, binary_expr>) {
                // Short-circuit for and/or
                if (node.op == binary_op::logical_and) {
                    compile_expr(*node.left);
                    int jump = emit_jump(opcode::jump_if_false, line);
                    emit(opcode::pop, line);
                    compile_expr(*node.right);
                    patch_jump(jump);
                    return;
                }
                if (node.op == binary_op::logical_or) {
                    compile_expr(*node.left);
                    int jump = emit_jump(opcode::jump_if_true, line);
                    emit(opcode::pop, line);
                    compile_expr(*node.right);
                    patch_jump(jump);
                    return;
                }

                compile_expr(*node.left);
                compile_expr(*node.right);

                switch (node.op) {
                    case binary_op::add: emit(opcode::add, line); break;
                    case binary_op::sub: emit(opcode::sub, line); break;
                    case binary_op::mul: emit(opcode::mul, line); break;
                    case binary_op::div: emit(opcode::div_op, line); break;
                    case binary_op::mod: emit(opcode::mod, line); break;
                    case binary_op::power: emit(opcode::power, line); break;
                    case binary_op::eq: emit(opcode::cmp_eq, line); break;
                    case binary_op::ne: emit(opcode::cmp_ne, line); break;
                    case binary_op::lt: emit(opcode::cmp_lt, line); break;
                    case binary_op::gt: emit(opcode::cmp_gt, line); break;
                    case binary_op::le: emit(opcode::cmp_le, line); break;
                    case binary_op::ge: emit(opcode::cmp_ge, line); break;
                    default: break;
                }
            }
            else if constexpr (std::is_same_v<T, unary_expr>) {
                compile_expr(*node.operand);
                switch (node.op) {
                    case unary_op::negate: emit(opcode::negate, line); break;
                    case unary_op::logical_not: emit(opcode::logical_not, line); break;
                }
            }
            else if constexpr (std::is_same_v<T, call_expr>) {
                if (is_append_call(node)) {
                    compile_append_call_expr(node, line);
                } else if (try_compile_fast_scene_call(node, line)) {
                    return;
                } else {
                    compile_expr(*node.callee);
                    for (auto& arg : node.args) {
                        compile_expr(*arg);
                    }
                    emit(opcode::call, static_cast<uint32_t>(node.args.size()), line);
                }
            }
            else if constexpr (std::is_same_v<T, call_with_kwargs_expr>) {
                if (try_compile_fast_scene_call(node, line) || try_compile_fast_node_ctor(node, line)) {
                    return;
                }
                compile_expr(*node.callee);
                for (auto& arg : node.positional_args) {
                    compile_expr(*arg);
                }
                for (auto& kw : node.keyword_args) {
                    int name_idx = add_string_constant(kw.name);
                    emit(opcode::load_kwarg_name, name_idx, line);
                    compile_expr(*kw.value);
                }
                emit(opcode::call_kwargs, static_cast<uint32_t>(node.positional_args.size()), line);
                emit(opcode::kwarg_count, static_cast<uint32_t>(node.keyword_args.size()), line);
            }
            else if constexpr (std::is_same_v<T, attr_expr>) {
                int ctx_slot = -1;
                ctx_attr_slot ctx_slot_id = ctx_attr_slot::ctx_time;
                if (try_get_ctx_attr_slot(e, ctx_slot, ctx_slot_id)) {
                    emit(opcode::load_local, static_cast<uint32_t>(ctx_slot), line);
                    emit(opcode::load_ctx_attr, static_cast<uint32_t>(ctx_slot_id), line);
                    return;
                }
                compile_expr(*node.object);
                int name_idx = add_string_constant(node.attr);
                emit(opcode::load_attr, name_idx, line);
            }
            else if constexpr (std::is_same_v<T, index_expr>) {
                compile_expr(*node.object);
                compile_expr(*node.index);
                emit(opcode::load_index, line);
            }
            else if constexpr (std::is_same_v<T, list_expr>) {
                for (auto& elem : node.elements) {
                    compile_expr(*elem);
                }
                emit(opcode::build_list, static_cast<uint32_t>(node.elements.size()), line);
            }
        }, e.kind);
    }

    // ---- Compile statements ----

    void compile_stmt(const stmt& s) {
        int line = s.loc.line;

        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, expr_stmt>) {
                if (in_draw_function() && is_draw_node_expr(*node.expression)) {
                    compile_draw_node_append(*node.expression, line);
                } else {
                    compile_expr(*node.expression);
                    emit(opcode::pop, line);
                }
            }
            else if constexpr (std::is_same_v<T, assign_stmt>) {
                compile_expr(*node.value);
                int slot = resolve_local(node.name);
                if (slot < 0) {
                    slot = declare_local(node.name);
                }
                emit(opcode::store_local, slot, line);
            }
            else if constexpr (std::is_same_v<T, attr_assign_stmt>) {
                compile_expr(*node.object);
                compile_expr(*node.value);
                int name_idx = add_string_constant(node.attr);
                emit(opcode::store_attr, name_idx, line);
            }
            else if constexpr (std::is_same_v<T, index_assign_stmt>) {
                compile_expr(*node.object);
                compile_expr(*node.index);
                compile_expr(*node.value);
                emit(opcode::store_index, line);
            }
            else if constexpr (std::is_same_v<T, return_stmt>) {
                if (node.value.has_value()) {
                    compile_expr(*node.value.value());
                } else {
                    emit(opcode::load_none, line);
                }
                emit(opcode::return_op, line);
            }
            else if constexpr (std::is_same_v<T, if_stmt>) {
                compile_expr(*node.main.condition);
                int false_jump = emit_jump(opcode::jump_if_false, line);
                emit(opcode::pop, line);

                for (auto& stmt : node.main.body) {
                    compile_stmt(*stmt);
                }

                std::vector<int> end_jumps;
                end_jumps.push_back(emit_jump(opcode::jump, line));

                patch_jump(false_jump);
                emit(opcode::pop, line);

                for (auto& elif : node.elifs) {
                    compile_expr(*elif.condition);
                    int elif_false = emit_jump(opcode::jump_if_false, elif.condition->loc.line);
                    emit(opcode::pop, elif.condition->loc.line);

                    for (auto& stmt : elif.body) {
                        compile_stmt(*stmt);
                    }
                    end_jumps.push_back(emit_jump(opcode::jump, line));

                    patch_jump(elif_false);
                    emit(opcode::pop, line);
                }

                for (auto& stmt : node.else_body) {
                    compile_stmt(*stmt);
                }

                for (int j : end_jumps) {
                    patch_jump(j);
                }
            }
            else if constexpr (std::is_same_v<T, for_stmt>) {
                if (try_compile_range_loop(node, line)) {
                    return;
                }
                // Evaluate iterable
                compile_expr(*node.iterable);
                int list_slot = declare_local("__for_list__");
                emit(opcode::store_local, list_slot, line);

                // Iterator index = 0
                int idx_const = add_constant(0.0);
                emit(opcode::load_const, idx_const, line);
                int idx_slot = declare_local("__for_idx__");
                emit(opcode::store_local, idx_slot, line);

                // Loop variable
                int var_slot = resolve_local(node.var_name);
                if (var_slot < 0) var_slot = declare_local(node.var_name);

                // Loop start
                int loop_start = static_cast<int>(current_chunk().code.size());

                // load list, load idx, index -> if out of bounds VM returns None
                emit(opcode::load_local, list_slot, line);
                emit(opcode::load_local, idx_slot, line);
                emit(opcode::load_index, line);

                // If None (out of bounds), exit loop
                emit(opcode::load_none, line);
                emit(opcode::cmp_eq, line);
                int exit_jump = emit_jump(opcode::jump_if_true, line);
                emit(opcode::pop, line); // pop comparison result

                // Re-load the actual value
                emit(opcode::load_local, list_slot, line);
                emit(opcode::load_local, idx_slot, line);
                emit(opcode::load_index, line);
                emit(opcode::store_local, var_slot, line);

                // Body
                for (auto& stmt : node.body) {
                    compile_stmt(*stmt);
                }

                // Increment index
                emit(opcode::load_local, idx_slot, line);
                int one_const = add_constant(1.0);
                emit(opcode::load_const, one_const, line);
                emit(opcode::add, line);
                emit(opcode::store_local, idx_slot, line);

                // Jump back
                emit(opcode::jump, static_cast<uint32_t>(loop_start), line);
                patch_jump(exit_jump);
                emit(opcode::pop, line); // pop comparison result
            }
            else if constexpr (std::is_same_v<T, func_def>) {
                // Save state
                int prev_func_idx = current_func_idx;
                auto prev_local_names = std::move(local_names);
                auto prev_local_slots = std::move(local_slots);
                auto prev_depth = scope_depth;
                auto prev_func_name = current_func_name;
                int prev_draw_nodes_slot = draw_nodes_slot;

                // Create new function chunk
                int func_idx = static_cast<int>(output.functions.size());
                output.function_map[node.name] = func_idx;

                function_chunk chunk;
                chunk.name = node.name;
                chunk.param_count = static_cast<int>(node.params.size());
                output.functions.push_back(std::move(chunk));

                current_func_idx = func_idx;
                local_names.clear();
                local_slots.clear();
                scope_depth = 0;
                current_func_name = node.name;
                draw_nodes_slot = -1;

                // Declare params as locals
                for (auto& param : node.params) {
                    declare_local(param);
                }

                if (current_func_name == "draw") {
                    draw_nodes_slot = declare_local("__draw_nodes__");
                    emit(opcode::build_list, 0, line);
                    emit(opcode::store_local, draw_nodes_slot, line);
                }

                // Compile body
                for (auto& stmt : node.body) {
                    compile_stmt(*stmt);
                }

                // Implicit return Scene(collected_nodes) for draw(), otherwise None
                if (current_func_name == "draw") {
                    emit(opcode::load_local, draw_nodes_slot, line);
                    emit(opcode::load_none, line);
                    emit(opcode::make_scene, line);
                } else {
                    emit(opcode::load_none, line);
                }
                emit(opcode::return_op, line);

                // Restore state
                current_func_idx = prev_func_idx;
                local_names = std::move(prev_local_names);
                local_slots = std::move(prev_local_slots);
                scope_depth = prev_depth;
                current_func_name = std::move(prev_func_name);
                draw_nodes_slot = prev_draw_nodes_slot;
            }
            else if constexpr (std::is_same_v<T, augmented_assign_stmt>) {
                int slot = resolve_local(node.name);
                if (slot < 0) {
                    error(line, "variable '" + node.name + "' used before assignment");
                    return;
                }
                emit(opcode::load_local, slot, line);
                compile_expr(*node.value);
                switch (node.op) {
                    case binary_op::add: emit(opcode::add, line); break;
                    case binary_op::sub: emit(opcode::sub, line); break;
                    case binary_op::mul: emit(opcode::mul, line); break;
                    case binary_op::div: emit(opcode::div_op, line); break;
                    case binary_op::mod: emit(opcode::mod, line); break;
                    default: break;
                }
                emit(opcode::store_local, slot, line);
            }
        }, s.kind);
    }

    void compile_program(const program& ast) {
        // Create top-level chunk (__main__)
        function_chunk main_chunk;
        main_chunk.name = "__main__";
        output.functions.push_back(std::move(main_chunk));
        output.function_map["__main__"] = 0;
        current_func_idx = 0;

        for (auto& s : ast.statements) {
            compile_stmt(*s);
        }

        // Top-level implicit return None
        emit(opcode::load_none, 0);
        emit(opcode::return_op, 0);
    }
};

} // anonymous namespace

compile_result compile(const program& ast) {
    compiler_state state;
    state.compile_program(ast);
    return {std::move(state.output), std::move(state.errors), !state.had_error};
}

} // namespace mv
