#include "mv_sandbox.h"

#include <cctype>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <unordered_map>

#include "raylib.h"

namespace mv {

namespace {

struct validation_error {
    std::string message;
    int line = 0;
    int column = 0;
};

using name_set = std::unordered_set<std::string>;
using type_map = std::unordered_map<std::string, std::string>;

using member_set = std::unordered_set<std::string_view>;

const std::unordered_map<std::string_view, member_set>& ctx_member_schema() {
    static const std::unordered_map<std::string_view, member_set> schema = {
        {"ctx", {"time", "audio", "song", "chart", "screen"}},
        {"ctx.time", {"ms", "sec", "length_ms", "bpm", "beat", "beat_phase",
                      "meter_numerator", "meter_denominator", "progress"}},
        {"ctx.audio", {"analysis", "bands", "buffers"}},
        {"ctx.audio.analysis", {"level", "rms", "peak"}},
        {"ctx.audio.bands", {"low", "mid", "high"}},
        {"ctx.audio.buffers", {"spectrum", "spectrum_size", "oscilloscope", "oscilloscope_size",
                               "waveform", "waveform_size", "waveform_index"}},
        {"ctx.song", {"song_id", "title", "artist", "base_bpm"}},
        {"ctx.chart", {"chart_id", "song_id", "difficulty", "level", "chart_author",
                       "resolution", "offset", "total_notes", "combo", "accuracy", "key_count"}},
        {"ctx.screen", {"w", "h"}},
    };
    return schema;
}

bool is_known_name(const std::string& name,
                   const name_set& callable_names,
                   const name_set& visible_names) {
    return callable_names.contains(name) || visible_names.contains(name);
}

const std::unordered_map<std::string_view, member_set>& builtin_object_schema() {
    static const std::unordered_map<std::string_view, member_set> schema = {
        {"Scene", {"nodes", "clear_color"}},
        {"Point", {"x", "y"}},
        {"DrawRect", {"x", "y", "w", "h", "rotation", "opacity", "fill"}},
        {"DrawLine", {"x1", "y1", "x2", "y2", "thickness", "opacity", "stroke"}},
        {"DrawText", {"text", "x", "y", "font_size", "opacity", "fill"}},
        {"DrawCircle", {"cx", "cy", "radius", "opacity", "fill"}},
        {"DrawPolyline", {"points", "thickness", "opacity", "stroke"}},
        {"DrawBackground", {"fill", "opacity"}},
        {"Color", {"r", "g", "b", "a"}},
        {"list", {"append"}},
    };
    return schema;
}

std::optional<std::string> infer_expr_type(const expr& expression, const type_map& visible_types);

std::optional<std::string> ctx_object_path(const expr& expression) {
    if (const auto* ident = std::get_if<identifier>(&expression.kind)) {
        if (ident->name == "ctx") {
            return ident->name;
        }
        return std::nullopt;
    }
    if (const auto* attr = std::get_if<attr_expr>(&expression.kind)) {
        const auto base = ctx_object_path(*attr->object);
        if (!base.has_value()) {
            return std::nullopt;
        }
        return *base + "." + attr->attr;
    }
    return std::nullopt;
}

std::optional<std::string> infer_ctx_attr_type(std::string_view base, std::string_view attr) {
    const std::string full = std::string(base) + "." + std::string(attr);
    if (ctx_member_schema().contains(full)) {
        return full;
    }
    if (full == "ctx.audio.buffers.spectrum" || full == "ctx.audio.buffers.waveform" ||
        full == "ctx.audio.buffers.oscilloscope") {
        return std::string("list");
    }
    return std::nullopt;
}

void validate_ctx_attr(const expr& object_expr,
                       std::string_view attr_name,
                       source_location loc,
                       std::vector<validation_error>& errors) {
    const auto base = ctx_object_path(object_expr);
    if (!base.has_value()) {
        return;
    }

    const auto& schema = ctx_member_schema();
    const auto schema_it = schema.find(*base);
    if (schema_it == schema.end()) {
        return;
    }
    if (!schema_it->second.contains(attr_name)) {
        errors.push_back({"unknown attribute '" + std::string(attr_name) + "' on " + *base,
                          loc.line, loc.column});
    }
}

void validate_object_attr(const expr& object_expr,
                          std::string_view attr_name,
                          source_location loc,
                          const type_map& visible_types,
                          std::vector<validation_error>& errors) {
    if (const auto base = ctx_object_path(object_expr); base.has_value()) {
        validate_ctx_attr(object_expr, attr_name, loc, errors);
        return;
    }

    const auto object_type = infer_expr_type(object_expr, visible_types);
    if (!object_type.has_value()) {
        return;
    }

    const auto& schema = builtin_object_schema();
    const auto schema_it = schema.find(*object_type);
    if (schema_it == schema.end()) {
        return;
    }
    if (!schema_it->second.contains(attr_name)) {
        errors.push_back({"unknown attribute '" + std::string(attr_name) + "' on " + *object_type,
                          loc.line, loc.column});
    }
}

std::optional<std::string> infer_expr_type(const expr& expression, const type_map& visible_types) {
    return std::visit([&](const auto& node) -> std::optional<std::string> {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, identifier>) {
            auto it = visible_types.find(node.name);
            if (it != visible_types.end()) {
                return it->second;
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, list_expr>) {
            return "list";
        } else if constexpr (std::is_same_v<T, attr_expr>) {
            if (const auto base = ctx_object_path(*node.object); base.has_value()) {
                return infer_ctx_attr_type(*base, node.attr);
            }
            const auto object_type = infer_expr_type(*node.object, visible_types);
            if (!object_type.has_value()) {
                return std::nullopt;
            }
            if (*object_type == "Scene" && node.attr == "nodes") return std::string("list");
            if (*object_type == "DrawRect" && node.attr == "fill") return std::string("Color");
            if (*object_type == "DrawLine" && node.attr == "stroke") return std::string("Color");
            if (*object_type == "DrawText" && node.attr == "fill") return std::string("Color");
            if (*object_type == "DrawCircle" && node.attr == "fill") return std::string("Color");
            if (*object_type == "DrawPolyline" && node.attr == "points") return std::string("list");
            if (*object_type == "DrawPolyline" && node.attr == "stroke") return std::string("Color");
            if (*object_type == "DrawBackground" && node.attr == "fill") return std::string("Color");
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, call_expr>) {
            if (const auto* ident = std::get_if<identifier>(&node.callee->kind)) {
                if (ident->name == "range") return std::string("list");
                if (ident->name == "rgb") return std::string("Color");
            }
            if (const auto* attr = std::get_if<attr_expr>(&node.callee->kind)) {
                if (const auto object_type = infer_expr_type(*attr->object, visible_types);
                    object_type.has_value() && *object_type == "list" && attr->attr == "append") {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, call_with_kwargs_expr>) {
            if (const auto* ident = std::get_if<identifier>(&node.callee->kind)) {
                const std::string_view name = ident->name;
                if (builtin_object_schema().contains(name) && name != "list") {
                    return ident->name;
                }
            }
            return std::nullopt;
        }
        return std::nullopt;
    }, expression.kind);
}

std::optional<script_error> parse_error_message(const std::string& phase, const std::string& message) {
    auto skip_spaces = [&](size_t& i) {
        while (i < message.size() && std::isspace(static_cast<unsigned char>(message[i]))) {
            ++i;
        }
    };

    if (message.rfind("line ", 0) == 0) {
        size_t i = 5;
        int line = 0;
        while (i < message.size() && std::isdigit(static_cast<unsigned char>(message[i]))) {
            line = line * 10 + (message[i] - '0');
            ++i;
        }

        int column = 0;
        if (i < message.size() && message[i] == ':') {
            ++i;
            while (i < message.size() && std::isdigit(static_cast<unsigned char>(message[i]))) {
                column = column * 10 + (message[i] - '0');
                ++i;
            }
        }

        if (i < message.size() && message[i] == ':') {
            ++i;
        }
        skip_spaces(i);
        return script_error{phase, message.substr(i), line, column};
    }

    if (message.rfind("compile error line ", 0) == 0) {
        size_t i = 19;
        int line = 0;
        while (i < message.size() && std::isdigit(static_cast<unsigned char>(message[i]))) {
            line = line * 10 + (message[i] - '0');
            ++i;
        }
        if (i < message.size() && message[i] == ':') {
            ++i;
        }
        skip_spaces(i);
        return script_error{phase, message.substr(i), line, 0};
    }

    return std::nullopt;
}

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
                   const type_map& visible_types,
                   std::vector<validation_error>& errors);

void validate_expr_ptr(const expr_ptr& expression,
                       const name_set& callable_names,
                       const name_set& visible_names,
                       const type_map& visible_types,
                       std::vector<validation_error>& errors) {
    if (expression) {
        validate_expr(*expression, callable_names, visible_names, visible_types, errors);
    }
}

void validate_stmt(const stmt& statement,
                   const name_set& callable_names,
                   name_set& visible_names,
                   type_map& visible_types,
                   std::vector<validation_error>& errors);

void validate_block(const std::vector<stmt_ptr>& statements,
                    const name_set& callable_names,
                    name_set visible_names,
                    type_map visible_types,
                    std::vector<validation_error>& errors) {
    for (const auto& statement : statements) {
        validate_stmt(*statement, callable_names, visible_names, visible_types, errors);
    }
}

void validate_expr(const expr& expression,
                   const name_set& callable_names,
                   const name_set& visible_names,
                   const type_map& visible_types,
                   std::vector<validation_error>& errors) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, identifier>) {
            if (!is_known_name(node.name, callable_names, visible_names)) {
                errors.push_back({"undefined variable '" + node.name + "'",
                                  expression.loc.line, expression.loc.column});
            }
        } else
        if constexpr (std::is_same_v<T, binary_expr>) {
            validate_expr_ptr(node.left, callable_names, visible_names, visible_types, errors);
            validate_expr_ptr(node.right, callable_names, visible_names, visible_types, errors);
        } else if constexpr (std::is_same_v<T, unary_expr>) {
            validate_expr_ptr(node.operand, callable_names, visible_names, visible_types, errors);
        } else if constexpr (std::is_same_v<T, call_expr>) {
            if (auto* ident = std::get_if<identifier>(&node.callee->kind)) {
                if (!is_known_name(ident->name, callable_names, visible_names)) {
                    errors.push_back({"undefined function '" + ident->name + "'",
                                      expression.loc.line, expression.loc.column});
                }
            } else {
                validate_expr_ptr(node.callee, callable_names, visible_names, visible_types, errors);
            }
            for (const auto& arg : node.args) {
                validate_expr_ptr(arg, callable_names, visible_names, visible_types, errors);
            }
        } else if constexpr (std::is_same_v<T, call_with_kwargs_expr>) {
            if (auto* ident = std::get_if<identifier>(&node.callee->kind)) {
                if (!is_known_name(ident->name, callable_names, visible_names)) {
                    errors.push_back({"undefined function '" + ident->name + "'",
                                      expression.loc.line, expression.loc.column});
                }
            } else {
                validate_expr_ptr(node.callee, callable_names, visible_names, visible_types, errors);
            }
            for (const auto& arg : node.positional_args) {
                validate_expr_ptr(arg, callable_names, visible_names, visible_types, errors);
            }
            for (const auto& kw : node.keyword_args) {
                validate_expr_ptr(kw.value, callable_names, visible_names, visible_types, errors);
            }
        } else if constexpr (std::is_same_v<T, attr_expr>) {
            validate_expr_ptr(node.object, callable_names, visible_names, visible_types, errors);
            validate_object_attr(*node.object, node.attr, expression.loc, visible_types, errors);
        } else if constexpr (std::is_same_v<T, index_expr>) {
            validate_expr_ptr(node.object, callable_names, visible_names, visible_types, errors);
            validate_expr_ptr(node.index, callable_names, visible_names, visible_types, errors);
        } else if constexpr (std::is_same_v<T, list_expr>) {
            for (const auto& item : node.elements) {
                validate_expr_ptr(item, callable_names, visible_names, visible_types, errors);
            }
        }
    }, expression.kind);
}

