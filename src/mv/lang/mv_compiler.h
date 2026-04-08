#pragma once

#include "mv_ast.h"
#include "mv_bytecode.h"

#include <string>
#include <vector>

namespace mv {

struct compile_result {
    compiled_program program;
    std::vector<std::string> errors;
    bool success = true;
};

compile_result compile(const program& ast);

} // namespace mv
