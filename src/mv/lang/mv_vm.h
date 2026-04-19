#pragma once

#include "mv_bytecode.h"
#include "../api/mv_scene.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mv {

// Forward declarations to break recursive type
struct mv_list;
struct mv_object;

struct native_ref {
    int index = -1;
    bool kwargs = false;
    bool operator==(const native_ref&) const = default;
};

struct function_ref {
    int index = -1;
    bool operator==(const function_ref&) const = default;
};

using mv_value = std::variant<
    double,                         // number
    bool,                           // bool
    std::string,                    // string
    std::shared_ptr<mv_list>,       // list
    std::shared_ptr<mv_object>,     // object (for ctx, Scene, Node, etc.)
    native_ref,                     // native callable
    function_ref,                   // script callable
    std::monostate                  // None
>;

struct mv_list {
    std::vector<mv_value> elements;
    std::optional<std::vector<vec2>> cached_points;
    std::optional<std::vector<scene_node>> cached_scene_nodes;

    void clear_cached_render_data() {
        cached_points.reset();
        cached_scene_nodes.reset();
    }
};

enum class mv_object_kind {
    generic,
    ctx_root,
    ctx_time,
    ctx_audio,
    ctx_audio_analysis,
    ctx_audio_bands,
    ctx_audio_buffers,
    ctx_song,
    ctx_chart,
    ctx_screen,
    scene,
    point,
    color,
    draw_rect,
    draw_line,
    draw_text,
    draw_circle,
    draw_polyline,
    draw_background,
};

namespace detail {

inline double value_as_number(const mv_value& value, double fallback = 0.0) {
    if (const auto* number = std::get_if<double>(&value)) return *number;
    return fallback;
}

inline bool value_as_bool(const mv_value& value, bool fallback = false) {
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean;
    return fallback;
}

inline std::string value_as_string(const mv_value& value, const std::string& fallback = "") {
    if (const auto* text = std::get_if<std::string>(&value)) return *text;
    return fallback;
}

inline std::shared_ptr<mv_list> value_as_list(const mv_value& value) {
    if (const auto* list = std::get_if<std::shared_ptr<mv_list>>(&value)) return *list;
    return nullptr;
}

inline std::shared_ptr<mv_object> value_as_object(const mv_value& value) {
    if (const auto* object = std::get_if<std::shared_ptr<mv_object>>(&value)) return *object;
    return nullptr;
}

} // namespace detail

struct mv_object {
    explicit mv_object(mv_object_kind object_kind = mv_object_kind::generic, std::string object_type = {})
        : kind(object_kind), type_name(std::move(object_type)) {}

    virtual ~mv_object() = default;

    mv_object_kind kind = mv_object_kind::generic;
    std::string type_name;
    std::optional<scene_node> cached_scene_node;
    std::optional<vec2> cached_point;

    mv_value get_attr(const std::string& name) const {
        if (auto known = get_known_attr(name)) return *known;
        auto it = extra_attrs_.find(name);
        if (it != extra_attrs_.end()) return it->second;
        return std::monostate{};
    }

    const mv_value* find_attr(const std::string& name) const {
        if (auto known = get_known_attr(name)) {
            lookup_cache_ = std::move(*known);
            return &*lookup_cache_;
        }
        auto it = extra_attrs_.find(name);
        if (it != extra_attrs_.end()) return &it->second;
        lookup_cache_.reset();
        return nullptr;
    }

    void reserve_attrs(std::size_t capacity) {
        extra_attrs_.reserve(capacity);
    }

    void clear_cached_render_data() {
        cached_scene_node.reset();
        cached_point.reset();
    }

    void set_attr(const std::string& name, mv_value val) {
        if (!set_known_attr(name, std::move(val))) {
            extra_attrs_[name] = std::move(val);
        }
        clear_cached_render_data();
    }

    void set_cached_scene_node(scene_node node) {
        cached_scene_node = std::move(node);
    }

