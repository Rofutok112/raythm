#include "mv_sandbox.h"

#include <unordered_set>

#include "raylib.h"

namespace mv {

namespace {

struct validation_error {
    std::string message;
    int line = 0;
    int column = 0;
};

using name_set = std::unordered_set<std::string>;

void collect_function_names(const std::vector<stmt_ptr>& statements, name_set& names);

void collect_function_names_from_stmt(const stmt& statement, name_set& names) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, func_def>) {
            names.insert(node.name);
            collect_function_names(node.body, names);
        } else if constexpr (std::is_same_v<T, if_stmt>) {
            collect_function_names(node.main.body, names);
            for (const auto& branch : node.elifs) {
                collect_function_names(branch.body, names);
            }
            collect_function_names(node.else_body, names);
        } else if constexpr (std::is_same_v<T, for_stmt>) {
            collect_function_names(node.body, names);
        }
    }, statement.kind);
}

void collect_function_names(const std::vector<stmt_ptr>& statements, name_set& names) {
    for (const auto& statement : statements) {
        collect_function_names_from_stmt(*statement, names);
    }
}

void validate_expr(const expr& expression,
                   const name_set& callable_names,
                   const name_set& visible_names,
                   std::vector<validation_error>& errors);

void validate_expr_ptr(const expr_ptr& expression,
                       const name_set& callable_names,
                       const name_set& visible_names,
                       std::vector<validation_error>& errors) {
    if (expression) {
        validate_expr(*expression, callable_names, visible_names, errors);
    }
}

void validate_stmt(const stmt& statement,
                   const name_set& callable_names,
                   name_set& visible_names,
                   std::vector<validation_error>& errors);

void validate_block(const std::vector<stmt_ptr>& statements,
                    const name_set& callable_names,
                    name_set visible_names,
                    std::vector<validation_error>& errors) {
    for (const auto& statement : statements) {
        validate_stmt(*statement, callable_names, visible_names, errors);
    }
}

void validate_expr(const expr& expression,
                   const name_set& callable_names,
                   const name_set& visible_names,
                   std::vector<validation_error>& errors) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, binary_expr>) {
            validate_expr_ptr(node.left, callable_names, visible_names, errors);
            validate_expr_ptr(node.right, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, unary_expr>) {
            validate_expr_ptr(node.operand, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, call_expr>) {
            if (auto* ident = std::get_if<identifier>(&node.callee->kind)) {
                if (!callable_names.contains(ident->name) && !visible_names.contains(ident->name)) {
                    errors.push_back({"undefined function '" + ident->name + "'",
                                      expression.loc.line, expression.loc.column});
                }
            } else {
                validate_expr_ptr(node.callee, callable_names, visible_names, errors);
            }
            for (const auto& arg : node.args) {
                validate_expr_ptr(arg, callable_names, visible_names, errors);
            }
        } else if constexpr (std::is_same_v<T, call_with_kwargs_expr>) {
            if (auto* ident = std::get_if<identifier>(&node.callee->kind)) {
                if (!callable_names.contains(ident->name) && !visible_names.contains(ident->name)) {
                    errors.push_back({"undefined function '" + ident->name + "'",
                                      expression.loc.line, expression.loc.column});
                }
            } else {
                validate_expr_ptr(node.callee, callable_names, visible_names, errors);
            }
            for (const auto& arg : node.positional_args) {
                validate_expr_ptr(arg, callable_names, visible_names, errors);
            }
            for (const auto& kw : node.keyword_args) {
                validate_expr_ptr(kw.value, callable_names, visible_names, errors);
            }
        } else if constexpr (std::is_same_v<T, attr_expr>) {
            validate_expr_ptr(node.object, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, index_expr>) {
            validate_expr_ptr(node.object, callable_names, visible_names, errors);
            validate_expr_ptr(node.index, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, list_expr>) {
            for (const auto& item : node.elements) {
                validate_expr_ptr(item, callable_names, visible_names, errors);
            }
        }
    }, expression.kind);
}

void validate_stmt(const stmt& statement,
                   const name_set& callable_names,
                   name_set& visible_names,
                   std::vector<validation_error>& errors) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, expr_stmt>) {
            validate_expr_ptr(node.expression, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, assign_stmt>) {
            validate_expr_ptr(node.value, callable_names, visible_names, errors);
            visible_names.insert(node.name);
        } else if constexpr (std::is_same_v<T, attr_assign_stmt>) {
            validate_expr_ptr(node.object, callable_names, visible_names, errors);
            validate_expr_ptr(node.value, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, index_assign_stmt>) {
            validate_expr_ptr(node.object, callable_names, visible_names, errors);
            validate_expr_ptr(node.index, callable_names, visible_names, errors);
            validate_expr_ptr(node.value, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, return_stmt>) {
            if (node.value.has_value()) {
                validate_expr_ptr(*node.value, callable_names, visible_names, errors);
            }
        } else if constexpr (std::is_same_v<T, if_stmt>) {
            validate_expr_ptr(node.main.condition, callable_names, visible_names, errors);
            validate_block(node.main.body, callable_names, visible_names, errors);
            for (const auto& branch : node.elifs) {
                validate_expr_ptr(branch.condition, callable_names, visible_names, errors);
                validate_block(branch.body, callable_names, visible_names, errors);
            }
            validate_block(node.else_body, callable_names, visible_names, errors);
        } else if constexpr (std::is_same_v<T, for_stmt>) {
            validate_expr_ptr(node.iterable, callable_names, visible_names, errors);
            name_set loop_visible = visible_names;
            loop_visible.insert(node.var_name);
            validate_block(node.body, callable_names, loop_visible, errors);
            visible_names.insert(node.var_name);
        } else if constexpr (std::is_same_v<T, func_def>) {
            name_set function_visible = visible_names;
            for (const auto& param : node.params) {
                function_visible.insert(param);
            }
            validate_block(node.body, callable_names, function_visible, errors);
        } else if constexpr (std::is_same_v<T, augmented_assign_stmt>) {
            validate_expr_ptr(node.value, callable_names, visible_names, errors);
            visible_names.insert(node.name);
        }
    }, statement.kind);
}

