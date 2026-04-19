#include "mv_builtins.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

#include "raylib.h"

namespace mv {

namespace {

// ---- Helpers ----

double as_number(const mv_value& v, double fallback = 0.0) {
    if (auto* d = std::get_if<double>(&v)) return *d;
    return fallback;
}

double as_number(const mv_value* v, double fallback = 0.0) {
    return v ? as_number(*v, fallback) : fallback;
}

std::string as_string(const mv_value& v, const std::string& fallback = "") {
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    return fallback;
}

std::string as_string(const mv_value* v, const std::string& fallback = "") {
    return v ? as_string(*v, fallback) : fallback;
}

std::shared_ptr<mv_object> as_object(const mv_value& v) {
    if (auto* o = std::get_if<std::shared_ptr<mv_object>>(&v)) return *o;
    return nullptr;
}

std::shared_ptr<mv_object> as_object(const mv_value* v) {
    return v ? as_object(*v) : nullptr;
}

std::shared_ptr<mv_list> as_list(const mv_value& v) {
    if (auto* l = std::get_if<std::shared_ptr<mv_list>>(&v)) return *l;
    return nullptr;
}

std::shared_ptr<mv_list> as_list(const mv_value* v) {
    return v ? as_list(*v) : nullptr;
}

std::shared_ptr<mv_object> make_typed_object_for_name(const std::string& type_name) {
    if (type_name == "Scene") return std::make_shared<scene_mv_object>();
    if (type_name == "Point") return std::make_shared<point_mv_object>();
    if (type_name == "Color") return std::make_shared<color_mv_object>();
    if (type_name == "DrawRect") return std::make_shared<rect_mv_object>();
    if (type_name == "DrawLine") return std::make_shared<line_mv_object>();
    if (type_name == "DrawText") return std::make_shared<text_mv_object>();
    if (type_name == "DrawCircle") return std::make_shared<circle_mv_object>();
    if (type_name == "DrawPolyline") return std::make_shared<polyline_mv_object>();
    if (type_name == "DrawBackground") return std::make_shared<background_mv_object>();
    auto obj = std::make_shared<mv_object>();
    obj->type_name = type_name;
    return obj;
}

// Find kwarg by name
const mv_value* find_kwarg(const std::vector<std::pair<std::string, mv_value>>& kwargs,
                           const std::string& name) {
    for (auto& [k, v] : kwargs) {
        if (k == name) return &v;
    }
    return nullptr;
}

double kwarg_number(const std::vector<std::pair<std::string, mv_value>>& kwargs,
                    const std::string& name, double fallback) {
    auto* v = find_kwarg(kwargs, name);
    return v ? as_number(*v, fallback) : fallback;
}

std::string kwarg_string(const std::vector<std::pair<std::string, mv_value>>& kwargs,
                         const std::string& name, const std::string& fallback) {
    auto* v = find_kwarg(kwargs, name);
    return v ? as_string(*v, fallback) : fallback;
}

// ---- Color helpers ----

color color_from_kwarg(const std::vector<std::pair<std::string, mv_value>>& kwargs,
                       const std::string& name, color fallback) {
    auto* v = find_kwarg(kwargs, name);
    if (!v) return fallback;
    // Could be a string "#rrggbb" or an object from rgb()
    if (auto* s = std::get_if<std::string>(v)) {
        return parse_color(*s);
    }
    if (auto obj = as_object(*v)) {
        if (obj->type_name == "Color") {
            return {
                static_cast<uint8_t>(as_number(obj->get_attr("r"), 255)),
                static_cast<uint8_t>(as_number(obj->get_attr("g"), 255)),
                static_cast<uint8_t>(as_number(obj->get_attr("b"), 255)),
                static_cast<uint8_t>(as_number(obj->get_attr("a"), 255))
            };
        }
    }
    return fallback;
}

std::optional<vec2> build_cached_point_from_kwargs(
    const std::vector<std::pair<std::string, mv_value>>& kwargs) {
    return vec2{
        static_cast<float>(kwarg_number(kwargs, "x", 0.0)),
        static_cast<float>(kwarg_number(kwargs, "y", 0.0))
    };
}

std::optional<scene_node> build_cached_scene_node_from_kwargs(
    const std::string& type_name,
    const std::vector<std::pair<std::string, mv_value>>& kwargs) {
    if (type_name == "DrawRect") {
        rect_node n;
        n.x = static_cast<float>(kwarg_number(kwargs, "x", 0.0));
        n.y = static_cast<float>(kwarg_number(kwargs, "y", 0.0));
        n.w = static_cast<float>(kwarg_number(kwargs, "w", 100.0));
        n.h = static_cast<float>(kwarg_number(kwargs, "h", 100.0));
        n.rotation = static_cast<float>(kwarg_number(kwargs, "rotation", 0.0));
        n.opacity = static_cast<float>(kwarg_number(kwargs, "opacity", 1.0));
        n.fill = color_from_kwarg(kwargs, "fill", n.fill);
        return scene_node{n};
    }
    if (type_name == "DrawLine") {
        line_node n;
        n.x1 = static_cast<float>(kwarg_number(kwargs, "x1", 0.0));
        n.y1 = static_cast<float>(kwarg_number(kwargs, "y1", 0.0));
        n.x2 = static_cast<float>(kwarg_number(kwargs, "x2", 100.0));
        n.y2 = static_cast<float>(kwarg_number(kwargs, "y2", 100.0));
        n.thickness = static_cast<float>(kwarg_number(kwargs, "thickness", 2.0));
        n.opacity = static_cast<float>(kwarg_number(kwargs, "opacity", 1.0));
        n.stroke = color_from_kwarg(kwargs, "stroke", n.stroke);
        return scene_node{n};
    }
    if (type_name == "DrawText") {
        text_node n;
        n.text = kwarg_string(kwargs, "text", "");
        n.x = static_cast<float>(kwarg_number(kwargs, "x", 0.0));
        n.y = static_cast<float>(kwarg_number(kwargs, "y", 0.0));
        n.font_size = static_cast<int>(kwarg_number(kwargs, "font_size", 20.0));
        n.opacity = static_cast<float>(kwarg_number(kwargs, "opacity", 1.0));
        n.fill = color_from_kwarg(kwargs, "fill", n.fill);
        return scene_node{n};
    }
    if (type_name == "DrawCircle") {
        circle_node n;
        n.cx = static_cast<float>(kwarg_number(kwargs, "cx", 0.0));
        n.cy = static_cast<float>(kwarg_number(kwargs, "cy", 0.0));
        n.radius = static_cast<float>(kwarg_number(kwargs, "radius", 50.0));
        n.opacity = static_cast<float>(kwarg_number(kwargs, "opacity", 1.0));
        n.fill = color_from_kwarg(kwargs, "fill", n.fill);
        return scene_node{n};
    }
    if (type_name == "DrawPolyline") {
        polyline_node n;
        n.thickness = static_cast<float>(kwarg_number(kwargs, "thickness", 2.0));
        n.opacity = static_cast<float>(kwarg_number(kwargs, "opacity", 1.0));
        n.stroke = color_from_kwarg(kwargs, "stroke", n.stroke);
        if (const mv_value* points_val = find_kwarg(kwargs, "points")) {
            if (auto points = as_list(*points_val)) {
                n.points.reserve(points->elements.size());
                for (const auto& point_val : points->elements) {
                    auto point_obj = as_object(point_val);
                    if (!point_obj || point_obj->type_name != "Point") {
                        continue;
                    }
                    if (point_obj->cached_point.has_value()) {
                        n.points.push_back(*point_obj->cached_point);
                    } else {
                        n.points.push_back({
                            static_cast<float>(as_number(point_obj->find_attr("x"))),
                            static_cast<float>(as_number(point_obj->find_attr("y")))
                        });
                    }
                }
            }
        }
        return scene_node{n};
    }
    if (type_name == "DrawBackground") {
        background_node n;
        n.opacity = static_cast<float>(kwarg_number(kwargs, "opacity", 1.0));
        n.fill = color_from_kwarg(kwargs, "fill", n.fill);
        return scene_node{n};
    }
    return std::nullopt;
}

// ---- Node extraction helpers ----

rect_node extract_rect(const std::shared_ptr<mv_object>& obj) {
    rect_node n;
    n.x = static_cast<float>(as_number(obj->find_attr("x")));
    n.y = static_cast<float>(as_number(obj->find_attr("y")));
    n.w = static_cast<float>(as_number(obj->find_attr("w"), 100));
    n.h = static_cast<float>(as_number(obj->find_attr("h"), 100));
    n.rotation = static_cast<float>(as_number(obj->find_attr("rotation")));
    n.opacity = static_cast<float>(as_number(obj->find_attr("opacity"), 1.0));
    // fill
    if (const mv_value* fill_val = obj->find_attr("fill")) {
        if (auto* s = std::get_if<std::string>(fill_val)) n.fill = parse_color(*s);
        else if (auto fo = as_object(fill_val)) {
            if (fo->type_name == "Color") {
                n.fill = {
                    static_cast<uint8_t>(as_number(fo->find_attr("r"), 255)),
                    static_cast<uint8_t>(as_number(fo->find_attr("g"), 255)),
                    static_cast<uint8_t>(as_number(fo->find_attr("b"), 255)),
                    static_cast<uint8_t>(as_number(fo->find_attr("a"), 255))
                };
            }
        }
    }
    return n;
}

line_node extract_line(const std::shared_ptr<mv_object>& obj) {
    line_node n;
    n.x1 = static_cast<float>(as_number(obj->find_attr("x1")));
    n.y1 = static_cast<float>(as_number(obj->find_attr("y1")));
    n.x2 = static_cast<float>(as_number(obj->find_attr("x2"), 100));
    n.y2 = static_cast<float>(as_number(obj->find_attr("y2"), 100));
    n.thickness = static_cast<float>(as_number(obj->find_attr("thickness"), 2));
    n.opacity = static_cast<float>(as_number(obj->find_attr("opacity"), 1.0));
    if (const mv_value* stroke_val = obj->find_attr("stroke"); stroke_val != nullptr) {
    if (auto* s = std::get_if<std::string>(stroke_val)) n.stroke = parse_color(*s);
    else if (auto so = as_object(stroke_val)) {
        if (so->type_name == "Color") {
            n.stroke = {
                static_cast<uint8_t>(as_number(so->find_attr("r"), 255)),
                static_cast<uint8_t>(as_number(so->find_attr("g"), 255)),
                static_cast<uint8_t>(as_number(so->find_attr("b"), 255)),
                static_cast<uint8_t>(as_number(so->find_attr("a"), 255))
            };
        }
    }
    }
    return n;
}

text_node extract_text(const std::shared_ptr<mv_object>& obj) {
    text_node n;
    n.text = as_string(obj->find_attr("text"));
    n.x = static_cast<float>(as_number(obj->find_attr("x")));
    n.y = static_cast<float>(as_number(obj->find_attr("y")));
    n.font_size = static_cast<int>(as_number(obj->find_attr("font_size"), 20));
    n.opacity = static_cast<float>(as_number(obj->find_attr("opacity"), 1.0));
    if (const mv_value* fill_val = obj->find_attr("fill"); fill_val != nullptr) {
    if (auto* s = std::get_if<std::string>(fill_val)) n.fill = parse_color(*s);
    else if (auto fo = as_object(fill_val)) {
        if (fo->type_name == "Color") {
            n.fill = {
                static_cast<uint8_t>(as_number(fo->find_attr("r"), 255)),
                static_cast<uint8_t>(as_number(fo->find_attr("g"), 255)),
                static_cast<uint8_t>(as_number(fo->find_attr("b"), 255)),
                static_cast<uint8_t>(as_number(fo->find_attr("a"), 255))
            };
        }
    }
    }
    return n;
}

circle_node extract_circle(const std::shared_ptr<mv_object>& obj) {
    circle_node n;
    n.cx = static_cast<float>(as_number(obj->find_attr("cx")));
    n.cy = static_cast<float>(as_number(obj->find_attr("cy")));
    n.radius = static_cast<float>(as_number(obj->find_attr("radius"), 50));
    n.opacity = static_cast<float>(as_number(obj->find_attr("opacity"), 1.0));
    if (const mv_value* fill_val = obj->find_attr("fill"); fill_val != nullptr) {
    if (auto* s = std::get_if<std::string>(fill_val)) n.fill = parse_color(*s);
    else if (auto fo = as_object(fill_val)) {
        if (fo->type_name == "Color") {
            n.fill = {
                static_cast<uint8_t>(as_number(fo->find_attr("r"), 255)),
                static_cast<uint8_t>(as_number(fo->find_attr("g"), 255)),
                static_cast<uint8_t>(as_number(fo->find_attr("b"), 255)),
                static_cast<uint8_t>(as_number(fo->find_attr("a"), 255))
            };
        }
    }
    }
    return n;
}

polyline_node extract_polyline(const std::shared_ptr<mv_object>& obj) {
    polyline_node n;
    n.thickness = static_cast<float>(as_number(obj->find_attr("thickness"), 2));
    n.opacity = static_cast<float>(as_number(obj->find_attr("opacity"), 1.0));
    if (const mv_value* stroke_val = obj->find_attr("stroke"); stroke_val != nullptr) {
    if (auto* s = std::get_if<std::string>(stroke_val)) n.stroke = parse_color(*s);
    else if (auto so = as_object(stroke_val)) {
        if (so->type_name == "Color") {
            n.stroke = {
                static_cast<uint8_t>(as_number(so->find_attr("r"), 255)),
                static_cast<uint8_t>(as_number(so->find_attr("g"), 255)),
                static_cast<uint8_t>(as_number(so->find_attr("b"), 255)),
                static_cast<uint8_t>(as_number(so->find_attr("a"), 255))
            };
        }
    }
    }

    if (auto points = as_list(obj->find_attr("points"))) {
        n.points.reserve(points->elements.size());
        for (const auto& point_val : points->elements) {
            auto point_obj = as_object(point_val);
            if (!point_obj || point_obj->type_name != "Point") {
                continue;
            }
            if (point_obj->cached_point.has_value()) {
                n.points.push_back(*point_obj->cached_point);
            } else {
                n.points.push_back({
                    static_cast<float>(as_number(point_obj->find_attr("x"))),
                    static_cast<float>(as_number(point_obj->find_attr("y")))
                });
            }
        }
    }

    return n;
}

spectrum_bar_node extract_spectrum_bar(const std::shared_ptr<mv_object>& obj) {
    spectrum_bar_node n;
    n.x = static_cast<float>(as_number(obj->get_attr("x")));
    n.y = static_cast<float>(as_number(obj->get_attr("y")));
    n.w = static_cast<float>(as_number(obj->get_attr("w"), 400));
    n.h = static_cast<float>(as_number(obj->get_attr("h"), 200));
    n.bar_count = static_cast<int>(as_number(obj->get_attr("bar_count"), 32));
    n.opacity = static_cast<float>(as_number(obj->get_attr("opacity"), 1.0));
    auto fill_val = obj->get_attr("fill");
    if (auto* s = std::get_if<std::string>(&fill_val)) n.fill = parse_color(*s);
    else if (auto fo = as_object(fill_val)) {
        if (fo->type_name == "Color") {
            n.fill = {
                static_cast<uint8_t>(as_number(fo->get_attr("r"), 255)),
                static_cast<uint8_t>(as_number(fo->get_attr("g"), 255)),
                static_cast<uint8_t>(as_number(fo->get_attr("b"), 255)),
                static_cast<uint8_t>(as_number(fo->get_attr("a"), 255))
            };
        }
    }
    // spectrum data is injected by runtime, not user
    return n;
}

beat_grid_node extract_beat_grid(const std::shared_ptr<mv_object>& obj) {
    beat_grid_node n;
    n.x = static_cast<float>(as_number(obj->get_attr("x")));
    n.y = static_cast<float>(as_number(obj->get_attr("y")));
    n.w = static_cast<float>(as_number(obj->get_attr("w"), 1920));
    n.h = static_cast<float>(as_number(obj->get_attr("h"), 1080));
    n.thickness = static_cast<float>(as_number(obj->get_attr("thickness"), 1));
    n.beat_phase = static_cast<float>(as_number(obj->get_attr("beat_phase")));
    n.opacity = static_cast<float>(as_number(obj->get_attr("opacity"), 1.0));
    auto stroke_val = obj->get_attr("stroke");
    if (auto* s = std::get_if<std::string>(&stroke_val)) n.stroke = parse_color(*s);
    else if (auto so = as_object(stroke_val)) {
        if (so->type_name == "Color") {
            n.stroke = {
                static_cast<uint8_t>(as_number(so->get_attr("r"), 255)),
                static_cast<uint8_t>(as_number(so->get_attr("g"), 255)),
                static_cast<uint8_t>(as_number(so->get_attr("b"), 255)),
                static_cast<uint8_t>(as_number(so->get_attr("a"), 255))
            };
        }
    }
    return n;
}

pulse_ring_node extract_pulse_ring(const std::shared_ptr<mv_object>& obj) {
    pulse_ring_node n;
    n.cx = static_cast<float>(as_number(obj->get_attr("cx"), 640));
    n.cy = static_cast<float>(as_number(obj->get_attr("cy"), 360));
    n.radius = static_cast<float>(as_number(obj->get_attr("radius"), 100));
    n.thickness = static_cast<float>(as_number(obj->get_attr("thickness"), 3));
    n.beat_phase = static_cast<float>(as_number(obj->get_attr("beat_phase")));
    n.opacity = static_cast<float>(as_number(obj->get_attr("opacity"), 1.0));
    auto stroke_val = obj->get_attr("stroke");
    if (auto* s = std::get_if<std::string>(&stroke_val)) n.stroke = parse_color(*s);
    else if (auto so = as_object(stroke_val)) {
        if (so->type_name == "Color") {
            n.stroke = {
                static_cast<uint8_t>(as_number(so->get_attr("r"), 255)),
                static_cast<uint8_t>(as_number(so->get_attr("g"), 255)),
                static_cast<uint8_t>(as_number(so->get_attr("b"), 255)),
                static_cast<uint8_t>(as_number(so->get_attr("a"), 255))
            };
        }
    }
    return n;
}

background_node extract_background(const std::shared_ptr<mv_object>& obj) {
    background_node n;
    n.opacity = static_cast<float>(as_number(obj->get_attr("opacity"), 1.0));
    auto fill_val = obj->get_attr("fill");
    if (auto* s = std::get_if<std::string>(&fill_val)) n.fill = parse_color(*s);
    else if (auto fo = as_object(fill_val)) {
        if (fo->type_name == "Color") {
            n.fill = {
                static_cast<uint8_t>(as_number(fo->get_attr("r"), 255)),
                static_cast<uint8_t>(as_number(fo->get_attr("g"), 255)),
                static_cast<uint8_t>(as_number(fo->get_attr("b"), 255)),
                static_cast<uint8_t>(as_number(fo->get_attr("a"), 255))
            };
        }
    }
    return n;
}

// Convert a node mv_object to a scene_node
std::optional<scene_node> convert_node(const mv_value& val) {
    auto obj = as_object(val);
    if (!obj) return std::nullopt;
    if (obj->cached_scene_node.has_value()) {
        return *obj->cached_scene_node;
    }

    switch (obj->kind) {
    case mv_object_kind::draw_rect: return extract_rect(obj);
    case mv_object_kind::draw_line: return extract_line(obj);
    case mv_object_kind::draw_text: return extract_text(obj);
    case mv_object_kind::draw_circle: return extract_circle(obj);
    case mv_object_kind::draw_polyline: return extract_polyline(obj);
    case mv_object_kind::draw_background: return extract_background(obj);
    default: break;
    }

    const auto& type = obj->type_name;
    if (type == "DrawRect") return extract_rect(obj);
    if (type == "DrawLine") return extract_line(obj);
    if (type == "DrawText") return extract_text(obj);
    if (type == "DrawCircle") return extract_circle(obj);
    if (type == "DrawPolyline") return extract_polyline(obj);
    if (type == "DrawBackground") return extract_background(obj);
    return std::nullopt;
}

// ---- Native function builders ----

// Helper: create a node object from kwargs, setting all kwargs as attrs
mv_value make_node_kwargs(const std::string& type_name,
                          const std::vector<mv_value>& /*args*/,
                          const std::vector<std::pair<std::string, mv_value>>& kwargs) {
    auto obj = make_typed_object_for_name(type_name);
    obj->reserve_attrs(kwargs.size() + 2);
    for (auto& [k, v] : kwargs) {
        obj->set_attr(k, v);
    }
    if (type_name == "Point") {
        if (auto point = build_cached_point_from_kwargs(kwargs)) {
            obj->set_cached_point(*point);
        }
    } else if (auto cached_node = build_cached_scene_node_from_kwargs(type_name, kwargs)) {
        obj->set_cached_scene_node(std::move(*cached_node));
    }
    return mv_value{obj};
}

} // anonymous namespace

// ---- Color parsing ----

color parse_color(const std::string& hex) {
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

    color c;
    if (hex.size() >= 7) {
        c.r = parse_byte(1);
        c.g = parse_byte(3);
        c.b = parse_byte(5);
    }
    c.a = (hex.size() >= 9) ? parse_byte(7) : 255;
    return c;
}

// ---- Register builtins (generic, works with vm or sandbox) ----

template<typename Host>
void register_builtins_impl(Host& host) {
    // Math functions
    host.register_native("sin", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::sin(as_number(args[0]));
    });
    host.register_native("cos", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::cos(as_number(args[0]));
    });
    host.register_native("abs", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::abs(as_number(args[0]));
    });
    host.register_native("floor", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::floor(as_number(args[0]));
    });
    host.register_native("ceil", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::ceil(as_number(args[0]));
    });
    host.register_native("sqrt", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::sqrt(as_number(args[0]));
    });
    host.register_native("min", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.size() < 2) return 0.0;
        return std::min(as_number(args[0]), as_number(args[1]));
    });
    host.register_native("max", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.size() < 2) return 0.0;
        return std::max(as_number(args[0]), as_number(args[1]));
    });
    host.register_native("clamp", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.size() < 3) return 0.0;
        double val = as_number(args[0]);
        double lo = as_number(args[1]);
        double hi = as_number(args[2]);
        return std::clamp(val, lo, hi);
    });
    host.register_native("lerp", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.size() < 3) return 0.0;
        double a = as_number(args[0]);
        double b = as_number(args[1]);
        double t = as_number(args[2]);
        return a + (b - a) * t;
    });
    host.register_native("smoothstep", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.size() < 3) return 0.0;
        double edge0 = as_number(args[0]);
        double edge1 = as_number(args[1]);
        double x = as_number(args[2]);
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    });

    // Color constructor: rgb(r, g, b) or rgba(r, g, b, a)
    host.register_native("rgb", [](const std::vector<mv_value>& args) -> mv_value {
        auto obj = std::make_shared<color_mv_object>();
        obj->r = args.size() > 0 ? as_number(args[0]) : 255.0;
        obj->g = args.size() > 1 ? as_number(args[1]) : 255.0;
        obj->b = args.size() > 2 ? as_number(args[2]) : 255.0;
        obj->a = args.size() > 3 ? as_number(args[3]) : 255.0;
        return mv_value{obj};
    });

    // Utility
    host.register_native("len", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        if (auto l = as_list(args[0])) return static_cast<double>(l->elements.size());
        if (auto* s = std::get_if<std::string>(&args[0])) return static_cast<double>(s->size());
        return 0.0;
    });
    host.register_native("range", [](const std::vector<mv_value>& args) -> mv_value {
        auto list = std::make_shared<mv_list>();
        int start = 0, end = 0, step = 1;
        if (args.size() == 1) { end = static_cast<int>(as_number(args[0])); }
        else if (args.size() >= 2) {
            start = static_cast<int>(as_number(args[0]));
            end = static_cast<int>(as_number(args[1]));
            if (args.size() >= 3) step = static_cast<int>(as_number(args[2]));
        }
        if (step == 0) step = 1;
        if (step > 0) {
            for (int i = start; i < end && list->elements.size() < 1024; i += step)
                list->elements.push_back(static_cast<double>(i));
        } else {
            for (int i = start; i > end && list->elements.size() < 1024; i += step)
                list->elements.push_back(static_cast<double>(i));
        }
        return mv_value{list};
    });
    host.register_native("str", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return std::string("");
        return mv_value{value_to_string(args[0])};
    });
    host.register_native("int", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return std::floor(as_number(args[0]));
    });
    host.register_native("float", [](const std::vector<mv_value>& args) -> mv_value {
        if (args.empty()) return 0.0;
        return as_number(args[0]);
    });

    // Pi constant via function
    host.register_native("pi", [](const std::vector<mv_value>&) -> mv_value {
        return 3.14159265358979323846;
    });

    // ---- Scene/Node constructors (kwargs) ----

    host.register_native_kwargs("Scene", [](const std::vector<mv_value>& args,
                                          const std::vector<std::pair<std::string, mv_value>>& kwargs) -> mv_value {
        auto obj = std::make_shared<scene_mv_object>();
        // nodes from first positional arg (list) or default empty
        if (!args.empty()) {
            obj->set_attr("nodes", args[0]);
        } else {
            auto* v = find_kwarg(kwargs, "nodes");
            if (v) obj->set_attr("nodes", *v);
            else obj->set_attr("nodes", mv_value{std::make_shared<mv_list>()});
        }
        // clear_color
        auto cc = kwarg_string(kwargs, "clear_color", "");
        if (!cc.empty()) obj->set_attr("clear_color", mv_value{cc});
        auto* cc_obj = find_kwarg(kwargs, "clear_color");
        if (cc_obj && std::holds_alternative<std::shared_ptr<mv_object>>(*cc_obj)) {
            obj->set_attr("clear_color", *cc_obj);
        }
        return mv_value{obj};
    });

    // Node constructors: all kwargs
    auto make_node_ctor = [](const std::string& type_name) -> native_kwargs_function {
        return [type_name](const std::vector<mv_value>& args,
                          const std::vector<std::pair<std::string, mv_value>>& kwargs) -> mv_value {
            return make_node_kwargs(type_name, args, kwargs);
        };
    };

    host.register_native_kwargs("Point", make_node_ctor("Point"));
    host.register_native_kwargs("DrawRect", make_node_ctor("DrawRect"));
    host.register_native_kwargs("DrawLine", make_node_ctor("DrawLine"));
    host.register_native_kwargs("DrawText", make_node_ctor("DrawText"));
    host.register_native_kwargs("DrawCircle", make_node_ctor("DrawCircle"));
    host.register_native_kwargs("DrawPolyline", make_node_ctor("DrawPolyline"));
    host.register_native_kwargs("DrawBackground", make_node_ctor("DrawBackground"));
}

