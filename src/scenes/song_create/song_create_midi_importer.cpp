#include "song_create/song_create_midi_importer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string_view>

#include "localization/localization.h"
#include "path_utils.h"
#include "raylib.h"

namespace {

bool timing_event_less(const timing_event& left, const timing_event& right) {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    return static_cast<int>(left.type) < static_cast<int>(right.type);
}

std::uint16_t read_be16(const std::vector<std::uint8_t>& bytes, size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t read_be32(const std::vector<std::uint8_t>& bytes, size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

bool read_vlq(const std::vector<std::uint8_t>& bytes, size_t& offset, size_t end, std::uint32_t& value) {
    value = 0;
    for (int count = 0; count < 4; ++count) {
        if (offset >= end) {
            return false;
        }
        const std::uint8_t byte = bytes[offset++];
        value = (value << 7) | static_cast<std::uint32_t>(byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return true;
        }
    }
    return false;
}

int midi_channel_data_length(std::uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0:
        case 0xD0:
            return 1;
        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        case 0xE0:
            return 2;
        default:
            return -1;
    }
}

}  // namespace

namespace song_create {

midi_timing_import import_midi_timing_file(const std::string& path) {
    midi_timing_import result;
    std::ifstream file(path_utils::from_utf8(path), std::ios::binary);
    if (!file) {
        result.message = "MIDI file could not be opened.";
        return result;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    if (bytes.size() < 14 || std::string_view(reinterpret_cast<const char*>(bytes.data()), 4) != "MThd") {
        result.message = "Selected file is not a Standard MIDI file.";
        return result;
    }

    const std::uint32_t header_length = read_be32(bytes, 4);
    if (header_length < 6 || bytes.size() < 8ULL + header_length) {
        result.message = "MIDI header is incomplete.";
        return result;
    }
    const std::uint16_t track_count = read_be16(bytes, 10);
    const std::uint16_t division = read_be16(bytes, 12);
    if ((division & 0x8000) != 0) {
        result.message = "SMPTE-based MIDI timing is not supported.";
        return result;
    }
    if (division == 0) {
        result.message = "MIDI timing resolution is invalid.";
        return result;
    }
    result.resolution = static_cast<int>(division);

    size_t offset = 8 + header_length;
    bool saw_bpm = false;
    bool saw_meter = false;
    for (std::uint16_t track_index = 0; track_index < track_count && offset + 8 <= bytes.size(); ++track_index) {
        if (std::string_view(reinterpret_cast<const char*>(bytes.data() + offset), 4) != "MTrk") {
            result.message = "MIDI track chunk is missing.";
            return result;
        }
        const std::uint32_t track_length = read_be32(bytes, offset + 4);
        offset += 8;
        if (offset + track_length > bytes.size()) {
            result.message = "MIDI track chunk is incomplete.";
            return result;
        }

        const size_t track_end = offset + track_length;
        std::uint32_t absolute_tick = 0;
        std::uint8_t running_status = 0;
        while (offset < track_end) {
            std::uint32_t delta = 0;
            if (!read_vlq(bytes, offset, track_end, delta)) {
                result.message = "MIDI delta time is invalid.";
                return result;
            }
            absolute_tick += delta;
            if (offset >= track_end) {
                break;
            }

            std::uint8_t status = bytes[offset++];
            if (status < 0x80) {
                if (running_status == 0) {
                    result.message = "MIDI running status is invalid.";
                    return result;
                }
                --offset;
                status = running_status;
            } else if (status < 0xF0) {
                running_status = status;
            }

            if (status == 0xFF) {
                if (offset >= track_end) {
                    result.message = "MIDI meta event is incomplete.";
                    return result;
                }
                const std::uint8_t meta_type = bytes[offset++];
                std::uint32_t meta_length = 0;
                if (!read_vlq(bytes, offset, track_end, meta_length) ||
                    offset + meta_length > track_end) {
                    result.message = "MIDI meta event length is invalid.";
                    return result;
                }
                if (meta_type == 0x51 && meta_length == 3) {
                    const std::uint32_t micros_per_quarter =
                        (static_cast<std::uint32_t>(bytes[offset]) << 16) |
                        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                        static_cast<std::uint32_t>(bytes[offset + 2]);
                    if (micros_per_quarter > 0) {
                        const float bpm = 60000000.0f / static_cast<float>(micros_per_quarter);
                        result.events.push_back({timing_event_type::bpm,
                                                 static_cast<int>(absolute_tick), bpm, 4, 4});
                        if (absolute_tick == 0) {
                            result.base_bpm = bpm;
                        }
                        saw_bpm = true;
                    }
                } else if (meta_type == 0x58 && meta_length >= 2) {
                    const int numerator = static_cast<int>(bytes[offset]);
                    const int denominator_power = static_cast<int>(bytes[offset + 1]);
                    if (numerator > 0 && denominator_power >= 0 && denominator_power <= 10) {
                        result.events.push_back({timing_event_type::meter,
                                                 static_cast<int>(absolute_tick), 0.0f, numerator,
                                                 1 << denominator_power});
                        saw_meter = true;
                    }
                }
                offset += meta_length;
                if (meta_type == 0x2F) {
                    break;
                }
                continue;
            }

            if (status == 0xF0 || status == 0xF7) {
                std::uint32_t sysex_length = 0;
                if (!read_vlq(bytes, offset, track_end, sysex_length) ||
                    offset + sysex_length > track_end) {
                    result.message = "MIDI SysEx event length is invalid.";
                    return result;
                }
                offset += sysex_length;
                running_status = 0;
                continue;
            }

            const int data_length = midi_channel_data_length(status);
            if (data_length < 0 || offset + static_cast<size_t>(data_length) > track_end) {
                result.message = "MIDI channel event is invalid.";
                return result;
            }
            offset += static_cast<size_t>(data_length);
        }
        offset = track_end;
    }

    if (!saw_bpm) {
        result.events.push_back({timing_event_type::bpm, 0, 120.0f, 4, 4});
        result.base_bpm = 120.0f;
    }
    if (!saw_meter) {
        result.events.push_back({timing_event_type::meter, 0, 0.0f, 4, 4});
    }
    std::stable_sort(result.events.begin(), result.events.end(), timing_event_less);
    result.events.erase(std::unique(result.events.begin(), result.events.end(),
                                    [](const timing_event& left, const timing_event& right) {
                                        return left.type == right.type && left.tick == right.tick;
                                    }),
                        result.events.end());
    result.ok = true;
    result.message = TextFormat("%s %d",
                                localization::tr_literal("Imported MIDI timing events:"),
                                static_cast<int>(result.events.size()));
    return result;
}

int normalized_midi_tick(int source_tick, int source_resolution) {
    if (source_resolution <= 0 || source_resolution == 480) {
        return source_tick;
    }
    return static_cast<int>(std::lround(static_cast<double>(source_tick) * 480.0 /
                                        static_cast<double>(source_resolution)));
}

}  // namespace song_create