    void set_cached_point(vec2 point) {
        cached_point = point;
    }

protected:
    virtual std::optional<mv_value> get_known_attr(std::string_view name) const {
        (void)name;
        return std::nullopt;
    }

    virtual bool set_known_attr(std::string_view name, mv_value&& value) {
        (void)name;
        (void)value;
        return false;
    }

private:
    std::unordered_map<std::string, mv_value> extra_attrs_;
    mutable std::optional<mv_value> lookup_cache_;
};

struct ctx_root_object final : mv_object {
    ctx_root_object() : mv_object(mv_object_kind::ctx_root, "ctx") {}

    std::shared_ptr<mv_object> time;
    std::shared_ptr<mv_object> audio;
    std::shared_ptr<mv_object> song;
    std::shared_ptr<mv_object> chart;
    std::shared_ptr<mv_object> screen;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "time") return mv_value{time};
        if (name == "audio") return mv_value{audio};
        if (name == "song") return mv_value{song};
        if (name == "chart") return mv_value{chart};
        if (name == "screen") return mv_value{screen};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "time") {
            time = detail::value_as_object(value);
            return true;
        }
        if (name == "audio") {
            audio = detail::value_as_object(value);
            return true;
        }
        if (name == "song") {
            song = detail::value_as_object(value);
            return true;
        }
        if (name == "chart") {
            chart = detail::value_as_object(value);
            return true;
        }
        if (name == "screen") {
            screen = detail::value_as_object(value);
            return true;
        }
        return false;
    }
};

struct ctx_time_object final : mv_object {
    ctx_time_object() : mv_object(mv_object_kind::ctx_time, "time") {}

    double ms = 0.0;
    double sec = 0.0;
    double length_ms = 0.0;
    double bpm = 120.0;
    double beat = 0.0;
    double beat_phase = 0.0;
    double meter_numerator = 4.0;
    double meter_denominator = 4.0;
    double progress = 0.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "ms") return mv_value{ms};
        if (name == "sec") return mv_value{sec};
        if (name == "length_ms") return mv_value{length_ms};
        if (name == "bpm") return mv_value{bpm};
        if (name == "beat") return mv_value{beat};
        if (name == "beat_phase") return mv_value{beat_phase};
        if (name == "meter_numerator") return mv_value{meter_numerator};
        if (name == "meter_denominator") return mv_value{meter_denominator};
        if (name == "progress") return mv_value{progress};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "ms") { ms = detail::value_as_number(value); return true; }
        if (name == "sec") { sec = detail::value_as_number(value); return true; }
        if (name == "length_ms") { length_ms = detail::value_as_number(value); return true; }
        if (name == "bpm") { bpm = detail::value_as_number(value); return true; }
        if (name == "beat") { beat = detail::value_as_number(value); return true; }
        if (name == "beat_phase") { beat_phase = detail::value_as_number(value); return true; }
        if (name == "meter_numerator") { meter_numerator = detail::value_as_number(value); return true; }
        if (name == "meter_denominator") { meter_denominator = detail::value_as_number(value); return true; }
        if (name == "progress") { progress = detail::value_as_number(value); return true; }
        return false;
    }
};

struct ctx_audio_analysis_object final : mv_object {
    ctx_audio_analysis_object() : mv_object(mv_object_kind::ctx_audio_analysis, "audio_analysis") {}

    double level = 0.0;
    double rms = 0.0;
    double peak = 0.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "level") return mv_value{level};
        if (name == "rms") return mv_value{rms};
        if (name == "peak") return mv_value{peak};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "level") { level = detail::value_as_number(value); return true; }
        if (name == "rms") { rms = detail::value_as_number(value); return true; }
        if (name == "peak") { peak = detail::value_as_number(value); return true; }
        return false;
    }
};

struct ctx_audio_bands_object final : mv_object {
    ctx_audio_bands_object() : mv_object(mv_object_kind::ctx_audio_bands, "audio_bands") {}

