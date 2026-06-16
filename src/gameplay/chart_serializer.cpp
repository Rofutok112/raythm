#include "chart_serializer.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>

#include "path_utils.h"
#include <vector>

namespace {
std::string format_float(float value) {
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return stream.str();
}

const char* timing_type_name(timing_event_type type) {
    switch (type) {
        case timing_event_type::bpm:
            return "bpm";
        case timing_event_type::meter:
            return "meter";
    }

    return "";
}

const char* scroll_automation_curve_name(scroll_automation_curve curve) {
    switch (curve) {
        case scroll_automation_curve::hold:
            return "hold";
        case scroll_automation_curve::linear:
            return "linear";
        case scroll_automation_curve::ease_in:
            return "easeIn";
        case scroll_automation_curve::ease_out:
            return "easeOut";
        case scroll_automation_curve::ease_in_out:
            return "easeInOut";
    }

    return "";
}

const char* note_type_name(note_type type) {
    switch (type) {
        case note_type::tap:
            return "tap";
        case note_type::hold:
            return "hold";
        case note_type::release:
            return "release";
        case note_type::stay:
            return "stay";
        case note_type::decorative_hold:
            return "decorativeHold";
    }

    return "";
}

bool scroll_guides_are_default(const scroll_automation_guides& guides) {
    constexpr std::array<float, 4> kDefault = {0.0f, 0.5f, 1.5f, 2.0f};
    for (size_t index = 0; index < kDefault.size(); ++index) {
        if (guides.values[index] != kDefault[index]) {
            return false;
        }
    }
    return true;
}

}

bool write_chart(std::ostream& output, const chart_data& data) {
    if (data.meta.chart_id.empty()) {
        return false;
    }

    output << "[Metadata]\n";
    output << "chartId=" << data.meta.chart_id << '\n';
    output << "keyCount=" << data.meta.key_count << '\n';
    output << "difficulty=" << data.meta.difficulty << '\n';
    output << "chartAuthor=" << data.meta.chart_author << '\n';
    const bool needs_format_v2 = std::any_of(data.notes.begin(), data.notes.end(), [](const note_data& note) {
        return note.type == note_type::release ||
               note.type == note_type::stay ||
               note.is_ray ||
               note_lane_width(note) > 1;
    });
    const bool needs_format_v5 = std::any_of(data.notes.begin(), data.notes.end(), [](const note_data& note) {
        return note.type == note_type::decorative_hold;
    });
    int required_format_version = needs_format_v5 ? 5 : (needs_format_v2 ? 2 : 1);
    if (!data.scroll_automation.empty()) {
        required_format_version = std::max(required_format_version, 4);
    }
    if (!scroll_guides_are_default(data.scroll_guides)) {
        required_format_version = std::max(required_format_version, 4);
    }
    output << "formatVersion=" << std::max(data.meta.format_version, required_format_version) << '\n';
    output << "resolution=" << data.meta.resolution << '\n';
    output << "offset=" << data.meta.offset << '\n';
    output << '\n';

    std::vector<timing_event> sorted_timing = data.timing_events;
    std::stable_sort(sorted_timing.begin(), sorted_timing.end(), [](const timing_event& left, const timing_event& right) {
        return left.tick < right.tick;
    });

    output << "[Timing]\n";
    for (const timing_event& event : sorted_timing) {
        output << timing_type_name(event.type) << ',' << event.tick << ',';
        if (event.type == timing_event_type::bpm) {
            output << format_float(event.bpm);
        } else {
            output << event.numerator << '/' << event.denominator;
        }
        output << '\n';
    }
    output << '\n';

    if (!data.scroll_automation.empty()) {
        std::vector<scroll_automation_point> sorted_points = data.scroll_automation;
        std::stable_sort(sorted_points.begin(), sorted_points.end(), [](const scroll_automation_point& left,
                                                                        const scroll_automation_point& right) {
            return left.tick < right.tick;
        });

        output << "[ScrollAutomation]\n";
        for (const scroll_automation_point& point : sorted_points) {
            output << "point," << point.tick << ',' << format_float(point.multiplier) << ','
                   << scroll_automation_curve_name(point.curve_to_next) << '\n';
        }
        output << '\n';
    }

    if (!scroll_guides_are_default(data.scroll_guides)) {
        output << "[ScrollAutomationGuides]\n";
        output << "guides";
        for (const float guide : data.scroll_guides.values) {
            output << ',' << format_float(guide);
        }
        output << "\n\n";
    }

    std::vector<note_data> sorted_notes = data.notes;
    std::sort(sorted_notes.begin(), sorted_notes.end(), [](const note_data& left, const note_data& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        return left.lane < right.lane;
    });

    output << "[Notes]\n";
    for (const note_data& note : sorted_notes) {
        output << note_type_name(note.type) << ',' << note.tick << ',' << note.lane;
        if (note_has_duration(note)) {
            output << ',' << note.end_tick;
        }
        if (note_lane_width(note) > 1) {
            output << ",width=" << note_lane_width(note);
        }
        if (note.is_ray) {
            output << ",ray";
        }
        output << '\n';
    }

    return output.good();
}

std::string chart_serializer::serialize_to_string(const chart_data& data) {
    std::ostringstream output;
    if (!write_chart(output, data)) {
        return {};
    }
    return output.str();
}

bool chart_serializer::serialize(const chart_data& data, const std::string& file_path) {
    std::ofstream output(path_utils::from_utf8(file_path), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    return write_chart(output, data);
}
