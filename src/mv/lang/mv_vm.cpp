#include "mv_vm.h"

#include <cmath>
#include <sstream>

namespace mv {

namespace {

std::optional<vec2> cached_point_from_value(const mv_value& value) {
    if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&value)) {
        if (*obj && (*obj)->cached_point.has_value()) {
            return (*obj)->cached_point;
        }
    }
    return std::nullopt;
}

std::optional<scene_node> cached_scene_node_from_value(const mv_value& value) {
    if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&value)) {
        if (*obj && (*obj)->cached_scene_node.has_value()) {
            return (*obj)->cached_scene_node;
        }
    }
    return std::nullopt;
}

void try_build_list_render_caches(mv_list& list) {
    if (list.elements.empty()) {
        list.cached_points = std::vector<vec2>{};
        list.cached_scene_nodes = std::vector<scene_node>{};
        return;
    }

    bool all_points = true;
    bool all_scene_nodes = true;
    std::vector<vec2> points;
    std::vector<scene_node> scene_nodes;
    points.reserve(list.elements.size());
    scene_nodes.reserve(list.elements.size());

    for (const auto& element : list.elements) {
        if (all_points) {
            if (auto point = cached_point_from_value(element)) {
                points.push_back(*point);
            } else {
                all_points = false;
                points.clear();
            }
        }
        if (all_scene_nodes) {
            if (auto node = cached_scene_node_from_value(element)) {
                scene_nodes.push_back(*node);
            } else {
                all_scene_nodes = false;
                scene_nodes.clear();
            }
        }
        if (!all_points && !all_scene_nodes) {
            break;
        }
    }

    list.cached_points = all_points ? std::optional<std::vector<vec2>>(std::move(points)) : std::nullopt;
    list.cached_scene_nodes = all_scene_nodes
        ? std::optional<std::vector<scene_node>>(std::move(scene_nodes))
        : std::nullopt;
}

void update_list_caches_for_append(mv_list& list, const mv_value& appended) {
    if (list.cached_points.has_value()) {
        if (auto point = cached_point_from_value(appended)) {
            list.cached_points->push_back(*point);
        } else {
            list.cached_points.reset();
        }
    } else if (list.elements.size() == 1) {
        if (auto point = cached_point_from_value(appended)) {
            list.cached_points = std::vector<vec2>{*point};
        }
    }

    if (list.cached_scene_nodes.has_value()) {
        if (auto node = cached_scene_node_from_value(appended)) {
            list.cached_scene_nodes->push_back(*node);
        } else {
            list.cached_scene_nodes.reset();
        }
    } else if (list.elements.size() == 1) {
        if (auto node = cached_scene_node_from_value(appended)) {
            list.cached_scene_nodes = std::vector<scene_node>{*node};
        }
    }
}

} // namespace

// ---- Helpers ----

bool is_truthy(const mv_value& val) {
    return std::visit([](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) return v != 0.0;
        else if constexpr (std::is_same_v<T, bool>) return v;
        else if constexpr (std::is_same_v<T, std::string>) return !v.empty();
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_list>>) return v && !v->elements.empty();
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_object>>) return v != nullptr;
        else if constexpr (std::is_same_v<T, native_ref>) return v.index >= 0;
        else if constexpr (std::is_same_v<T, function_ref>) return v.index >= 0;
        else if constexpr (std::is_same_v<T, std::monostate>) return false;
        else return false;
    }, val);
}

std::string value_type_name(const mv_value& val) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) return "number";
        else if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_list>>) return "list";
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_object>>) return v ? v->type_name : "object";
        else if constexpr (std::is_same_v<T, native_ref>) return "native_function";
        else if constexpr (std::is_same_v<T, function_ref>) return "function";
        else if constexpr (std::is_same_v<T, std::monostate>) return "None";
        else return "unknown";
    }, val);
}

std::string value_to_string(const mv_value& val) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }
        else if constexpr (std::is_same_v<T, bool>) return v ? "True" : "False";
        else if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_list>>) {
            std::string s = "[";
            if (v) {
                for (size_t i = 0; i < v->elements.size(); i++) {
                    if (i > 0) s += ", ";
                    s += value_to_string(v->elements[i]);
                }
            }
            return s + "]";
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_object>>) {
            return v ? "<" + v->type_name + ">" : "<null>";
        }
        else if constexpr (std::is_same_v<T, native_ref>) return "<native>";
        else if constexpr (std::is_same_v<T, function_ref>) return "<function>";
        else if constexpr (std::is_same_v<T, std::monostate>) return "None";
        else return "?";
    }, val);
}