    double low = 0.0;
    double mid = 0.0;
    double high = 0.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "low") return mv_value{low};
        if (name == "mid") return mv_value{mid};
        if (name == "high") return mv_value{high};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "low") { low = detail::value_as_number(value); return true; }
        if (name == "mid") { mid = detail::value_as_number(value); return true; }
        if (name == "high") { high = detail::value_as_number(value); return true; }
        return false;
    }
};

struct ctx_audio_buffers_object final : mv_object {
    ctx_audio_buffers_object() : mv_object(mv_object_kind::ctx_audio_buffers, "audio_buffers") {}

    std::shared_ptr<mv_list> spectrum;
    std::shared_ptr<mv_list> waveform;
    std::shared_ptr<mv_list> oscilloscope;
    double spectrum_size = 0.0;
    double waveform_size = 0.0;
    double waveform_index = 0.0;
    double oscilloscope_size = 0.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "spectrum") return mv_value{spectrum};
        if (name == "waveform") return mv_value{waveform};
        if (name == "oscilloscope") return mv_value{oscilloscope};
        if (name == "spectrum_size") return mv_value{spectrum_size};
        if (name == "waveform_size") return mv_value{waveform_size};
        if (name == "waveform_index") return mv_value{waveform_index};
        if (name == "oscilloscope_size") return mv_value{oscilloscope_size};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "spectrum") { spectrum = detail::value_as_list(value); return true; }
        if (name == "waveform") { waveform = detail::value_as_list(value); return true; }
        if (name == "oscilloscope") { oscilloscope = detail::value_as_list(value); return true; }
        if (name == "spectrum_size") { spectrum_size = detail::value_as_number(value); return true; }
        if (name == "waveform_size") { waveform_size = detail::value_as_number(value); return true; }
        if (name == "waveform_index") { waveform_index = detail::value_as_number(value); return true; }
        if (name == "oscilloscope_size") { oscilloscope_size = detail::value_as_number(value); return true; }
        return false;
    }
};

struct ctx_audio_object final : mv_object {
    ctx_audio_object() : mv_object(mv_object_kind::ctx_audio, "audio") {}

    std::shared_ptr<mv_object> analysis;
    std::shared_ptr<mv_object> bands;
    std::shared_ptr<mv_object> buffers;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "analysis") return mv_value{analysis};
        if (name == "bands") return mv_value{bands};
        if (name == "buffers") return mv_value{buffers};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "analysis") { analysis = detail::value_as_object(value); return true; }
        if (name == "bands") { bands = detail::value_as_object(value); return true; }
        if (name == "buffers") { buffers = detail::value_as_object(value); return true; }
        return false;
    }
};

struct ctx_song_object final : mv_object {
    ctx_song_object() : mv_object(mv_object_kind::ctx_song, "song") {}

    std::string song_id;
    std::string title;
    std::string artist;
    double base_bpm = 0.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "song_id") return mv_value{song_id};
        if (name == "title") return mv_value{title};
        if (name == "artist") return mv_value{artist};
        if (name == "base_bpm") return mv_value{base_bpm};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "song_id") { song_id = detail::value_as_string(value); return true; }
        if (name == "title") { title = detail::value_as_string(value); return true; }
        if (name == "artist") { artist = detail::value_as_string(value); return true; }
        if (name == "base_bpm") { base_bpm = detail::value_as_number(value); return true; }
        return false;
    }
};

struct ctx_chart_object final : mv_object {
    ctx_chart_object() : mv_object(mv_object_kind::ctx_chart, "chart") {}

