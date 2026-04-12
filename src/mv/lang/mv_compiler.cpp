#include "mv_compiler.h"

#include <algorithm>
#include <array>
#include <sstream>

namespace mv {

namespace {

struct compiler_state {
    compiled_program output;
    std::vector<std::string> errors;
    bool had_error = false;

    // Current function being compiled (index into output.functions)
    int current_func_idx = -1;
    std::vector<std::pair<std::string, int>> locals; // name -> slot
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
        auto& consts = output.constants;
        for (int i = 0; i < static_cast<int>(consts.size()); i++) {
            if (consts[i] == val) return i;
        }
        consts.push_back(val);
        return static_cast<int>(consts.size()) - 1;
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
        for (int i = static_cast<int>(locals.size()) - 1; i >= 0; i--) {
            if (locals[i].first == name) return locals[i].second;
        }
        return -1;
    }

    int declare_local(const std::string& name) {
        int slot = static_cast<int>(locals.size());
        locals.push_back({name, slot});
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

    const expr* find_kwarg(const std::vector<keyword_arg>& kwargs, std::string_view name) const {
        for (const auto& kw : kwargs) {
            if (kw.name == name) return kw.value.get();
        }
        return nullptr;
    }

    bool kwargs_only_from(const std::vector<keyword_arg>& kwargs,
                          std::initializer_list<std::string_view> allowed) const {
        for (const auto& kw : kwargs) {
            bool ok = false;
            for (auto allowed_name : allowed) {
                if (kw.name == allowed_name) {
                    ok = true;
                    break;
                }
            }
            if (!ok) return false;
        }
        return true;
    }

    void compile_expr_or_none(const expr* value, int line) {
        if (value != nullptr) {
            compile_expr(*value);
        } else {
            emit(opcode::load_none, line);
        }
    }

    bool try_get_ctx_attr_slot(const expr& expression, int& ctx_slot_out, ctx_attr_slot& slot_out) const {
        std::vector<std::string_view> path;
        const expr* current = &expression;
        while (true) {
            if (const auto* attr = std::get_if<attr_expr>(&current->kind)) {
                path.push_back(attr->attr);
                current = attr->object.get();
                continue;
            }
            const auto* id = std::get_if<identifier>(&current->kind);
            if (id == nullptr || id->name != "ctx") {
                return false;
            }
            ctx_slot_out = resolve_local(id->name);
            if (ctx_slot_out < 0) return false;
            path.push_back("ctx");
            break;
        }

        std::reverse(path.begin(), path.end());

        auto match = [&](std::initializer_list<std::string_view> target, ctx_attr_slot slot) -> bool {
            if (path.size() != target.size()) return false;
            size_t i = 0;
            for (auto part : target) {
                if (path[i++] != part) return false;
            }
            slot_out = slot;
            return true;
        };

        return
            match({"ctx", "time"}, ctx_attr_slot::ctx_time) ||
            match({"ctx", "time", "ms"}, ctx_attr_slot::ctx_time_ms) ||
            match({"ctx", "time", "sec"}, ctx_attr_slot::ctx_time_sec) ||
            match({"ctx", "time", "length_ms"}, ctx_attr_slot::ctx_time_length_ms) ||
            match({"ctx", "time", "bpm"}, ctx_attr_slot::ctx_time_bpm) ||
            match({"ctx", "time", "beat"}, ctx_attr_slot::ctx_time_beat) ||
            match({"ctx", "time", "beat_phase"}, ctx_attr_slot::ctx_time_beat_phase) ||
            match({"ctx", "time", "meter_numerator"}, ctx_attr_slot::ctx_time_meter_numerator) ||
            match({"ctx", "time", "meter_denominator"}, ctx_attr_slot::ctx_time_meter_denominator) ||
            match({"ctx", "time", "progress"}, ctx_attr_slot::ctx_time_progress) ||
            match({"ctx", "audio"}, ctx_attr_slot::ctx_audio) ||
            match({"ctx", "audio", "analysis"}, ctx_attr_slot::ctx_audio_analysis) ||
            match({"ctx", "audio", "analysis", "level"}, ctx_attr_slot::ctx_audio_analysis_level) ||
            match({"ctx", "audio", "analysis", "rms"}, ctx_attr_slot::ctx_audio_analysis_rms) ||
            match({"ctx", "audio", "analysis", "peak"}, ctx_attr_slot::ctx_audio_analysis_peak) ||
            match({"ctx", "audio", "bands"}, ctx_attr_slot::ctx_audio_bands) ||
            match({"ctx", "audio", "bands", "low"}, ctx_attr_slot::ctx_audio_bands_low) ||
            match({"ctx", "audio", "bands", "mid"}, ctx_attr_slot::ctx_audio_bands_mid) ||
            match({"ctx", "audio", "bands", "high"}, ctx_attr_slot::ctx_audio_bands_high) ||
            match({"ctx", "audio", "buffers"}, ctx_attr_slot::ctx_audio_buffers) ||
            match({"ctx", "audio", "buffers", "spectrum"}, ctx_attr_slot::ctx_audio_buffers_spectrum) ||
            match({"ctx", "audio", "buffers", "spectrum_size"}, ctx_attr_slot::ctx_audio_buffers_spectrum_size) ||
            match({"ctx", "audio", "buffers", "waveform"}, ctx_attr_slot::ctx_audio_buffers_waveform) ||
            match({"ctx", "audio", "buffers", "waveform_size"}, ctx_attr_slot::ctx_audio_buffers_waveform_size) ||
            match({"ctx", "audio", "buffers", "waveform_index"}, ctx_attr_slot::ctx_audio_buffers_waveform_index) ||
            match({"ctx", "audio", "buffers", "oscilloscope"}, ctx_attr_slot::ctx_audio_buffers_oscilloscope) ||
            match({"ctx", "audio", "buffers", "oscilloscope_size"}, ctx_attr_slot::ctx_audio_buffers_oscilloscope_size) ||
            match({"ctx", "song"}, ctx_attr_slot::ctx_song) ||
            match({"ctx", "song", "song_id"}, ctx_attr_slot::ctx_song_song_id) ||
            match({"ctx", "song", "title"}, ctx_attr_slot::ctx_song_title) ||
            match({"ctx", "song", "artist"}, ctx_attr_slot::ctx_song_artist) ||
            match({"ctx", "song", "base_bpm"}, ctx_attr_slot::ctx_song_base_bpm) ||
            match({"ctx", "chart"}, ctx_attr_slot::ctx_chart) ||
            match({"ctx", "chart", "chart_id"}, ctx_attr_slot::ctx_chart_chart_id) ||
            match({"ctx", "chart", "song_id"}, ctx_attr_slot::ctx_chart_song_id) ||
            match({"ctx", "chart", "difficulty"}, ctx_attr_slot::ctx_chart_difficulty) ||
            match({"ctx", "chart", "level"}, ctx_attr_slot::ctx_chart_level) ||
            match({"ctx", "chart", "chart_author"}, ctx_attr_slot::ctx_chart_chart_author) ||
            match({"ctx", "chart", "resolution"}, ctx_attr_slot::ctx_chart_resolution) ||
            match({"ctx", "chart", "offset"}, ctx_attr_slot::ctx_chart_offset) ||
            match({"ctx", "chart", "total_notes"}, ctx_attr_slot::ctx_chart_total_notes) ||
            match({"ctx", "chart", "combo"}, ctx_attr_slot::ctx_chart_combo) ||
            match({"ctx", "chart", "accuracy"}, ctx_attr_slot::ctx_chart_accuracy) ||
            match({"ctx", "chart", "key_count"}, ctx_attr_slot::ctx_chart_key_count) ||
            match({"ctx", "screen"}, ctx_attr_slot::ctx_screen) ||
            match({"ctx", "screen", "w"}, ctx_attr_slot::ctx_screen_w) ||
            match({"ctx", "screen", "h"}, ctx_attr_slot::ctx_screen_h);
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
        if (!is_unshadowed_builtin_identifier(*node.callee, "Scene") ||
            !kwargs_only_from(node.keyword_args, {"nodes", "clear_color"}) ||
            node.positional_args.size() > 1) {
            return false;
        }
        const expr* nodes_expr = !node.positional_args.empty() ? node.positional_args[0].get() : find_kwarg(node.keyword_args, "nodes");
        compile_expr_or_none(nodes_expr, line);
        compile_expr_or_none(find_kwarg(node.keyword_args, "clear_color"), line);
        emit(opcode::make_scene, line);
        return true;
    }

    bool try_compile_fast_node_ctor(const call_with_kwargs_expr& node, int line) {
        const auto* callee = std::get_if<identifier>(&node.callee->kind);
        if (callee == nullptr || resolve_local(callee->name) >= 0) {
            return false;
        }

        if (callee->name == "Point" && kwargs_only_from(node.keyword_args, {"x", "y"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "x"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "y"), line);
            emit(opcode::make_point, line);
            return true;
        }
        if (callee->name == "DrawRect" && kwargs_only_from(node.keyword_args, {"x", "y", "w", "h", "rotation", "opacity", "fill"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "x"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "y"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "w"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "h"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "rotation"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "opacity"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "fill"), line);
            emit(opcode::make_draw_rect, line);
            return true;
        }
        if (callee->name == "DrawLine" && kwargs_only_from(node.keyword_args, {"x1", "y1", "x2", "y2", "thickness", "opacity", "stroke"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "x1"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "y1"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "x2"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "y2"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "thickness"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "opacity"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "stroke"), line);
            emit(opcode::make_draw_line, line);
            return true;
        }
        if (callee->name == "DrawText" && kwargs_only_from(node.keyword_args, {"text", "x", "y", "font_size", "opacity", "fill"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "text"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "x"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "y"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "font_size"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "opacity"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "fill"), line);
            emit(opcode::make_draw_text, line);
            return true;
        }
        if (callee->name == "DrawCircle" && kwargs_only_from(node.keyword_args, {"cx", "cy", "radius", "opacity", "fill"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "cx"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "cy"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "radius"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "opacity"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "fill"), line);
            emit(opcode::make_draw_circle, line);
            return true;
        }
        if (callee->name == "DrawPolyline" && kwargs_only_from(node.keyword_args, {"points", "thickness", "opacity", "stroke"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "points"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "thickness"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "opacity"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "stroke"), line);
            emit(opcode::make_draw_polyline, line);
            return true;
        }
        if (callee->name == "DrawBackground" && kwargs_only_from(node.keyword_args, {"fill", "opacity"}) &&
            node.positional_args.empty()) {
            compile_expr_or_none(find_kwarg(node.keyword_args, "fill"), line);
            compile_expr_or_none(find_kwarg(node.keyword_args, "opacity"), line);
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
                auto prev_locals = std::move(locals);
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
                locals.clear();
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
                locals = std::move(prev_locals);
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
