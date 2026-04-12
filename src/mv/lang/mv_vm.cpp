#include "mv_vm.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace mv {

namespace {

template<typename T>
T* object_cast(const std::shared_ptr<mv_object>& object, mv_object_kind expected_kind) {
    if (!object || object->kind != expected_kind) return nullptr;
    return static_cast<T*>(object.get());
}

std::optional<vec2> cached_point_from_value(const mv_value& value) {
    if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&value)) {
        if (*obj && (*obj)->cached_point.has_value()) {
            return (*obj)->cached_point;
        }
    }
    return std::nullopt;
}

std::optional<scene_node> cached_scene_node_from_value(const mv_value& value) {
    if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&value)) {
        if (*obj && (*obj)->cached_scene_node.has_value()) {
            return (*obj)->cached_scene_node;
        }
    }
    return std::nullopt;
}

void try_build_list_render_caches(mv_list& list) {
    if (list.elements.empty()) {
        list.cached_points = std::vector<vec2>{};
        list.cached_scene_nodes = std::vector<scene_node>{};
        return;
    }

    bool all_points = true;
    bool all_scene_nodes = true;
    std::vector<vec2> points;
    std::vector<scene_node> scene_nodes;
    points.reserve(list.elements.size());
    scene_nodes.reserve(list.elements.size());

    for (const auto& element : list.elements) {
        if (all_points) {
            if (auto point = cached_point_from_value(element)) {
                points.push_back(*point);
            } else {
                all_points = false;
                points.clear();
            }
        }
        if (all_scene_nodes) {
            if (auto node = cached_scene_node_from_value(element)) {
                scene_nodes.push_back(*node);
            } else {
                all_scene_nodes = false;
                scene_nodes.clear();
            }
        }
        if (!all_points && !all_scene_nodes) {
            break;
        }
    }

    list.cached_points = all_points ? std::optional<std::vector<vec2>>(std::move(points)) : std::nullopt;
    list.cached_scene_nodes = all_scene_nodes
        ? std::optional<std::vector<scene_node>>(std::move(scene_nodes))
        : std::nullopt;
}

void update_list_caches_for_append(mv_list& list, const mv_value& appended) {
    if (list.cached_points.has_value()) {
        if (auto point = cached_point_from_value(appended)) {
            list.cached_points->push_back(*point);
        } else {
            list.cached_points.reset();
        }
    } else if (list.elements.size() == 1) {
        if (auto point = cached_point_from_value(appended)) {
            list.cached_points = std::vector<vec2>{*point};
        }
    }

    if (list.cached_scene_nodes.has_value()) {
        if (auto node = cached_scene_node_from_value(appended)) {
            list.cached_scene_nodes->push_back(*node);
        } else {
            list.cached_scene_nodes.reset();
        }
    } else if (list.elements.size() == 1) {
        if (auto node = cached_scene_node_from_value(appended)) {
            list.cached_scene_nodes = std::vector<scene_node>{*node};
        }
    }
}

double number_or_default(const mv_value& value, double fallback) {
    if (std::holds_alternative<std::monostate>(value)) return fallback;
    return detail::value_as_number(value, fallback);
}

std::string string_or_default(const mv_value& value, const std::string& fallback) {
    if (std::holds_alternative<std::monostate>(value)) return fallback;
    return detail::value_as_string(value, fallback);
}

std::shared_ptr<mv_list> list_or_empty(const mv_value& value) {
    if (auto list = detail::value_as_list(value)) return list;
    return std::make_shared<mv_list>();
}

mv_value color_value_or_default(mv_value value, const std::string& fallback) {
    if (std::holds_alternative<std::monostate>(value)) {
        return mv_value{fallback};
    }
    return value;
}

color parse_color_value(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return {255, 255, 255, 255};
    auto parse_byte = [&](size_t pos) -> uint8_t {
        if (pos + 1 >= hex.size()) return 0;
        auto from_hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        return static_cast<uint8_t>(from_hex(hex[pos]) * 16 + from_hex(hex[pos + 1]));
    };

    color parsed;
    if (hex.size() >= 7) {
        parsed.r = parse_byte(1);
        parsed.g = parse_byte(3);
        parsed.b = parse_byte(5);
    }
    parsed.a = (hex.size() >= 9) ? parse_byte(7) : 255;
    return parsed;
}

color color_from_value(const mv_value& value, color fallback) {
    if (auto* text = std::get_if<std::string>(&value)) {
        return parse_color_value(*text);
    }
    if (auto obj = detail::value_as_object(value)) {
        if (auto* color_obj = object_cast<color_mv_object>(obj, mv_object_kind::color)) {
            return {
                static_cast<uint8_t>(std::clamp(color_obj->r, 0.0, 255.0)),
                static_cast<uint8_t>(std::clamp(color_obj->g, 0.0, 255.0)),
                static_cast<uint8_t>(std::clamp(color_obj->b, 0.0, 255.0)),
                static_cast<uint8_t>(std::clamp(color_obj->a, 0.0, 255.0)),
            };
        }
    }
    return fallback;
}

vec2 point_from_value(const mv_value& value) {
    if (auto point = cached_point_from_value(value)) {
        return *point;
    }
    auto object = detail::value_as_object(value);
    if (auto* point_obj = object_cast<point_mv_object>(object, mv_object_kind::point)) {
        return {static_cast<float>(point_obj->x), static_cast<float>(point_obj->y)};
    }
    return {};
}

