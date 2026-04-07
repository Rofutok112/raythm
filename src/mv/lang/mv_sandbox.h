#pragma once

#include "mv_parser.h"
#include "mv_compiler.h"
#include "mv_vm.h"

#include <string>
#include <vector>

namespace mv {

struct script_error {
    std::string phase;   // "lex", "parse", "compile", "runtime"
    std::string message;
    int line = 0;
    int column = 0;
};

struct script_result {
    mv_value value;
    std::vector<script_error> errors;
    bool success = true;
};

class sandbox {
public:
    void set_limits(const sandbox_limits& limits);
    void set_global(const std::string& name, mv_value value);
    void register_native(const std::string& name, native_function fn);
    void register_native_kwargs(const std::string& name, native_kwargs_function fn);

    // Compile source into a prepared script. Returns false on error.
    bool compile(const std::string& source);

    // Call a function on the already-compiled script.
    script_result call(const std::string& func_name, const std::vector<mv_value>& args);

    // Convenience: compile + call draw(ctx)
    script_result run_draw(const std::string& source, mv_value ctx);

    const std::vector<script_error>& last_errors() const { return errors_; }

private:
    sandbox_limits limits_;
    std::vector<script_error> errors_;

    // Compiled state
    bool compiled_ = false;
    compiled_program compiled_prog_;

    // Pre-registered globals and natives
    std::vector<std::pair<std::string, mv_value>> globals_;
    std::vector<std::pair<std::string, native_function>> natives_;
    std::vector<std::pair<std::string, native_kwargs_function>> natives_kwargs_;
};

} // namespace mv