    std::string chart_id;
    std::string song_id;
    std::string difficulty;
    double level = 0.0;
    std::string chart_author;
    double resolution = 0.0;
    double offset = 0.0;
    double total_notes = 0.0;
    double combo = 0.0;
    double accuracy = 0.0;
    double key_count = 4.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "chart_id") return mv_value{chart_id};
        if (name == "song_id") return mv_value{song_id};
        if (name == "difficulty") return mv_value{difficulty};
        if (name == "level") return mv_value{level};
        if (name == "chart_author") return mv_value{chart_author};
        if (name == "resolution") return mv_value{resolution};
        if (name == "offset") return mv_value{offset};
        if (name == "total_notes") return mv_value{total_notes};
        if (name == "combo") return mv_value{combo};
        if (name == "accuracy") return mv_value{accuracy};
        if (name == "key_count") return mv_value{key_count};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "chart_id") { chart_id = detail::value_as_string(value); return true; }
        if (name == "song_id") { song_id = detail::value_as_string(value); return true; }
        if (name == "difficulty") { difficulty = detail::value_as_string(value); return true; }
        if (name == "level") { level = detail::value_as_number(value); return true; }
        if (name == "chart_author") { chart_author = detail::value_as_string(value); return true; }
        if (name == "resolution") { resolution = detail::value_as_number(value); return true; }
        if (name == "offset") { offset = detail::value_as_number(value); return true; }
        if (name == "total_notes") { total_notes = detail::value_as_number(value); return true; }
        if (name == "combo") { combo = detail::value_as_number(value); return true; }
        if (name == "accuracy") { accuracy = detail::value_as_number(value); return true; }
        if (name == "key_count") { key_count = detail::value_as_number(value); return true; }
        return false;
    }
};

struct ctx_screen_object final : mv_object {
    ctx_screen_object() : mv_object(mv_object_kind::ctx_screen, "screen") {}

    double w = 1920.0;
    double h = 1080.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "w") return mv_value{w};
        if (name == "h") return mv_value{h};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "w") { w = detail::value_as_number(value); return true; }
        if (name == "h") { h = detail::value_as_number(value); return true; }
        return false;
    }
};

struct scene_mv_object final : mv_object {
    scene_mv_object()
        : mv_object(mv_object_kind::scene, "Scene"),
          nodes(std::make_shared<mv_list>()) {}

    std::shared_ptr<mv_list> nodes;
    mv_value clear_color = std::monostate{};

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "nodes") return mv_value{nodes};
        if (name == "clear_color") return clear_color;
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "nodes") {
            if (auto list = detail::value_as_list(value)) nodes = std::move(list);
            else nodes = std::make_shared<mv_list>();
            return true;
        }
        if (name == "clear_color") {
            clear_color = std::move(value);
            return true;
        }
        return false;
    }
};

struct point_mv_object final : mv_object {
    point_mv_object() : mv_object(mv_object_kind::point, "Point") {}

    double x = 0.0;
    double y = 0.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "x") return mv_value{x};
        if (name == "y") return mv_value{y};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "x") { x = detail::value_as_number(value); return true; }
        if (name == "y") { y = detail::value_as_number(value); return true; }
        return false;
    }
};

struct color_mv_object final : mv_object {
    color_mv_object() : mv_object(mv_object_kind::color, "Color") {}

    double r = 255.0;
    double g = 255.0;
    double b = 255.0;
    double a = 255.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "r") return mv_value{r};
        if (name == "g") return mv_value{g};
        if (name == "b") return mv_value{b};
        if (name == "a") return mv_value{a};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "r") { r = detail::value_as_number(value, 255.0); return true; }
        if (name == "g") { g = detail::value_as_number(value, 255.0); return true; }
        if (name == "b") { b = detail::value_as_number(value, 255.0); return true; }
        if (name == "a") { a = detail::value_as_number(value, 255.0); return true; }
        return false;
    }
};

struct rect_mv_object final : mv_object {
    rect_mv_object() : mv_object(mv_object_kind::draw_rect, "DrawRect") {}