polyline_node polyline_from_list(const std::shared_ptr<mv_list>& points,
                                 const mv_value& stroke_value,
                                 double thickness,
                                 double opacity) {
    polyline_node node;
    node.stroke = color_from_value(stroke_value, node.stroke);
    node.thickness = static_cast<float>(thickness);
    node.opacity = static_cast<float>(opacity);

    if (!points) return node;

    if (points->cached_points.has_value()) {
        node.points = *points->cached_points;
        return node;
    }

    node.points.reserve(points->elements.size());
    for (const auto& point_value : points->elements) {
        auto point_object = detail::value_as_object(point_value);
        if (!point_object || point_object->kind != mv_object_kind::point) {
            continue;
        }
        node.points.push_back(point_from_value(point_value));
    }
    return node;
}

std::shared_ptr<mv_object> make_scene_object(const mv_value& nodes_value, mv_value clear_color) {
    auto object = std::make_shared<scene_mv_object>();
    object->nodes = list_or_empty(nodes_value);
    object->clear_color = std::move(clear_color);
    return object;
}

std::shared_ptr<mv_object> make_point_object(double x, double y) {
    auto object = std::make_shared<point_mv_object>();
    object->x = x;
    object->y = y;
    object->set_cached_point({static_cast<float>(x), static_cast<float>(y)});
    return object;
}

std::shared_ptr<mv_object> make_rect_object(double x,
                                            double y,
                                            double w,
                                            double h,
                                            double rotation,
                                            double opacity,
                                            mv_value fill) {
    auto object = std::make_shared<rect_mv_object>();
    object->x = x;
    object->y = y;
    object->w = w;
    object->h = h;
    object->rotation = rotation;
    object->opacity = opacity;
    object->fill = std::move(fill);

    rect_node node;
    node.x = static_cast<float>(x);
    node.y = static_cast<float>(y);
    node.w = static_cast<float>(w);
    node.h = static_cast<float>(h);
    node.rotation = static_cast<float>(rotation);
    node.opacity = static_cast<float>(opacity);
    node.fill = color_from_value(object->fill, node.fill);
    object->set_cached_scene_node(scene_node{node});
    return object;
}

std::shared_ptr<mv_object> make_line_object(double x1,
                                            double y1,
                                            double x2,
                                            double y2,
                                            double thickness,
                                            double opacity,
                                            mv_value stroke) {
    auto object = std::make_shared<line_mv_object>();
    object->x1 = x1;
    object->y1 = y1;
    object->x2 = x2;
    object->y2 = y2;
    object->thickness = thickness;
    object->opacity = opacity;
    object->stroke = std::move(stroke);

    line_node node;
    node.x1 = static_cast<float>(x1);
    node.y1 = static_cast<float>(y1);
    node.x2 = static_cast<float>(x2);
    node.y2 = static_cast<float>(y2);
    node.thickness = static_cast<float>(thickness);
    node.opacity = static_cast<float>(opacity);
    node.stroke = color_from_value(object->stroke, node.stroke);
    object->set_cached_scene_node(scene_node{node});
    return object;
}

std::shared_ptr<mv_object> make_text_object(std::string text,
                                            double x,
                                            double y,
                                            double font_size,
                                            double opacity,
                                            mv_value fill) {
    auto object = std::make_shared<text_mv_object>();
    object->text = std::move(text);
    object->x = x;
    object->y = y;
    object->font_size = font_size;
    object->opacity = opacity;
    object->fill = std::move(fill);

    text_node node;
    node.text = object->text;
    node.x = static_cast<float>(x);
    node.y = static_cast<float>(y);
    node.font_size = static_cast<int>(font_size);
    node.opacity = static_cast<float>(opacity);
    node.fill = color_from_value(object->fill, node.fill);
    object->set_cached_scene_node(scene_node{node});
    return object;
}

std::shared_ptr<mv_object> make_circle_object(double cx,
                                              double cy,
                                              double radius,
                                              double opacity,
                                              mv_value fill) {
    auto object = std::make_shared<circle_mv_object>();
    object->cx = cx;
    object->cy = cy;
    object->radius = radius;
    object->opacity = opacity;
    object->fill = std::move(fill);

    circle_node node;
    node.cx = static_cast<float>(cx);
    node.cy = static_cast<float>(cy);
    node.radius = static_cast<float>(radius);
    node.opacity = static_cast<float>(opacity);
    node.fill = color_from_value(object->fill, node.fill);
    object->set_cached_scene_node(scene_node{node});
    return object;
}

std::shared_ptr<mv_object> make_polyline_object(std::shared_ptr<mv_list> points,
                                                double thickness,
                                                double opacity,
                                                mv_value stroke) {
    auto object = std::make_shared<polyline_mv_object>();
    object->points = points ? std::move(points) : std::make_shared<mv_list>();
    object->thickness = thickness;
    object->opacity = opacity;
    object->stroke = std::move(stroke);
    object->set_cached_scene_node(scene_node{
        polyline_from_list(object->points, object->stroke, thickness, opacity)
    });
    return object;
}

std::shared_ptr<mv_object> make_background_object(mv_value fill, double opacity) {
    auto object = std::make_shared<background_mv_object>();
    object->fill = std::move(fill);
    object->opacity = opacity;

    background_node node;
    node.opacity = static_cast<float>(opacity);
    node.fill = color_from_value(object->fill, node.fill);
    object->set_cached_scene_node(scene_node{node});
    return object;
}

mv_value traverse_attr_path(mv_value current, std::initializer_list<std::string_view> path) {
    for (std::string_view part : path) {
        auto object = detail::value_as_object(current);
        if (!object) return std::monostate{};
        current = object->get_attr(std::string(part));
    }
    return current;
}