// ---- VM implementation ----

vm::vm(compiled_program prog) : program_(std::move(prog)) {
    stack_.reserve(256);
    frames_.reserve(32);
}

void vm::set_global(const std::string& name, mv_value value) {
    globals_[name] = std::move(value);
}

void vm::register_native(const std::string& name, native_function fn) {
    int index = 0;
    auto it = native_index_by_name_.find(name);
    if (it == native_index_by_name_.end()) {
        index = static_cast<int>(native_table_.size());
        native_index_by_name_[name] = index;
        native_table_.push_back(std::move(fn));
    } else {
        index = it->second;
        native_table_[static_cast<size_t>(index)] = std::move(fn);
    }
    globals_[name] = native_ref{index, false};
}

void vm::register_native_kwargs(const std::string& name, native_kwargs_function fn) {
    int index = 0;
    auto it = native_kwargs_index_by_name_.find(name);
    if (it == native_kwargs_index_by_name_.end()) {
        index = static_cast<int>(native_kwargs_table_.size());
        native_kwargs_index_by_name_[name] = index;
        native_kwargs_table_.push_back(std::move(fn));
    } else {
        index = it->second;
        native_kwargs_table_[static_cast<size_t>(index)] = std::move(fn);
    }
    globals_[name] = native_ref{index, true};
}

void vm::set_limits(const sandbox_limits& limits) {
    limits_ = limits;
}

void vm::push(mv_value val) {
    stack_.push_back(std::move(val));
}

mv_value vm::pop() {
    mv_value val = std::move(stack_.back());
    stack_.pop_back();
    return val;
}

mv_value& vm::peek_ref(int offset) {
    return stack_[stack_.size() - 1 - offset];
}

const std::string& vm::get_string_constant(uint32_t idx) const {
    return std::get<std::string>(program_.constants[idx]);
}

std::optional<vm_error> vm::check_number_result(double val, int line) {
    if (std::isnan(val) || std::isinf(val)) {
        return vm_error{"numeric result is NaN or infinity", line};
    }
    return std::nullopt;
}

vm_result vm::run_top_level() {
    auto it = program_.function_map.find("__main__");
    if (it == program_.function_map.end()) {
        return {std::monostate{}, vm_error{"no main chunk found", 0}, false};
    }

    // Register all script function names as globals so call opcode can resolve them
    for (auto& [fname, fidx] : program_.function_map) {
        if (fname != "__main__") {
            globals_[fname] = function_ref{fidx};
        }
    }

    step_count_ = 0;
    stack_.clear();
    frames_.clear();

    call_frame frame;
    frame.chunk = &program_.functions[it->second];
    frame.ip = 0;
    frame.stack_base = 0;
    frame.locals.resize(frame.chunk->local_count);

    frames_.push_back(std::move(frame));
    return execute();
}

vm_result vm::call_function(const std::string& name, const std::vector<mv_value>& args) {
    // First run top level to define functions
    auto top_result = run_top_level();
    if (!top_result.success) return top_result;

    auto it = program_.function_map.find(name);
    if (it == program_.function_map.end()) {
        return {std::monostate{}, vm_error{"function '" + name + "' not defined", 0}, false};
    }

    step_count_ = 0;
    stack_.clear();
    frames_.clear();

    const function_chunk& func = program_.functions[it->second];
    if (static_cast<int>(args.size()) != func.param_count) {
        return {std::monostate{}, vm_error{
            "function '" + name + "' expects " + std::to_string(func.param_count) +
            " args, got " + std::to_string(args.size()), 0}, false};
    }

    call_frame frame;
    frame.chunk = &func;
    frame.ip = 0;
    frame.stack_base = 0;
    frame.locals.resize(func.local_count);

    for (int i = 0; i < static_cast<int>(args.size()); i++) {
        frame.locals[i] = args[i];
    }

    frames_.push_back(std::move(frame));
    return execute();
}

