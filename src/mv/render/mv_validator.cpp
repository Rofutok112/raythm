#include "mv_validator.h"

#include <cmath>
#include <sstream>

namespace mv {

namespace {

bool is_finite(float v) { return std::isfinite(v); }

// Clamp and sanitize a single float, return true if it was changed
bool sanitize_float(float& v, float lo, float hi) {
    if (!is_finite(v)) { v = 0; return true; }
    if (v < lo) { v = lo; return true; }
    if (v > hi) { v = hi; return true; }
    return false;
}

} // anonymous namespace

validation_result validate_scene(scene& sc, const validation_limits& limits) {
    validation_result result;

    // Node count limit
    if (static_cast<int>(sc.nodes.size()) > limits.max_nodes) {
        std::ostringstream oss;
        oss << "node count " << sc.nodes.size() << " exceeds limit "
            << limits.max_nodes << ", truncated";
        result.warnings.push_back(oss.str());
        sc.nodes.resize(limits.max_nodes);
    }

    // Sanitize node values (NaN/inf → 0, clamp to reasonable range)
    constexpr float kCoordMax = 4096.0f;
    constexpr float kSizeMax = 4096.0f;

    for (auto& node : sc.nodes) {
        std::visit([&](auto& n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, rect_node>) {
                sanitize_float(n.x, -kCoordMax, kCoordMax);
                sanitize_float(n.y, -kCoordMax, kCoordMax);
                sanitize_float(n.w, 0, kSizeMax);
                sanitize_float(n.h, 0, kSizeMax);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, line_node>) {
                sanitize_float(n.x1, -kCoordMax, kCoordMax);
                sanitize_float(n.y1, -kCoordMax, kCoordMax);
                sanitize_float(n.x2, -kCoordMax, kCoordMax);
                sanitize_float(n.y2, -kCoordMax, kCoordMax);
                sanitize_float(n.thickness, 0, 100);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, text_node>) {
                sanitize_float(n.x, -kCoordMax, kCoordMax);
                sanitize_float(n.y, -kCoordMax, kCoordMax);
                sanitize_float(n.opacity, 0, 1);
                if (n.font_size < 1) n.font_size = 1;
                if (n.font_size > 200) n.font_size = 200;
                if (n.text.size() > 256) n.text.resize(256);
            }
            else if constexpr (std::is_same_v<T, circle_node>) {
                sanitize_float(n.cx, -kCoordMax, kCoordMax);
                sanitize_float(n.cy, -kCoordMax, kCoordMax);
                sanitize_float(n.radius, 0, kSizeMax);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, polyline_node>) {
                for (auto& p : n.points) {
                    sanitize_float(p.x, -kCoordMax, kCoordMax);
                    sanitize_float(p.y, -kCoordMax, kCoordMax);
                }
                sanitize_float(n.thickness, 0, 100);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, spectrum_bar_node>) {
                sanitize_float(n.x, -kCoordMax, kCoordMax);
                sanitize_float(n.y, -kCoordMax, kCoordMax);
                sanitize_float(n.w, 0, kSizeMax);
                sanitize_float(n.h, 0, kSizeMax);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, beat_grid_node>) {
                sanitize_float(n.x, -kCoordMax, kCoordMax);
                sanitize_float(n.y, -kCoordMax, kCoordMax);
                sanitize_float(n.w, 0, kSizeMax);
                sanitize_float(n.h, 0, kSizeMax);
                sanitize_float(n.beat_phase, 0, 1);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, pulse_ring_node>) {
                sanitize_float(n.cx, -kCoordMax, kCoordMax);
                sanitize_float(n.cy, -kCoordMax, kCoordMax);
                sanitize_float(n.radius, 0, kSizeMax);
                sanitize_float(n.beat_phase, 0, 1);
                sanitize_float(n.opacity, 0, 1);
            }
            else if constexpr (std::is_same_v<T, background_node>) {
                sanitize_float(n.opacity, 0, 1);
            }
        }, node);
    }

    return result;
}

} // namespace mv