mv_value load_ctx_attr_fast(const mv_value& base_value, ctx_attr_slot slot) {
    auto ctx = detail::value_as_object(base_value);
    auto* root = object_cast<ctx_root_object>(ctx, mv_object_kind::ctx_root);
    if (root == nullptr) {
        switch (slot) {
        case ctx_attr_slot::ctx_time: return traverse_attr_path(base_value, {"time"});
        case ctx_attr_slot::ctx_time_ms: return traverse_attr_path(base_value, {"time", "ms"});
        case ctx_attr_slot::ctx_time_sec: return traverse_attr_path(base_value, {"time", "sec"});
        case ctx_attr_slot::ctx_time_length_ms: return traverse_attr_path(base_value, {"time", "length_ms"});
        case ctx_attr_slot::ctx_time_bpm: return traverse_attr_path(base_value, {"time", "bpm"});
        case ctx_attr_slot::ctx_time_beat: return traverse_attr_path(base_value, {"time", "beat"});
        case ctx_attr_slot::ctx_time_beat_phase: return traverse_attr_path(base_value, {"time", "beat_phase"});
        case ctx_attr_slot::ctx_time_meter_numerator: return traverse_attr_path(base_value, {"time", "meter_numerator"});
        case ctx_attr_slot::ctx_time_meter_denominator: return traverse_attr_path(base_value, {"time", "meter_denominator"});
        case ctx_attr_slot::ctx_time_progress: return traverse_attr_path(base_value, {"time", "progress"});
        case ctx_attr_slot::ctx_audio: return traverse_attr_path(base_value, {"audio"});
        case ctx_attr_slot::ctx_audio_analysis: return traverse_attr_path(base_value, {"audio", "analysis"});
        case ctx_attr_slot::ctx_audio_analysis_level: return traverse_attr_path(base_value, {"audio", "analysis", "level"});
        case ctx_attr_slot::ctx_audio_analysis_rms: return traverse_attr_path(base_value, {"audio", "analysis", "rms"});
        case ctx_attr_slot::ctx_audio_analysis_peak: return traverse_attr_path(base_value, {"audio", "analysis", "peak"});
        case ctx_attr_slot::ctx_audio_bands: return traverse_attr_path(base_value, {"audio", "bands"});
        case ctx_attr_slot::ctx_audio_bands_low: return traverse_attr_path(base_value, {"audio", "bands", "low"});
        case ctx_attr_slot::ctx_audio_bands_mid: return traverse_attr_path(base_value, {"audio", "bands", "mid"});
        case ctx_attr_slot::ctx_audio_bands_high: return traverse_attr_path(base_value, {"audio", "bands", "high"});
        case ctx_attr_slot::ctx_audio_buffers: return traverse_attr_path(base_value, {"audio", "buffers"});
        case ctx_attr_slot::ctx_audio_buffers_spectrum: return traverse_attr_path(base_value, {"audio", "buffers", "spectrum"});
        case ctx_attr_slot::ctx_audio_buffers_spectrum_size: return traverse_attr_path(base_value, {"audio", "buffers", "spectrum_size"});
        case ctx_attr_slot::ctx_audio_buffers_waveform: return traverse_attr_path(base_value, {"audio", "buffers", "waveform"});
        case ctx_attr_slot::ctx_audio_buffers_waveform_size: return traverse_attr_path(base_value, {"audio", "buffers", "waveform_size"});
        case ctx_attr_slot::ctx_audio_buffers_waveform_index: return traverse_attr_path(base_value, {"audio", "buffers", "waveform_index"});
        case ctx_attr_slot::ctx_audio_buffers_oscilloscope: return traverse_attr_path(base_value, {"audio", "buffers", "oscilloscope"});
        case ctx_attr_slot::ctx_audio_buffers_oscilloscope_size: return traverse_attr_path(base_value, {"audio", "buffers", "oscilloscope_size"});
        case ctx_attr_slot::ctx_song: return traverse_attr_path(base_value, {"song"});
        case ctx_attr_slot::ctx_song_song_id: return traverse_attr_path(base_value, {"song", "song_id"});
        case ctx_attr_slot::ctx_song_title: return traverse_attr_path(base_value, {"song", "title"});
        case ctx_attr_slot::ctx_song_artist: return traverse_attr_path(base_value, {"song", "artist"});
        case ctx_attr_slot::ctx_song_base_bpm: return traverse_attr_path(base_value, {"song", "base_bpm"});
        case ctx_attr_slot::ctx_chart: return traverse_attr_path(base_value, {"chart"});
        case ctx_attr_slot::ctx_chart_chart_id: return traverse_attr_path(base_value, {"chart", "chart_id"});
        case ctx_attr_slot::ctx_chart_song_id: return traverse_attr_path(base_value, {"chart", "song_id"});
        case ctx_attr_slot::ctx_chart_difficulty: return traverse_attr_path(base_value, {"chart", "difficulty"});
        case ctx_attr_slot::ctx_chart_level: return traverse_attr_path(base_value, {"chart", "level"});
        case ctx_attr_slot::ctx_chart_chart_author: return traverse_attr_path(base_value, {"chart", "chart_author"});
        case ctx_attr_slot::ctx_chart_resolution: return traverse_attr_path(base_value, {"chart", "resolution"});
        case ctx_attr_slot::ctx_chart_offset: return traverse_attr_path(base_value, {"chart", "offset"});
        case ctx_attr_slot::ctx_chart_total_notes: return traverse_attr_path(base_value, {"chart", "total_notes"});
        case ctx_attr_slot::ctx_chart_combo: return traverse_attr_path(base_value, {"chart", "combo"});
        case ctx_attr_slot::ctx_chart_accuracy: return traverse_attr_path(base_value, {"chart", "accuracy"});
        case ctx_attr_slot::ctx_chart_key_count: return traverse_attr_path(base_value, {"chart", "key_count"});
        case ctx_attr_slot::ctx_screen: return traverse_attr_path(base_value, {"screen"});
        case ctx_attr_slot::ctx_screen_w: return traverse_attr_path(base_value, {"screen", "w"});
        case ctx_attr_slot::ctx_screen_h: return traverse_attr_path(base_value, {"screen", "h"});
        }
        return std::monostate{};
    }

    auto* time = object_cast<ctx_time_object>(root->time, mv_object_kind::ctx_time);
    auto* audio = object_cast<ctx_audio_object>(root->audio, mv_object_kind::ctx_audio);
    auto* analysis = audio ? object_cast<ctx_audio_analysis_object>(audio->analysis, mv_object_kind::ctx_audio_analysis) : nullptr;
    auto* bands = audio ? object_cast<ctx_audio_bands_object>(audio->bands, mv_object_kind::ctx_audio_bands) : nullptr;
    auto* buffers = audio ? object_cast<ctx_audio_buffers_object>(audio->buffers, mv_object_kind::ctx_audio_buffers) : nullptr;
    auto* song = object_cast<ctx_song_object>(root->song, mv_object_kind::ctx_song);
    auto* chart = object_cast<ctx_chart_object>(root->chart, mv_object_kind::ctx_chart);
    auto* screen = object_cast<ctx_screen_object>(root->screen, mv_object_kind::ctx_screen);

    switch (slot) {
    case ctx_attr_slot::ctx_time: return mv_value{root->time};
    case ctx_attr_slot::ctx_time_ms: return time ? mv_value{time->ms} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_sec: return time ? mv_value{time->sec} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_length_ms: return time ? mv_value{time->length_ms} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_bpm: return time ? mv_value{time->bpm} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_beat: return time ? mv_value{time->beat} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_beat_phase: return time ? mv_value{time->beat_phase} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_meter_numerator: return time ? mv_value{time->meter_numerator} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_meter_denominator: return time ? mv_value{time->meter_denominator} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_time_progress: return time ? mv_value{time->progress} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio: return mv_value{root->audio};
    case ctx_attr_slot::ctx_audio_analysis: return audio ? mv_value{audio->analysis} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_analysis_level: return analysis ? mv_value{analysis->level} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_analysis_rms: return analysis ? mv_value{analysis->rms} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_analysis_peak: return analysis ? mv_value{analysis->peak} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_bands: return audio ? mv_value{audio->bands} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_bands_low: return bands ? mv_value{bands->low} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_bands_mid: return bands ? mv_value{bands->mid} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_bands_high: return bands ? mv_value{bands->high} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers: return audio ? mv_value{audio->buffers} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_spectrum: return buffers ? mv_value{buffers->spectrum} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_spectrum_size: return buffers ? mv_value{buffers->spectrum_size} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_waveform: return buffers ? mv_value{buffers->waveform} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_waveform_size: return buffers ? mv_value{buffers->waveform_size} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_waveform_index: return buffers ? mv_value{buffers->waveform_index} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_oscilloscope: return buffers ? mv_value{buffers->oscilloscope} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_audio_buffers_oscilloscope_size: return buffers ? mv_value{buffers->oscilloscope_size} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_song: return mv_value{root->song};
    case ctx_attr_slot::ctx_song_song_id: return song ? mv_value{song->song_id} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_song_title: return song ? mv_value{song->title} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_song_artist: return song ? mv_value{song->artist} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_song_base_bpm: return song ? mv_value{song->base_bpm} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart: return mv_value{root->chart};
    case ctx_attr_slot::ctx_chart_chart_id: return chart ? mv_value{chart->chart_id} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_song_id: return chart ? mv_value{chart->song_id} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_difficulty: return chart ? mv_value{chart->difficulty} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_level: return chart ? mv_value{chart->level} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_chart_author: return chart ? mv_value{chart->chart_author} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_resolution: return chart ? mv_value{chart->resolution} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_offset: return chart ? mv_value{chart->offset} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_total_notes: return chart ? mv_value{chart->total_notes} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_combo: return chart ? mv_value{chart->combo} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_accuracy: return chart ? mv_value{chart->accuracy} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_chart_key_count: return chart ? mv_value{chart->key_count} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_screen: return mv_value{root->screen};
    case ctx_attr_slot::ctx_screen_w: return screen ? mv_value{screen->w} : mv_value{std::monostate{}};
    case ctx_attr_slot::ctx_screen_h: return screen ? mv_value{screen->h} : mv_value{std::monostate{}};
    }

    return std::monostate{};
}

} // namespace

