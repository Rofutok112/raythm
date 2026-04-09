#pragma once

#include "api/mv_context.h"
#include "api/mv_scene.h"
#include "lang/mv_sandbox.h"
#include "render/mv_validator.h"

#include <optional>
#include <string>

namespace mv {

// Per-song MV script runtime.
// Lifecycle: load script → compile once → tick every frame with context_input.
class mv_runtime {
public:
    // Load and compile script from file. Returns false on error.
    bool load_file(const std::string& path);

    // Load and compile script from string.
    bool load_source(const std::string& source);

    // Returns true if a script is compiled and ready.
    bool is_loaded() const { return loaded_; }

    // Execute draw(ctx) and return the validated scene.
    // Returns nullopt on error or if no script is loaded.
    std::optional<scene> tick(const context_input& input);

    // Get last error messages (compile or runtime).
    const std::vector<script_error>& last_errors() const { return sandbox_.last_errors(); }

    // Reset state (unload script).
    void reset();

private:
    sandbox sandbox_;
    context_builder context_builder_;
    bool loaded_ = false;
    validation_limits validation_limits_;
};

} // namespace mv
