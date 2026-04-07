#pragma once

#include "../lang/mv_sandbox.h"
#include "../lang/mv_vm.h"
#include "mv_scene.h"

namespace mv {

// Register all builtin functions (math, color, Scene/Node constructors) into the VM.
void register_builtins(vm& vm);

// Register all builtins into a sandbox (same functions, sandbox API).
void register_builtins_to_sandbox(sandbox& sb);

// Convert a VM mv_value (expected to be a Scene mv_object) into a C++ scene struct.
// Returns nullopt if the value is not a valid scene.
std::optional<scene> extract_scene(const mv_value& val);

} // namespace mv
