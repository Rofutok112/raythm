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

    int resolve_local(const std::string& name) {
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
            "Background", "Rect", "Circle", "Line", "Text", "Polyline",
            "SpectrumBar", "BeatGrid", "PulseRing"
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
                } else {
                    compile_expr(*node.callee);
                    for (auto& arg : node.args) {
                        compile_expr(*arg);
                    }
                    emit(opcode::call, static_cast<uint32_t>(node.args.size()), line);
                }
            }
            else if constexpr (std::is_same_v<T, call_with_kwargs_expr>) {
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
                    int scene_name_idx = add_string_constant("Scene");
                    emit(opcode::load_global, scene_name_idx, line);
                    emit(opcode::load_local, draw_nodes_slot, line);
                    emit(opcode::call, 1, line);
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