std::vector<validation_error> validate_callable_names(const program& prog,
                                                      const std::vector<std::pair<std::string, mv_value>>& globals,
                                                      const std::vector<std::pair<std::string, native_function>>& natives,
                                                      const std::vector<std::pair<std::string, native_kwargs_function>>& natives_kwargs) {
    name_set callable_names;
    name_set visible_names;

    for (const auto& [name, _] : globals) {
        visible_names.insert(name);
        callable_names.insert(name);
    }
    for (const auto& [name, _] : natives) {
        callable_names.insert(name);
    }
    for (const auto& [name, _] : natives_kwargs) {
        callable_names.insert(name);
    }

    collect_function_names(prog.statements, callable_names);

    std::vector<validation_error> errors;
    for (const auto& statement : prog.statements) {
        validate_stmt(*statement, callable_names, visible_names, errors);
    }
    return errors;
}

} // namespace

void sandbox::set_limits(const sandbox_limits& limits) {
    limits_ = limits;
}

void sandbox::set_global(const std::string& name, mv_value value) {
    globals_.push_back({name, std::move(value)});
}

void sandbox::register_native(const std::string& name, native_function fn) {
    natives_.push_back({name, std::move(fn)});
}

void sandbox::register_native_kwargs(const std::string& name, native_kwargs_function fn) {
    natives_kwargs_.push_back({name, std::move(fn)});
}

bool sandbox::compile(const std::string& source) {
    errors_.clear();
    compiled_ = false;

    // Parse
    auto pr = parse(source);
    if (!pr.success) {
        for (auto& msg : pr.errors) {
            errors_.push_back({"parse", msg, 0, 0});
        }
        return false;
    }

    const auto validation_errors = validate_callable_names(pr.prog, globals_, natives_, natives_kwargs_);
    if (!validation_errors.empty()) {
        for (const auto& err : validation_errors) {
            errors_.push_back({"compile", err.message, err.line, err.column});
        }
        return false;
    }

    // Compile
    auto cr = mv::compile(pr.prog);
    if (!cr.success) {
        for (auto& msg : cr.errors) {
            errors_.push_back({"compile", msg, 0, 0});
        }
        return false;
    }

    compiled_prog_ = std::move(cr.program);
    compiled_ = true;
    return true;
}

script_result sandbox::call(const std::string& func_name, const std::vector<mv_value>& args) {
    static int call_count = 0;
    if (call_count < 3) {
        TraceLog(LOG_INFO, "MV SANDBOX: call('%s') compiled=%d funcs=%d consts=%d",
                 func_name.c_str(), compiled_ ? 1 : 0,
                 static_cast<int>(compiled_prog_.functions.size()),
                 static_cast<int>(compiled_prog_.constants.size()));
        auto fit = compiled_prog_.function_map.find(func_name);
        if (fit != compiled_prog_.function_map.end()) {
            const auto& func = compiled_prog_.functions[fit->second];
            TraceLog(LOG_INFO, "MV SANDBOX: func '%s' locals=%d params=%d instrs=%d",
                     func.name.c_str(), func.local_count, func.param_count,
                     static_cast<int>(func.code.size()));
            int limit = static_cast<int>(func.code.size());
            if (limit > 60) limit = 60;
            for (int i = 0; i < limit; i++) {
                const auto& ins = func.code[i];
                int op_int = static_cast<int>(ins.op);
                TraceLog(LOG_INFO, "MV SANDBOX:   [%03d] L%d op=%d arg=%u", i, ins.source_line, op_int, ins.arg);
            }
        } else {
            TraceLog(LOG_WARNING, "MV SANDBOX: func '%s' NOT FOUND in map", func_name.c_str());
        }
        call_count++;
    }

    if (!compiled_) {
        errors_ = {{"runtime", "no compiled script", 0, 0}};
        return {std::monostate{}, errors_, false};
    }

    vm v(compiled_prog_);
    v.set_limits(limits_);

    for (auto& [name, val] : globals_) {
        v.set_global(name, val);
    }
    for (auto& [name, fn] : natives_) {
        v.register_native(name, fn);
    }
    for (auto& [name, fn] : natives_kwargs_) {
        v.register_native_kwargs(name, fn);
    }

    auto result = v.call_function(func_name, args);
    if (!result.success) {
        errors_ = {{"runtime", result.error->message, result.error->line, 0}};
        return {std::monostate{}, errors_, false};
    }

    errors_.clear();
    return {std::move(result.value), {}, true};
}

script_result sandbox::run_draw(const std::string& source, mv_value ctx) {
    if (!compile(source)) {
        return {std::monostate{}, errors_, false};
    }

    vm v(compiled_prog_);
    v.set_limits(limits_);

    for (auto& [name, val] : globals_) {
        v.set_global(name, val);
    }
    for (auto& [name, fn] : natives_) {
        v.register_native(name, fn);
    }
    for (auto& [name, fn] : natives_kwargs_) {
        v.register_native_kwargs(name, fn);
    }

    auto result = v.call_function("draw", {std::move(ctx)});
    if (!result.success) {
        return {std::monostate{}, {{"runtime", result.error->message, result.error->line, 0}}, false};
    }

    return {std::move(result.value), {}, true};
}

} // namespace mv
