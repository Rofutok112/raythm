#include "mv_runtime.h"

#include "api/mv_builtins.h"
#include "api/mv_context.h"
#include "render/mv_renderer.h"

#include <fstream>
#include <sstream>

#include "raylib.h"

namespace mv {

bool mv_runtime::load_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    return load_source(ss.str());
}

bool mv_runtime::load_source(const std::string& source) {
    loaded_ = false;
    sandbox_ = sandbox{};

    register_builtins_to_sandbox(sandbox_);

    if (!sandbox_.compile(source)) return false;

    loaded_ = true;
    return true;
}

std::optional<scene> mv_runtime::tick(const context_input& input) {
    const scene* scene_ptr = tick_ref(input);
    if (scene_ptr == nullptr) {
        return std::nullopt;
    }
    return *scene_ptr;
}

const scene* mv_runtime::tick_ref(const context_input& input) {
    if (!loaded_) return nullptr;

    auto ctx = context_builder_.build(input);
    auto result = sandbox_.call("draw", {mv_value{ctx}});

    if (!result.success) {
        static int log_count_a = 0;
        if (log_count_a < 5) {
            TraceLog(LOG_WARNING, "MV tick: call('draw') failed");
            for (const auto& e : result.errors) {
                TraceLog(LOG_WARNING, "MV tick:   L%d [%s] %s", e.line, e.phase.c_str(), e.message.c_str());
            }
            log_count_a++;
        }
        return nullptr;
    }

    if (!extract_scene_into(result.value, scratch_scene_)) {
        static int log_count_b = 0;
        if (log_count_b < 5) {
            TraceLog(LOG_WARNING, "MV tick: extract_scene failed (return value is not a Scene)");
            TraceLog(LOG_WARNING, "MV tick:   value type index = %d", static_cast<int>(result.value.index()));
            if (auto obj = std::get_if<std::shared_ptr<mv_object>>(&result.value)) {
                TraceLog(LOG_WARNING, "MV tick:   object type_name = '%s'", (*obj)->type_name.c_str());
            }
            log_count_b++;
        }
        return nullptr;
    }

    if (validation_enabled_) {
        validate_scene(scratch_scene_, validation_limits_);
    }
    return &scratch_scene_;
}

void mv_runtime::reset() {
    sandbox_ = sandbox{};
    loaded_ = false;
}

} // namespace mv
