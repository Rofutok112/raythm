#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mv {

enum class opcode : uint8_t {
    // Stack
    load_const,       // arg: constant index
    load_none,
    load_true,
    load_false,
    pop,

    // Variables
    load_local,       // arg: local slot
    store_local,      // arg: local slot
    load_global,      // arg: name index in constant pool (string)
    store_global,     // arg: name index

    // Arithmetic
    add,
    sub,
    mul,
    div_op,
    mod,
    power,
    negate,

    // Comparison
    cmp_eq,
    cmp_ne,
    cmp_lt,
    cmp_gt,
    cmp_le,
    cmp_ge,

    // Logic
    logical_not,

    // Control flow
    jump,             // arg: absolute offset
    jump_if_false,    // arg: absolute offset
    jump_if_true,     // arg: absolute offset

    // Functions
    call,             // arg: arg count
    return_op,

    // Objects
    load_attr,        // arg: name index
    store_attr,       // arg: name index

    // Lists
    build_list,       // arg: element count
    load_index,
    store_index,

    // Kwargs call
    call_kwargs,      // arg: positional count, followed by kwarg count instruction
    kwarg_count,      // arg: kwarg count (always follows call_kwargs)
    load_kwarg_name,  // arg: name index (emitted before each kwarg value)
};

struct instruction {
    opcode op;
    uint32_t arg = 0;
    int source_line = 0;
};

using constant_value = std::variant<double, bool, std::string>;

struct function_chunk {
    std::string name;
    int param_count = 0;
    int local_count = 0;
    std::vector<instruction> code;
};

struct compiled_program {
    std::vector<constant_value> constants;
    std::vector<function_chunk> functions;      // index 0 = top-level (implicit __main__)
    std::unordered_map<std::string, int> function_map; // name → function index
};

} // namespace mv