void validate_stmt(const stmt& statement,
                   const name_set& callable_names,
                   name_set& visible_names,
                   type_map& visible_types,
                   std::vector<validation_error>& errors) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, expr_stmt>) {
            validate_expr_ptr(node.expression, callable_names, visible_names, visible_types, errors);
        } else if constexpr (std::is_same_v<T, assign_stmt>) {
            validate_expr_ptr(node.value, callable_names, visible_names, visible_types, errors);
            visible_names.insert(node.name);
            if (const auto inferred = infer_expr_type(*node.value, visible_types); inferred.has_value()) {
                visible_types[node.name] = *inferred;
            } else {
                visible_types.erase(node.name);
            }
        } else if constexpr (std::is_same_v<T, attr_assign_stmt>) {
            validate_expr_ptr(node.object, callable_names, visible_names, visible_types, errors);
            validate_expr_ptr(node.value, callable_names, visible_names, visible_types, errors);
            validate_object_attr(*node.object, node.attr, statement.loc, visible_types, errors);
        } else if constexpr (std::is_same_v<T, index_assign_stmt>) {
            validate_expr_ptr(node.object, callable_names, visible_names, visible_types, errors);
            validate_expr_ptr(node.index, callable_names, visible_names, visible_types, errors);
            validate_expr_ptr(node.value, callable_names, visible_names, visible_types, errors);
        } else if constexpr (std::is_same_v<T, return_stmt>) {
            if (node.value.has_value()) {
                validate_expr_ptr(*node.value, callable_names, visible_names, visible_types, errors);
            }
        } else if constexpr (std::is_same_v<T, if_stmt>) {
            validate_expr_ptr(node.main.condition, callable_names, visible_names, visible_types, errors);
            validate_block(node.main.body, callable_names, visible_names, visible_types, errors);
            for (const auto& branch : node.elifs) {
                validate_expr_ptr(branch.condition, callable_names, visible_names, visible_types, errors);
                validate_block(branch.body, callable_names, visible_names, visible_types, errors);
            }
            validate_block(node.else_body, callable_names, visible_names, visible_types, errors);
        } else if constexpr (std::is_same_v<T, for_stmt>) {
            validate_expr_ptr(node.iterable, callable_names, visible_names, visible_types, errors);
            name_set loop_visible = visible_names;
            type_map loop_types = visible_types;
            loop_visible.insert(node.var_name);
            loop_types.erase(node.var_name);
            validate_block(node.body, callable_names, loop_visible, loop_types, errors);
            visible_names.insert(node.var_name);
            visible_types.erase(node.var_name);
        } else if constexpr (std::is_same_v<T, func_def>) {
            name_set function_visible = visible_names;
            type_map function_types = visible_types;
            for (const auto& param : node.params) {
                function_visible.insert(param);
                function_types.erase(param);
            }
            validate_block(node.body, callable_names, function_visible, function_types, errors);
        } else if constexpr (std::is_same_v<T, augmented_assign_stmt>) {
            validate_expr_ptr(node.value, callable_names, visible_names, visible_types, errors);
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
    type_map visible_types;

    for (const auto& [name, _] : globals) {
        visible_names.insert(name);
        callable_names.insert(name);
        if (name == "ctx") {
            visible_types.emplace(name, "ctx");
        }
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
        validate_stmt(*statement, callable_names, visible_names, visible_types, errors);
    }
    return errors;
}

} // namespace

