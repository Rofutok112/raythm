#pragma once

#include <string>
#include <vector>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct parse_result {
    bool success = false;
    mv_composition composition;
    std::vector<std::string> errors;
};

std::string serialize(const mv_composition& composition);
parse_result parse(const std::string& text);
std::string fingerprint(const mv_composition& composition);

}  // namespace mv::composition