// ---- Helpers ----

bool is_truthy(const mv_value& val) {
    return std::visit([](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) return v != 0.0;
        else if constexpr (std::is_same_v<T, bool>) return v;
        else if constexpr (std::is_same_v<T, std::string>) return !v.empty();
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_list>>) return v && !v->elements.empty();
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_object>>) return v != nullptr;
        else if constexpr (std::is_same_v<T, native_ref>) return v.index >= 0;
        else if constexpr (std::is_same_v<T, function_ref>) return v.index >= 0;
        else if constexpr (std::is_same_v<T, std::monostate>) return false;
        else return false;
    }, val);
}

std::string value_type_name(const mv_value& val) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) return "number";
        else if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_list>>) return "list";
        else if constexpr (std::is_same_v<T, std::shared_ptr<mv_object>>) return v ? v->type_name : "object";
        else if constexpr (std::is_same_v<T, native_ref>) return "native_function";
        else if constexpr (std::is_same_v<T, function_ref>) return "function";
        else if constexpr (std::is_same_v<T, std::monostate>) return "None";
        else return "unknown";
    }, val);
}

std::string value_to_string(const mv_value& val) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "True" : "False";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<mv_list>>) {
            std::string s = "[";
            if (v) {
                for (size_t i = 0; i < v->elements.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += value_to_string(v->elements[i]);
                }
            }
            return s + "]";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<mv_object>>) {
            return v ? "<" + v->type_name + ">" : "<null>";
        } else if constexpr (std::is_same_v<T, native_ref>) {
            return "<native>";
        } else if constexpr (std::is_same_v<T, function_ref>) {
            return "<function>";
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            return "None";
        } else {
            return "?";
        }
    }, val);
}