void register_builtins(vm& v) { register_builtins_impl(v); }
void register_builtins_to_sandbox(sandbox& sb) { register_builtins_impl(sb); }

// ---- extract_scene ----

bool extract_scene_into(const mv_value& val, scene& sc) {
    auto obj = as_object(val);
    if (!obj || (obj->kind != mv_object_kind::scene && obj->type_name != "Scene")) return false;

    sc.clear_color = {0, 0, 0, 0};
    sc.nodes.clear();

    // clear_color
    auto cc_val = obj->get_attr("clear_color");
    if (auto* s = std::get_if<std::string>(&cc_val)) {
        sc.clear_color = parse_color(*s);
    } else if (auto cc_obj = as_object(cc_val)) {
        if (cc_obj->type_name == "Color") {
            sc.clear_color = {
                static_cast<uint8_t>(as_number(cc_obj->get_attr("r"), 0)),
                static_cast<uint8_t>(as_number(cc_obj->get_attr("g"), 0)),
                static_cast<uint8_t>(as_number(cc_obj->get_attr("b"), 0)),
                static_cast<uint8_t>(as_number(cc_obj->get_attr("a"), 0))
            };
        }
    }

    // nodes
    auto nodes_val = obj->get_attr("nodes");
    auto nodes_list = as_list(nodes_val);
    if (nodes_list) {
        if (nodes_list->cached_scene_nodes.has_value()) {
            sc.nodes = *nodes_list->cached_scene_nodes;
        } else {
            sc.nodes.reserve(nodes_list->elements.size());
            for (auto& elem : nodes_list->elements) {
                auto node = convert_node(elem);
                if (node.has_value()) {
                    sc.nodes.push_back(std::move(*node));
                }
            }
        }
    }

    return true;
}

std::optional<scene> extract_scene(const mv_value& val) {
    scene sc;
    if (!extract_scene_into(val, sc)) {
        return std::nullopt;
    }
    return sc;
}

} // namespace mv
