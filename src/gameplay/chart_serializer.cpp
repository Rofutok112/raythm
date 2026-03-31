#include "chart_serializer.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
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

const char* note_type_name(note_type type) {
    switch (type) {
        case note_type::tap:
            return "tap";
        case note_type::hold:
            return "hold";
    }

    return "";
}
}

bool chart_serializer::serialize(const chart_data& data, const std::string& file_path) {
    std::ofstream output(file_path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "[Metadata]\n";
    output << "chartId=" << data.meta.chart_id << '\n';
    output << "keyCount=" << data.meta.key_count << '\n';
    output << "difficulty=" << data.meta.difficulty << '\n';
    output << "level=" << data.meta.level << '\n';
    output << "chartAuthor=" << data.meta.chart_author << '\n';
    output << "formatVersion=" << data.meta.format_version << '\n';
    output << "resolution=" << data.meta.resolution << '\n';
    output << "offset=" << data.meta.offset << "\n\n";

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
        if (note.type == note_type::hold) {
            output << ',' << note.end_tick;
        }
        output << '\n';
    }

    return output.good();
}
