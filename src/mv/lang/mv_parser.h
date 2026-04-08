#pragma once

#include "mv_ast.h"

#include <string>
#include <vector>

namespace mv {

struct parse_result {
    program prog;
    std::vector<std::string> errors;
    bool success = true;
};

parse_result parse(const std::string& source);

} // namespace mv