// ---- VM implementation ----

vm::vm(compiled_program prog) : program_(std::move(prog)) {
    stack_.reserve(256);
    frames_.reserve(32);
}

void vm::set_global(const std::string& name, mv_value value) {
    globals_[name] = std::move(value);
}

void vm::register_native(const std::string& name, native_function fn) {
    int index = 0;
    auto it = native_index_by_name_.find(name);
    if (it == native_index_by_name_.end()) {
        index = static_cast<int>(native_table_.size());
        native_index_by_name_[name] = index;
        native_table_.push_back(std::move(fn));
    } else {
        index = it->second;
        native_table_[static_cast<size_t>(index)] = std::move(fn);
    }
    globals_[name] = native_ref{index, false};
}

void vm::register_native_kwargs(const std::string& name, native_kwargs_function fn) {
    int index = 0;
    auto it = native_kwargs_index_by_name_.find(name);
    if (it == native_kwargs_index_by_name_.end()) {
        index = static_cast<int>(native_kwargs_table_.size());
        native_kwargs_index_by_name_[name] = index;
        native_kwargs_table_.push_back(std::move(fn));
    } else {
        index = it->second;
        native_kwargs_table_[static_cast<size_t>(index)] = std::move(fn);
    }
    globals_[name] = native_ref{index, true};
}

void vm::set_limits(const sandbox_limits& limits) {
    limits_ = limits;
}

void vm::push(mv_value val) {
    stack_.push_back(std::move(val));
}

mv_value vm::pop() {
    mv_value val = std::move(stack_.back());
    stack_.pop_back();
    return val;
}

mv_value& vm::peek_ref(int offset) {
    return stack_[stack_.size() - 1 - offset];
}

const std::string& vm::get_string_constant(uint32_t idx) const {
    return std::get<std::string>(program_.constants[idx]);
}

std::optional<vm_error> vm::check_number_result(double val, int line) {
    if (std::isnan(val) || std::isinf(val)) {
        return vm_error{"numeric result is NaN or infinity", line};
    }
    return std::nullopt;
}

void vm::register_function_globals() {
    for (const auto& [fname, fidx] : program_.function_map) {
        if (fname != "__main__") {
            globals_[fname] = function_ref{fidx};
        }
    }
}

vm_result vm::run_top_level() {
    if (top_level_ran_) {
        return {std::monostate{}, std::nullopt, true};
    }

    auto it = program_.function_map.find("__main__");
    if (it == program_.function_map.end()) {
        return {std::monostate{}, vm_error{"no main chunk found", 0}, false};
    }

    register_function_globals();

    step_count_ = 0;
    stack_.clear();
    frames_.clear();

    call_frame frame;
    frame.chunk = &program_.functions[it->second];
    frame.ip = 0;
    frame.stack_base = 0;
    frame.locals.resize(frame.chunk->local_count);

    frames_.push_back(std::move(frame));
    auto result = execute();
    if (result.success) {
        top_level_ran_ = true;
    }
    return result;
}

vm_result vm::call_function(const std::string& name, const std::vector<mv_value>& args) {
    if (!top_level_ran_) {
        return {std::monostate{}, vm_error{"top-level not initialized", 0}, false};
    }

    auto it = program_.function_map.find(name);
    if (it == program_.function_map.end()) {
        return {std::monostate{}, vm_error{"function '" + name + "' not defined", 0}, false};
    }

    step_count_ = 0;
    stack_.clear();
    frames_.clear();

    const function_chunk& func = program_.functions[it->second];
    if (static_cast<int>(args.size()) != func.param_count) {
        return {std::monostate{}, vm_error{
            "function '" + name + "' expects " + std::to_string(func.param_count) +
            " args, got " + std::to_string(args.size()), 0}, false};
    }

    call_frame frame;
    frame.chunk = &func;
    frame.ip = 0;
    frame.stack_base = 0;
    frame.locals.resize(func.local_count);

    for (int i = 0; i < static_cast<int>(args.size()); ++i) {
        frame.locals[i] = args[i];
    }

    frames_.push_back(std::move(frame));
    return execute();
}

vm_result vm::execute() {
    while (!frames_.empty()) {
        auto& frame = frames_.back();

        if (frame.ip >= static_cast<int>(frame.chunk->code.size())) {
            frames_.pop_back();
            if (frames_.empty()) {
                mv_value v = stack_.empty() ? mv_value{std::monostate{}} : pop();
                return {std::move(v), std::nullopt, true};
            }
            push(std::monostate{});
            continue;
        }

        auto err = run_instruction(frame);
        if (err) {
            return {std::monostate{}, std::move(*err), false};
        }
    }

    mv_value v = stack_.empty() ? mv_value{std::monostate{}} : pop();
    return {std::move(v), std::nullopt, true};
}

