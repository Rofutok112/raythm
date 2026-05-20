#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "data_models.h"
#include "timing_engine.h"

class play_scroll_map final {
public:
    void init(const chart_data& chart, const timing_engine& timing) {
        automation_points_.clear();
        automation_points_.reserve(chart.scroll_automation.size());
        for (const scroll_automation_point& point : chart.scroll_automation) {
            automation_points_.push_back({
                timing.tick_to_ms(point.tick),
                std::max(0.0f, point.multiplier),
                point.curve_to_next,
            });
        }
        std::stable_sort(automation_points_.begin(), automation_points_.end(),
                         [](const automation_point& left, const automation_point& right) {
                             return left.ms < right.ms;
                         });

    }

    [[nodiscard]] double visual_ms_at(double source_ms) const {
        if (!automation_points_.empty()) {
            return automation_visual_ms_at(source_ms);
        }
        return source_ms;
    }

private:
    struct automation_point {
        double ms = 0.0;
        float multiplier = 1.0f;
        scroll_automation_curve curve_to_next = scroll_automation_curve::hold;
    };

    [[nodiscard]] static double ease_integral(scroll_automation_curve curve, double t) {
        t = std::clamp(t, 0.0, 1.0);
        switch (curve) {
            case scroll_automation_curve::hold:
                return 0.0;
            case scroll_automation_curve::linear:
                return t * t * 0.5;
            case scroll_automation_curve::ease_in:
                return t * t * t / 3.0;
            case scroll_automation_curve::ease_out:
                return t * t - t * t * t / 3.0;
            case scroll_automation_curve::ease_in_out:
                return t * t * t - 0.5 * t * t * t * t;
        }
        return t * t * 0.5;
    }

    [[nodiscard]] static double automation_segment_delta(double elapsed_ms,
                                                         double duration_ms,
                                                         float start_multiplier,
                                                         float end_multiplier,
                                                         scroll_automation_curve curve) {
        if (elapsed_ms <= 0.0 || duration_ms <= 0.0) {
            return 0.0;
        }
        const double clamped_elapsed = std::clamp(elapsed_ms, 0.0, duration_ms);
        if (curve == scroll_automation_curve::hold) {
            return clamped_elapsed * (static_cast<double>(start_multiplier) - 1.0);
        }
        const double t = clamped_elapsed / duration_ms;
        const double multiplier_integral =
            clamped_elapsed * static_cast<double>(start_multiplier) +
            duration_ms * static_cast<double>(end_multiplier - start_multiplier) * ease_integral(curve, t);
        return multiplier_integral - clamped_elapsed;
    }

    [[nodiscard]] double automation_visual_ms_at(double source_ms) const {
        double visual_ms = source_ms;
        for (size_t index = 0; index + 1 < automation_points_.size(); ++index) {
            const automation_point& current = automation_points_[index];
            const automation_point& next = automation_points_[index + 1];
            if (source_ms <= current.ms) {
                break;
            }

            const double duration_ms = next.ms - current.ms;
            if (duration_ms <= 0.0) {
                continue;
            }
            const double elapsed_ms = std::min(source_ms, next.ms) - current.ms;
            visual_ms += automation_segment_delta(elapsed_ms, duration_ms,
                                                  current.multiplier, next.multiplier,
                                                  current.curve_to_next);
            if (source_ms <= next.ms) {
                return visual_ms;
            }
        }

        const automation_point& last = automation_points_.back();
        if (source_ms > last.ms) {
            visual_ms += (source_ms - last.ms) * (static_cast<double>(last.multiplier) - 1.0);
        }
        return visual_ms;
    }

    std::vector<automation_point> automation_points_;
};
