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
    if (!loaded_) return std::nullopt;

    auto ctx = build_context(input);
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
        return std::nullopt;
    }

    auto sc = extract_scene(result.value);
    if (!sc.has_value()) {
        static int log_count_b = 0;
        if (log_count_b < 5) {
            TraceLog(LOG_WARNING, "MV tick: extract_scene failed (return value is not a Scene)");
            TraceLog(LOG_WARNING, "MV tick:   value type index = %d", static_cast<int>(result.value.index()));
            if (auto obj = std::get_if<std::shared_ptr<mv_object>>(&result.value)) {
                TraceLog(LOG_WARNING, "MV tick:   object type_name = '%s'", (*obj)->type_name.c_str());
            }
            log_count_b++;
        }
        return std::nullopt;
    }

    // Inject spectrum data into SpectrumBar nodes
    for (auto& node : sc->nodes) {
        if (auto* sb = std::get_if<spectrum_bar_node>(&node)) {
            sb->spectrum.clear();
            sb->spectrum.reserve(input.spectrum.size());
            for (float v : input.spectrum) {
                sb->spectrum.push_back(v);
            }
        }
    }

    validate_scene(*sc, validation_limits_);
    return sc;
}

void mv_runtime::reset() {
    sandbox_ = sandbox{};
    loaded_ = false;
}

} // namespace mv