std::optional<vm_error> vm::run_instruction(call_frame& frame) {
    if (++step_count_ > limits_.max_steps) {
        return vm_error{"execution step limit exceeded (" + std::to_string(limits_.max_steps) + ")", 0};
    }

    const instruction& instr = frame.chunk->code[frame.ip++];
    int line = instr.source_line;

    switch (instr.op) {
    case opcode::load_const:
        push(std::visit([](const auto& v) -> mv_value { return v; }, program_.constants[instr.arg]));
        break;

    case opcode::load_none: push(std::monostate{}); break;
    case opcode::load_true: push(true); break;
    case opcode::load_false: push(false); break;
    case opcode::pop: pop(); break;

    case opcode::load_local:
        if (instr.arg < frame.locals.size()) {
            push(frame.locals[instr.arg]);
        } else {
            return vm_error{"invalid local variable slot", line};
        }
        break;

    case opcode::store_local:
        if (instr.arg >= frame.locals.size()) {
            frame.locals.resize(instr.arg + 1);
        }
        frame.locals[instr.arg] = pop();
        break;

    case opcode::load_global: {
        const std::string& name = get_string_constant(instr.arg);
        auto it = globals_.find(name);
        if (it != globals_.end()) {
            push(it->second);
        } else {
            return vm_error{"undefined variable '" + name + "'", line};
        }
        break;
    }

    case opcode::store_global: {
        const std::string& name = get_string_constant(instr.arg);
        globals_[name] = pop();
        break;
    }

    case opcode::add: {
        auto b = pop();
        auto a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = *na + *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        if (auto* sa = std::get_if<std::string>(&a)) {
            if (auto* sb = std::get_if<std::string>(&b)) {
                std::string result = *sa + *sb;
                if (static_cast<int>(result.size()) > limits_.max_string_length) {
                    return vm_error{"string length exceeds limit", line};
                }
                push(std::move(result));
                break;
            }
        }
        if (auto* la = std::get_if<std::shared_ptr<mv_list>>(&a)) {
            if (auto* lb = std::get_if<std::shared_ptr<mv_list>>(&b)) {
                auto result = std::make_shared<mv_list>();
                if (*la) result->elements = (*la)->elements;
                if (*lb) {
                    result->elements.insert(result->elements.end(),
                                            (*lb)->elements.begin(), (*lb)->elements.end());
                }
                if (static_cast<int>(result->elements.size()) > limits_.max_list_size) {
                    return vm_error{"list size exceeds limit", line};
                }
                try_build_list_render_caches(*result);
                push(mv_value{result});
                break;
            }
        }
        return vm_error{"cannot add " + value_type_name(a) + " and " + value_type_name(b), line};
    }

    case opcode::sub: {
        auto b = pop();
        auto a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = *na - *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot subtract " + value_type_name(b) + " from " + value_type_name(a), line};
    }

    case opcode::mul: {
        auto b = pop();
        auto a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = *na * *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot multiply " + value_type_name(a) + " and " + value_type_name(b), line};
    }

    case opcode::div_op: {
        auto b = pop();
        auto a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                if (*nb == 0.0) return vm_error{"division by zero", line};
                double result = *na / *nb;
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot divide " + value_type_name(a) + " by " + value_type_name(b), line};
    }

    case opcode::mod: {
        auto b = pop();
        auto a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                if (*nb == 0.0) return vm_error{"modulo by zero", line};
                double result = std::fmod(*na, *nb);
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot modulo " + value_type_name(a) + " by " + value_type_name(b), line};
    }

    case opcode::power: {
        auto b = pop();
        auto a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) {
                double result = std::pow(*na, *nb);
                if (auto err = check_number_result(result, line)) return err;
                push(result);
                break;
            }
        }
        return vm_error{"cannot raise " + value_type_name(a) + " to power " + value_type_name(b), line};
    }

    case opcode::negate: {
        auto val = pop();
        if (auto* n = std::get_if<double>(&val)) {
            push(-*n);
            break;
        }
        return vm_error{"cannot negate " + value_type_name(val), line};
    }

    case opcode::cmp_eq: { auto b = pop(), a = pop(); push(a == b); break; }
    case opcode::cmp_ne: { auto b = pop(), a = pop(); push(a != b); break; }

    case opcode::cmp_lt: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na < *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }
    case opcode::cmp_gt: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na > *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }
    case opcode::cmp_le: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na <= *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }
    case opcode::cmp_ge: {
        auto b = pop(), a = pop();
        if (auto* na = std::get_if<double>(&a)) {
            if (auto* nb = std::get_if<double>(&b)) { push(*na >= *nb); break; }
        }
        return vm_error{"cannot compare " + value_type_name(a) + " and " + value_type_name(b), line};
    }

    case opcode::logical_not:
        push(!is_truthy(pop()));
        break;

    case opcode::jump:
        frame.ip = static_cast<int>(instr.arg);
        break;

    case opcode::jump_if_false:
        if (!is_truthy(peek_ref())) {
            frame.ip = static_cast<int>(instr.arg);
        }
        break;

    case opcode::jump_if_true:
        if (is_truthy(peek_ref())) {
            frame.ip = static_cast<int>(instr.arg);
        }
        break;

    case opcode::call: {
        int arg_count = static_cast<int>(instr.arg);
        mv_value callee = stack_[stack_.size() - 1 - arg_count];

        if (auto* native = std::get_if<native_ref>(&callee)) {
            std::vector<mv_value> args(arg_count);
            for (int i = arg_count - 1; i >= 0; --i) args[i] = pop();
            pop();
            if (native->kwargs) {
                if (native->index >= 0 && native->index < static_cast<int>(native_kwargs_table_.size())) {
                    std::vector<std::pair<std::string, mv_value>> empty_kwargs;
                    push(native_kwargs_table_[static_cast<size_t>(native->index)](args, empty_kwargs));
                    break;
                }
            } else if (native->index >= 0 && native->index < static_cast<int>(native_table_.size())) {
                push(native_table_[static_cast<size_t>(native->index)](args));
                break;
            }
        }

        int func_index = -1;
        if (auto* fn = std::get_if<function_ref>(&callee)) {
            func_index = fn->index;
        } else if (auto* s = std::get_if<std::string>(&callee)) {
            auto func_it = program_.function_map.find(*s);
            if (func_it != program_.function_map.end()) {
                func_index = func_it->second;
            }
        }

        if (func_index < 0 || func_index >= static_cast<int>(program_.functions.size())) {
            return vm_error{"'" + value_to_string(callee) + "' is not callable", line};
        }

        if (static_cast<int>(frames_.size()) >= limits_.max_call_depth) {
            return vm_error{"call depth limit exceeded (" + std::to_string(limits_.max_call_depth) + ")", line};
        }

        const function_chunk& func = program_.functions[static_cast<size_t>(func_index)];
        if (arg_count != func.param_count) {
            return vm_error{
                "function '" + func.name + "' expects " + std::to_string(func.param_count) +
                " args, got " + std::to_string(arg_count), line};
        }

        call_frame new_frame;
        new_frame.chunk = &func;
        new_frame.ip = 0;
        new_frame.stack_base = static_cast<int>(stack_.size()) - arg_count - 1;
        new_frame.locals.resize(func.local_count);

        for (int i = arg_count - 1; i >= 0; --i) {
            new_frame.locals[i] = pop();
        }
        pop();

        frames_.push_back(std::move(new_frame));
        break;
    }

    case opcode::return_op: {
        mv_value result = pop();
        frames_.pop_back();
        push(std::move(result));
        break;
    }

    case opcode::load_attr: {
        auto obj_val = pop();
        const std::string& attr_name = get_string_constant(instr.arg);
        if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&obj_val)) {
            if (*obj) {
                push((*obj)->get_attr(attr_name));
                break;
            }
        }
        return vm_error{"cannot access attribute '" + attr_name + "' on " + value_type_name(obj_val), line};
    }

    case opcode::store_attr: {
        auto val = pop();
        auto obj_val = pop();
        const std::string& attr_name = get_string_constant(instr.arg);
        if (auto* obj = std::get_if<std::shared_ptr<mv_object>>(&obj_val)) {
            if (*obj) {
                (*obj)->set_attr(attr_name, std::move(val));
                break;
            }
        }
        return vm_error{"cannot set attribute on " + value_type_name(obj_val), line};
    }

    case opcode::load_ctx_attr: {
        auto ctx_value = pop();
        push(load_ctx_attr_fast(ctx_value, static_cast<ctx_attr_slot>(instr.arg)));
        break;
    }

    case opcode::build_list: {
        int count = static_cast<int>(instr.arg);
        if (count > limits_.max_list_size) {
            return vm_error{"list size exceeds limit", line};
        }
        auto list = std::make_shared<mv_list>();
        list->elements.resize(count);
        for (int i = count - 1; i >= 0; --i) {
            list->elements[i] = pop();
        }
        try_build_list_render_caches(*list);
        push(std::move(list));
        break;
    }

    case opcode::append_list: {
        auto value = pop();
        auto list_val = pop();
        if (auto* list = std::get_if<std::shared_ptr<mv_list>>(&list_val)) {
            if (*list) {
                if (static_cast<int>((*list)->elements.size()) >= limits_.max_list_size) {
                    return vm_error{"list size exceeds limit", line};
                }
                (*list)->elements.push_back(value);
                update_list_caches_for_append(*(*list), value);
                push(std::monostate{});
                break;
            }
        }
        return vm_error{"append_list on non-list", line};
    }

    case opcode::load_index: {
        auto index_val = pop();
        auto object_val = pop();
        if (auto* list = std::get_if<std::shared_ptr<mv_list>>(&object_val)) {
            if (auto* index = std::get_if<double>(&index_val)) {
                int idx = static_cast<int>(*index);
                if (*list && idx >= 0 && idx < static_cast<int>((*list)->elements.size())) {
                    push((*list)->elements[static_cast<size_t>(idx)]);
                } else {
                    push(std::monostate{});
                }
                break;
            }
        }
        return vm_error{"index access on unsupported types", line};
    }

    case opcode::store_index: {
        auto value = pop();
        auto index_val = pop();
        auto object_val = pop();
        if (auto* list = std::get_if<std::shared_ptr<mv_list>>(&object_val)) {
            if (auto* index = std::get_if<double>(&index_val)) {
                int idx = static_cast<int>(*index);
                if (*list && idx >= 0) {
                    if (idx >= static_cast<int>((*list)->elements.size())) {
                        (*list)->elements.resize(static_cast<size_t>(idx) + 1);
                    }
                    (*list)->elements[static_cast<size_t>(idx)] = std::move(value);
                    (*list)->clear_cached_render_data();
                    break;
                }
            }
        }
        return vm_error{"index assignment on unsupported types", line};
    }

    case opcode::call_kwargs: {
        int positional_count = static_cast<int>(instr.arg);
        if (frame.ip >= static_cast<int>(frame.chunk->code.size()) ||
            frame.chunk->code[frame.ip].op != opcode::kwarg_count) {
            return vm_error{"internal error: expected kwarg_count after call_kwargs", line};
        }
        int kw_count = static_cast<int>(frame.chunk->code[frame.ip++].arg);

        std::vector<std::pair<std::string, mv_value>> kwargs(kw_count);
        for (int i = kw_count - 1; i >= 0; --i) {
            mv_value val = pop();
            mv_value name_val = pop();
            kwargs[i] = {std::get<std::string>(name_val), std::move(val)};
        }

        std::vector<mv_value> pos_args(positional_count);
        for (int i = positional_count - 1; i >= 0; --i) {
            pos_args[i] = pop();
        }

        mv_value callee = pop();

        if (auto* native = std::get_if<native_ref>(&callee)) {
            if (native->kwargs) {
                if (native->index >= 0 && native->index < static_cast<int>(native_kwargs_table_.size())) {
                    push(native_kwargs_table_[static_cast<size_t>(native->index)](pos_args, kwargs));
                    break;
                }
            } else if (native->index >= 0 && native->index < static_cast<int>(native_table_.size())) {
                push(native_table_[static_cast<size_t>(native->index)](pos_args));
                break;
            }
        } else if (auto* s = std::get_if<std::string>(&callee)) {
            auto native_kwargs_it = native_kwargs_index_by_name_.find(*s);
            if (native_kwargs_it != native_kwargs_index_by_name_.end()) {
                push(native_kwargs_table_[static_cast<size_t>(native_kwargs_it->second)](pos_args, kwargs));
                break;
            }
            auto native_it = native_index_by_name_.find(*s);
            if (native_it != native_index_by_name_.end()) {
                push(native_table_[static_cast<size_t>(native_it->second)](pos_args));
                break;
            }
        }

        return vm_error{"kwargs call to non-native function not supported", line};
    }

    case opcode::kwarg_count:
        return vm_error{"internal error: unexpected kwarg_count", line};

    case opcode::load_kwarg_name: {
        const std::string& name = get_string_constant(instr.arg);
        push(name);
        break;
    }

    case opcode::make_scene: {
        mv_value clear_color = pop();
        mv_value nodes = pop();
        push(mv_value{make_scene_object(nodes, std::move(clear_color))});
        break;
    }

    case opcode::make_point: {
        mv_value y = pop();
        mv_value x = pop();
        push(mv_value{make_point_object(number_or_default(x, 0.0), number_or_default(y, 0.0))});
        break;
    }

    case opcode::make_draw_rect: {
        mv_value fill = color_value_or_default(pop(), "#ffffff");
        mv_value opacity = pop();
        mv_value rotation = pop();
        mv_value h = pop();
        mv_value w = pop();
        mv_value y = pop();
        mv_value x = pop();
        push(mv_value{make_rect_object(
            number_or_default(x, 0.0),
            number_or_default(y, 0.0),
            number_or_default(w, 100.0),
            number_or_default(h, 100.0),
            number_or_default(rotation, 0.0),
            number_or_default(opacity, 1.0),
            std::move(fill)
        )});
        break;
    }

    case opcode::make_draw_line: {
        mv_value stroke = color_value_or_default(pop(), "#ffffff");
        mv_value opacity = pop();
        mv_value thickness = pop();
        mv_value y2 = pop();
        mv_value x2 = pop();
        mv_value y1 = pop();
        mv_value x1 = pop();
        push(mv_value{make_line_object(
            number_or_default(x1, 0.0),
            number_or_default(y1, 0.0),
            number_or_default(x2, 100.0),
            number_or_default(y2, 100.0),
            number_or_default(thickness, 2.0),
            number_or_default(opacity, 1.0),
            std::move(stroke)
        )});
        break;
    }

    case opcode::make_draw_text: {
        mv_value fill = color_value_or_default(pop(), "#ffffff");
        mv_value opacity = pop();
        mv_value font_size = pop();
        mv_value y = pop();
        mv_value x = pop();
        mv_value text = pop();
        push(mv_value{make_text_object(
            string_or_default(text, ""),
            number_or_default(x, 0.0),
            number_or_default(y, 0.0),
            number_or_default(font_size, 20.0),
            number_or_default(opacity, 1.0),
            std::move(fill)
        )});
        break;
    }

    case opcode::make_draw_circle: {
        mv_value fill = color_value_or_default(pop(), "#ffffff");
        mv_value opacity = pop();
        mv_value radius = pop();
        mv_value cy = pop();
        mv_value cx = pop();
        push(mv_value{make_circle_object(
            number_or_default(cx, 0.0),
            number_or_default(cy, 0.0),
            number_or_default(radius, 50.0),
            number_or_default(opacity, 1.0),
            std::move(fill)
        )});
        break;
    }

    case opcode::make_draw_polyline: {
        mv_value stroke = color_value_or_default(pop(), "#ffffff");
        mv_value opacity = pop();
        mv_value thickness = pop();
        mv_value points = pop();
        push(mv_value{make_polyline_object(
            list_or_empty(points),
            number_or_default(thickness, 2.0),
            number_or_default(opacity, 1.0),
            std::move(stroke)
        )});
        break;
    }

    case opcode::make_draw_background: {
        mv_value opacity = pop();
        mv_value fill = color_value_or_default(pop(), "#000000");
        push(mv_value{make_background_object(
            std::move(fill),
            number_or_default(opacity, 1.0)
        )});
        break;
    }
    }

    return std::nullopt;
}

} // namespace mv