void sandbox::set_limits(const sandbox_limits& limits) {
    limits_ = limits;
    if (runtime_vm_) {
        runtime_vm_->set_limits(limits_);
    }
}

void sandbox::set_global(const std::string& name, mv_value value) {
    globals_.push_back({name, std::move(value)});
    if (runtime_vm_) {
        runtime_vm_->set_global(globals_.back().first, globals_.back().second);
    }
}

void sandbox::register_native(const std::string& name, native_function fn) {
    natives_.push_back({name, std::move(fn)});
    if (runtime_vm_) {
        runtime_vm_->register_native(natives_.back().first, natives_.back().second);
    }
}

void sandbox::register_native_kwargs(const std::string& name, native_kwargs_function fn) {
    natives_kwargs_.push_back({name, std::move(fn)});
    if (runtime_vm_) {
        runtime_vm_->register_native_kwargs(natives_kwargs_.back().first, natives_kwargs_.back().second);
    }
}

bool sandbox::compile(const std::string& source) {
    errors_.clear();
    compiled_ = false;
    runtime_vm_.reset();

    // Parse
    auto pr = parse(source);
    if (!pr.success) {
        for (auto& msg : pr.errors) {
            if (auto parsed = parse_error_message("parse", msg)) {
                errors_.push_back(*parsed);
            } else {
                errors_.push_back({"parse", msg, 0, 0});
            }
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
            if (auto parsed = parse_error_message("compile", msg)) {
                errors_.push_back(*parsed);
            } else {
                errors_.push_back({"compile", msg, 0, 0});
            }
        }
        return false;
    }

    compiled_prog_ = std::move(cr.program);
    runtime_vm_ = std::make_unique<vm>(compiled_prog_);
    runtime_vm_->set_limits(limits_);
    for (auto& [name, val] : globals_) {
        runtime_vm_->set_global(name, val);
    }
    for (auto& [name, fn] : natives_) {
        runtime_vm_->register_native(name, fn);
    }
    for (auto& [name, fn] : natives_kwargs_) {
        runtime_vm_->register_native_kwargs(name, fn);
    }

    auto init_result = runtime_vm_->run_top_level();
    if (!init_result.success) {
        errors_.push_back({"runtime", init_result.error->message, init_result.error->line, 0});
        runtime_vm_.reset();
        return false;
    }
    compiled_ = true;
    return true;
}

script_result sandbox::call(const std::string& func_name, const std::vector<mv_value>& args) {
    if (!compiled_ || !runtime_vm_) {
        errors_ = {{"runtime", "no compiled script", 0, 0}};
        return {std::monostate{}, errors_, false};
    }

    auto result = runtime_vm_->call_function(func_name, args);
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
    return call("draw", {std::move(ctx)});
}

} // namespace mv