    double x = 0.0;
    double y = 0.0;
    double w = 100.0;
    double h = 100.0;
    mv_value fill = std::string("#ffffff");
    double rotation = 0.0;
    double opacity = 1.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "x") return mv_value{x};
        if (name == "y") return mv_value{y};
        if (name == "w") return mv_value{w};
        if (name == "h") return mv_value{h};
        if (name == "fill") return fill;
        if (name == "rotation") return mv_value{rotation};
        if (name == "opacity") return mv_value{opacity};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "x") { x = detail::value_as_number(value); return true; }
        if (name == "y") { y = detail::value_as_number(value); return true; }
        if (name == "w") { w = detail::value_as_number(value, 100.0); return true; }
        if (name == "h") { h = detail::value_as_number(value, 100.0); return true; }
        if (name == "fill") { fill = std::move(value); return true; }
        if (name == "rotation") { rotation = detail::value_as_number(value); return true; }
        if (name == "opacity") { opacity = detail::value_as_number(value, 1.0); return true; }
        return false;
    }
};

struct line_mv_object final : mv_object {
    line_mv_object() : mv_object(mv_object_kind::draw_line, "DrawLine") {}

    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 100.0;
    double y2 = 100.0;
    mv_value stroke = std::string("#ffffff");
    double thickness = 2.0;
    double opacity = 1.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "x1") return mv_value{x1};
        if (name == "y1") return mv_value{y1};
        if (name == "x2") return mv_value{x2};
        if (name == "y2") return mv_value{y2};
        if (name == "stroke") return stroke;
        if (name == "thickness") return mv_value{thickness};
        if (name == "opacity") return mv_value{opacity};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "x1") { x1 = detail::value_as_number(value); return true; }
        if (name == "y1") { y1 = detail::value_as_number(value); return true; }
        if (name == "x2") { x2 = detail::value_as_number(value, 100.0); return true; }
        if (name == "y2") { y2 = detail::value_as_number(value, 100.0); return true; }
        if (name == "stroke") { stroke = std::move(value); return true; }
        if (name == "thickness") { thickness = detail::value_as_number(value, 2.0); return true; }
        if (name == "opacity") { opacity = detail::value_as_number(value, 1.0); return true; }
        return false;
    }
};

struct text_mv_object final : mv_object {
    text_mv_object() : mv_object(mv_object_kind::draw_text, "DrawText") {}

    std::string text;
    double x = 0.0;
    double y = 0.0;
    double font_size = 20.0;
    mv_value fill = std::string("#ffffff");
    double opacity = 1.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "text") return mv_value{text};
        if (name == "x") return mv_value{x};
        if (name == "y") return mv_value{y};
        if (name == "font_size") return mv_value{font_size};
        if (name == "fill") return fill;
        if (name == "opacity") return mv_value{opacity};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "text") { text = detail::value_as_string(value); return true; }
        if (name == "x") { x = detail::value_as_number(value); return true; }
        if (name == "y") { y = detail::value_as_number(value); return true; }
        if (name == "font_size") { font_size = detail::value_as_number(value, 20.0); return true; }
        if (name == "fill") { fill = std::move(value); return true; }
        if (name == "opacity") { opacity = detail::value_as_number(value, 1.0); return true; }
        return false;
    }
};

struct circle_mv_object final : mv_object {
    circle_mv_object() : mv_object(mv_object_kind::draw_circle, "DrawCircle") {}

    double cx = 0.0;
    double cy = 0.0;
    double radius = 50.0;
    mv_value fill = std::string("#ffffff");
    double opacity = 1.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "cx") return mv_value{cx};
        if (name == "cy") return mv_value{cy};
        if (name == "radius") return mv_value{radius};
        if (name == "fill") return fill;
        if (name == "opacity") return mv_value{opacity};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "cx") { cx = detail::value_as_number(value); return true; }
        if (name == "cy") { cy = detail::value_as_number(value); return true; }
        if (name == "radius") { radius = detail::value_as_number(value, 50.0); return true; }
        if (name == "fill") { fill = std::move(value); return true; }
        if (name == "opacity") { opacity = detail::value_as_number(value, 1.0); return true; }
        return false;
    }
};

