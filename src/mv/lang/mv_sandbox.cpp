#include "mv_sandbox.h"

#include "raylib.h"

namespace mv {

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