vm_result vm::execute() {
    while (!frames_.empty()) {
        auto& frame = frames_.back();

        if (frame.ip >= static_cast<int>(frame.chunk->code.size())) {
            frames_.pop_back();
            if (frames_.empty()) {
                { mv_value v = stack_.empty() ? mv_value{std::monostate{}} : pop(); return {std::move(v), std::nullopt, true}; }
            }
            push(std::monostate{}); // implicit return None
            continue;
        }

        auto err = run_instruction(frame);
        if (err) {
            return {std::monostate{}, std::move(*err), false};
        }
    }

    { mv_value v = stack_.empty() ? mv_value{std::monostate{}} : pop(); return {std::move(v), std::nullopt, true}; }
}

std::optional<vm_error> vm::run_instruction(call_frame& frame) {
    if (++step_count_ > limits_.max_steps) {
        return vm_error{"execution step limit exceeded (" + std::to_string(limits_.max_steps) + ")", 0};
    }

    const instruction& instr = frame.chunk->code[frame.ip++];
    int line = instr.source_line;

    switch (instr.op) {
    case opcode::load_const:
        push(std::visit([](const auto& v) -> mv_value { return v; }, program_.constants[instr.arg]));
        break;

    case opcode::load_none: push(std::monostate{}); break;
    case opcode::load_true: push(true); break;
    case opcode::load_false: push(false); break;
    case opcode::pop: pop(); break;

    case opcode::load_local:
        if (instr.arg < frame.locals.size()) {
            push(frame.locals[instr.arg]);
        } else {
            return vm_error{"invalid local variable slot", line};
        }
        break;

    case opcode::store_local:
        if (instr.arg >= frame.locals.size()) {
            frame.locals.resize(instr.arg + 1);
        }
        frame.locals[instr.arg] = pop();
        break;

    case opcode::load_global: {
        const std::string& name = get_string_constant(instr.arg);
        auto it = globals_.find(name);
        if (it != globals_.end()) {
            push(it->second);
        } else {
            return vm_error{"undefined variable '" + name + "'", line};
        }
        break;
    }

    case opcode::store_global: {
        const std::string& name = get_string_constant(instr.arg);
        globals_[name] = pop();
        break;
    }

    case opcode::add: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = *na + *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        if (auto* sa = std::get_if<std::string>(&a)) {
            if (auto* sb = std::get_if<std::string>(&b)) {
                std::string result = *sa + *sb;
                if (static_cast<int>(result.size()) > limits_.max_string_length) {
                    return vm_error{"string length exceeds limit", line};
                }
                push(std::move(result));
                break;
            }
        }
        if (auto* la = std::get_if<std::shared_ptr<mv_list>>(&a)) {
            if (auto* lb = std::get_if<std::shared_ptr<mv_list>>(&b)) {
                auto result = std::make_shared<mv_list>();
                if (*la) result->elements = (*la)->elements;
                if (*lb) {
                    result->elements.insert(result->elements.end(),
                                            (*lb)->elements.begin(), (*lb)->elements.end());
                }
                if (static_cast<int>(result->elements.size()) > limits_.max_list_size) {
                    return vm_error{"list size exceeds limit", line};
                }
                push(mv_value{result});
                break;
            }
        }
        return vm_error{"cannot add " + value_type_name(a) + " and " + value_type_name(b), line};
    }

    case opcode::sub: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = *na - *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot subtract " + value_type_name(b) + " from " + value_type_name(a), line};
    }

    case opcode::mul: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = *na * *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot multiply " + value_type_name(a) + " and " + value_type_name(b), line};
    }

    case opcode::div_op: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                if (*nb == 0.0) return vm_error{"division by zero", line};
                double result = *na / *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot divide " + value_type_name(a) + " by " + value_type_name(b), line};
    }

    case opcode::mod: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                if (*nb == 0.0) return vm_error{"modulo by zero", line};
                double result = std::fmod(*na, *nb);
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot modulo " + value_type_name(a) + " by " + value_type_name(b), line};
    }

    case opcode::power: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = std::pow(*na, *nb);
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot raise " + value_type_name(a) + " to power " + value_type_name(b), line};
    }

    case opcode::negate: {
        auto val = pop();
        if (auto* n = std::get_if<double>(&val)) {
            push(-*n);
            break;
        }
        return vm_error{"cannot negate " + value_type_name(val), line};
    }

    case opcode::cmp_eq: { auto b = pop(), a = pop(); push(a == b); break; }
    case opcode::cmp_ne: { auto b = pop(), a = pop(); push(a != b); break; }

    case opcode::cmp_lt: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na < *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }
    case opcode::cmp_gt: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na > *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }
    case opcode::cmp_le: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na <= *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }
    case opcode::cmp_ge: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na >= *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }

    case opcode::logical_not:
        push(!is_truthy(pop()));
        break;

    case opcode::jump:
        frame.ip = static_cast<int>(instr.arg);
        break;

    case opcode::jump_if_false:
        if (!is_truthy(peek_ref())) {
            frame.ip = static_cast<int>(instr.arg);
        }
        break;

    case opcode::jump_if_true:
        if (is_truthy(peek_ref())) {
            frame.ip = static_cast<int>(instr.arg);
        }
        break;

    case opcode::call: {
        int arg_count = static_cast<int>(instr.arg);
        mv_value callee = stack_[stack_.size() - 1 - arg_count];

        if (auto* native = std::get_if<native_ref>(&callee)) {
            std::vector<mv_value> args(arg_count);
            for (int i = arg_count - 1; i >= 0; i--) args[i] = pop();
            pop(); // callee
            if (native->kwargs) {
                if (native->index >= 0 && native->index < static_cast<int>(native_kwargs_table_.size())) {
                    std::vector<std::pair<std::string, mv_value>> empty_kwargs;
                    push(native_kwargs_table_[static_cast<size_t>(native->index)](args, empty_kwargs));
                    break;
                }
            } else if (native->index >= 0 && native->index < static_cast<int>(native_table_.size())) {
                push(native_table_[static_cast<size_t>(native->index)](args));
                break;
            }
        }

        int func_index = -1;
        if (auto* fn = std::get_if<function_ref>(&callee)) {
            func_index = fn->index;
        } else if (auto* s = std::get_if<std::string>(&callee)) {
            auto func_it = program_.function_map.find(*s);
            if (func_it != program_.function_map.end()) {
                func_index = func_it->second;
            }
        }

        if (func_index < 0 || func_index >= static_cast<int>(program_.functions.size())) {
            return vm_error{"'" + value_to_string(callee) + "' is not callable", line};
        }

        if (static_cast<int>(frames_.size()) >= limits_.max_call_depth) {
            return vm_error{"call depth limit exceeded (" + std::to_string(limits_.max_call_depth) + ")", line};
        }

        const function_chunk& func = program_.functions[static_cast<size_t>(func_index)];
        if (arg_count != func.param_count) {
            return vm_error{
                "function '" + func.name + "' expects " + std::to_string(func.param_count) +
                " args, got " + std::to_string(arg_count), line};
        }

        call_frame new_frame;
        new_frame.chunk = &func;
        new_frame.ip = 0;
        new_frame.stack_base = static_cast<int>(stack_.size()) - arg_count - 1;
        new_frame.locals.resize(func.local_count);

        for (int i = arg_count - 1; i >= 0; i--) {
            new_frame.locals[i] = pop();
        }
        pop(); // callee

        frames_.push_back(std::move(new_frame));
        break;
    }

    case opcode::call_kwargs: {
        int positional_count = static_cast<int>(instr.arg);
        // Next instruction must be kwarg_count
        if (frame.ip >= static_cast<int>(frame.chunk->code.size()) ||
            frame.chunk->code[frame.ip].op != opcode::kwarg_count) {
            return vm_error{"internal error: expected kwarg_count after call_kwargs", line};
        }
        int kw_count = static_cast<int>(frame.chunk->code[frame.ip++].arg);

        // Pop kwargs (value, name pairs in reverse)
        std::vector<std::pair<std::string, mv_value>> kwargs(kw_count);
        for (int i = kw_count - 1; i >= 0; i--) {
            mv_value val = pop();
            mv_value name_val = pop();
            kwargs[i] = {std::get<std::string>(name_val), std::move(val)};
        }

        // Pop positional args
        std::vector<mv_value> pos_args(positional_count);
        for (int i = positional_count - 1; i >= 0; i--) {
            pos_args[i] = pop();
        }

        mv_value callee = pop();

        // Check for native kwargs function
        if (auto* native = std::get_if<native_ref>(&callee)) {
            if (native->kwargs) {
                if (native->index >= 0 && native->index < static_cast<int>(native_kwargs_table_.size())) {
                    push(native_kwargs_table_[static_cast<size_t>(native->index)](pos_args, kwargs));
                    break;
                }
            } else if (native->index >= 0 && native->index < static_cast<int>(native_table_.size())) {
                push(native_table_[static_cast<size_t>(native->index)](pos_args));
                break;
            }
        } else if (auto* s = std::get_if<std::string>(&callee)) {
            auto native_kwargs_it = native_kwargs_index_by_name_.find(*s);
            if (native_kwargs_it != native_kwargs_index_by_name_.end()) {
                push(native_kwargs_table_[static_cast<size_t>(native_kwargs_it->second)](pos_args, kwargs));
                break;
            }
            auto native_it = native_index_by_name_.find(*s);
            if (native_it != native_index_by_name_.end()) {
                push(native_table_[static_cast<size_t>(native_it->second)](pos_args));
                break;
            }
        }

        return vm_error{"kwargs call to non-native function not supported", line};
    }

    case opcode::kwarg_count:
        return vm_error{"internal error: unexpected kwarg_count", line};

    case opcode::load_kwarg_name: {
        const std::string& name = get_string_constant(instr.arg);
        push(name);
        break;
    }

    case opcode::return_op: {
        mv_value result = pop();
        frames_.pop_back();
        push(std::move(result));
        break;
    }

    case opcode::load_attr: {
        auto obj_val = pop();
        const std::string& attr_name = get_string_constant(instr.arg);
        if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&obj_val)) {
            if (*obj) {
                push((*obj)->get_attr(attr_name));
                break;
            }
        }
        return vm_error{"cannot access attribute '" + attr_name + "' on " + value_type_name(obj_val), line};
    }

    case opcode::store_attr: {
        auto val = pop();
        auto obj_val = pop();
        const std::string& attr_name = get_string_constant(instr.arg);
        if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&obj_val)) {
            if (*obj) {
                (*obj)->set_attr(attr_name, std::move(val));
                break;
            }
        }
        return vm_error{"cannot set attribute on " + value_type_name(obj_val), line};
    }

    case opcode::build_list: {
        int count = static_cast<int>(instr.arg);
        if (count > limits_.max_list_size) {
            return vm_error{"list size exceeds limit", line};
        }
        auto list = std::make_shared<mv_list>();
        list->elements.resize(count);
        for (int i = count - 1; i >= 0; i--) {
            list->elements[i] = pop();
        }
        try_build_list_render_caches(*list);
        push(std::move(list));
        break;
    }

    case opcode::append_list: {
        auto value = pop();
        auto list_val = pop();
        if (auto* list = std::get_if<std::shared_ptr<mv_list>>(&list_val)) {
            if (*list) {
                if (static_cast<int>((*list)->elements.size()) >= limits_.max_list_size) {
                    return vm_error{"list size exceeds limit", line};
                }
                (*list)->elements.push_back(std::move(value));
                update_list_caches_for_append(**list, (*list)->elements.back());
                push(std::monostate{});
                break;
            }
        }
        return vm_error{"cannot append to " + value_type_name(list_val), line};
    }

    case opcode::load_index: {
        auto idx_val = pop();
        auto obj_val = pop();
        if (auto* list = std::get_if<std::shared_ptr<mv_list>>(&obj_val)) {
            if (auto* idx = std::get_if<double>(&idx_val)) {
                int i = static_cast<int>(*idx);
                if (*list && i >= 0 && i < static_cast<int>((*list)->elements.size())) {
                    push((*list)->elements[i]);
                    break;
                }
                // Out of bounds -> None (used by for-loop pattern)
                push(std::monostate{});
                break;
            }
        }
        return vm_error{"cannot index " + value_type_name(obj_val) + " with " + value_type_name(idx_val), line};
    }

    case opcode::store_index: {
        auto val = pop();
        auto idx_val = pop();
        auto obj_val = pop();
        if (auto* list = std::get_if<std::shared_ptr<mv_list>>(&obj_val)) {
            if (auto* idx = std::get_if<double>(&idx_val)) {
                int i = static_cast<int>(*idx);
                if (*list && i >= 0 && i < static_cast<int>((*list)->elements.size())) {
                    (*list)->elements[i] = std::move(val);
                    (*list)->clear_cached_render_data();
                    break;
                }
                return vm_error{"list index out of range", line};
            }
        }
        return vm_error{"cannot index-assign " + value_type_name(obj_val), line};
    }
    }

    return std::nullopt;
}

} // namespace mv