struct polyline_mv_object final : mv_object {
    polyline_mv_object()
        : mv_object(mv_object_kind::draw_polyline, "DrawPolyline"),
          points(std::make_shared<mv_list>()) {}

    std::shared_ptr<mv_list> points;
    mv_value stroke = std::string("#ffffff");
    double thickness = 2.0;
    double opacity = 1.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "points") return mv_value{points};
        if (name == "stroke") return stroke;
        if (name == "thickness") return mv_value{thickness};
        if (name == "opacity") return mv_value{opacity};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "points") {
            if (auto list = detail::value_as_list(value)) points = std::move(list);
            else points = std::make_shared<mv_list>();
            return true;
        }
        if (name == "stroke") { stroke = std::move(value); return true; }
        if (name == "thickness") { thickness = detail::value_as_number(value, 2.0); return true; }
        if (name == "opacity") { opacity = detail::value_as_number(value, 1.0); return true; }
        return false;
    }
};

struct background_mv_object final : mv_object {
    background_mv_object() : mv_object(mv_object_kind::draw_background, "DrawBackground") {}

    mv_value fill = std::string("#000000");
    double opacity = 1.0;

protected:
    std::optional<mv_value> get_known_attr(std::string_view name) const override {
        if (name == "fill") return fill;
        if (name == "opacity") return mv_value{opacity};
        return std::nullopt;
    }

    bool set_known_attr(std::string_view name, mv_value&& value) override {
        if (name == "fill") { fill = std::move(value); return true; }
        if (name == "opacity") { opacity = detail::value_as_number(value, 1.0); return true; }
        return false;
    }
};

using native_function = std::function<mv_value(const std::vector<mv_value>&)>;

using native_kwargs_function = std::function<mv_value(
    const std::vector<mv_value>&,
    const std::vector<std::pair<std::string, mv_value>>&
)>;

struct vm_error {
    std::string message;
    int line = 0;
};

struct sandbox_limits {
    int max_steps = 1000000;
    int max_call_depth = 32;
    int max_string_length = 1024;
    int max_list_size = 1024;
};

struct vm_result {
    mv_value value;
    std::optional<vm_error> error;
    bool success = true;
};

class vm {
public:
    explicit vm(compiled_program prog);

    void set_global(const std::string& name, mv_value value);
    void register_native(const std::string& name, native_function fn);
    void register_native_kwargs(const std::string& name, native_kwargs_function fn);
    void set_limits(const sandbox_limits& limits);

    vm_result run_top_level();
    vm_result call_function(const std::string& name, const std::vector<mv_value>& args);

private:
    struct call_frame {
        const function_chunk* chunk = nullptr;
        int ip = 0;
        int stack_base = 0;
        std::vector<mv_value> locals;
    };

    compiled_program program_;
    std::vector<mv_value> stack_;
    std::vector<call_frame> frames_;
    std::unordered_map<std::string, mv_value> globals_;
    std::vector<native_function> native_table_;
    std::vector<native_kwargs_function> native_kwargs_table_;
    std::unordered_map<std::string, int> native_index_by_name_;
    std::unordered_map<std::string, int> native_kwargs_index_by_name_;
    sandbox_limits limits_;
    int step_count_ = 0;
    bool top_level_ran_ = false;

    vm_result execute();
    std::optional<vm_error> run_instruction(call_frame& frame);
    void push(mv_value val);
    mv_value pop();
    mv_value& peek_ref(int offset = 0);
    const std::string& get_string_constant(uint32_t idx) const;
    std::optional<vm_error> check_number_result(double val, int line);
    void register_function_globals();
};

// Helpers
bool is_truthy(const mv_value& val);
std::string value_type_name(const mv_value& val);
std::string value_to_string(const mv_value& val);

} // namespace mv
