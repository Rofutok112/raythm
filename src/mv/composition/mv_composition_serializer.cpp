#include "mv/composition/mv_composition_serializer.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "mv/composition/mv_component_registry.h"
#include "updater/update_verify.h"

namespace mv::composition {
namespace {

using json = nlohmann::json;

constexpr const char* kExpectedFormat = "raythm.mv.composition";
constexpr int kSupportedFormatVersion = 3;

double sane_number(double value, double fallback = 0.0) {
    return std::isfinite(value) ? value : fallback;
}

std::string get_string(const json& object, const char* key, const std::string& fallback = {}) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : fallback;
}

double get_number(const json& object, const char* key, double fallback = 0.0) {
    const auto it = object.find(key);
    return it != object.end() && it->is_number() ? sane_number(it->get<double>(), fallback) : fallback;
}

float get_float(const json& object, const char* key, float fallback = 0.0f) {
    return static_cast<float>(get_number(object, key, fallback));
}

bool get_bool(const json& object, const char* key, bool fallback = false) {
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

bool is_relative_asset_path(const std::string& path) {
    if (path.empty() || path[0] == '/' || path[0] == '\\') {
        return false;
    }
    if (path.size() >= 2 && path[1] == ':') {
        return false;
    }
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized.find("../") == std::string::npos && normalized.rfind("..", 0) != 0;
}

double array_number_at(const json& array, std::size_t index, double fallback) {
    if (!array.is_array() || index >= array.size() || !array[index].is_number()) {
        return fallback;
    }
    return sane_number(array[index].get<double>(), fallback);
}

json transform_to_json(const transform& value) {
    return {
        {"position", {value.position_x, value.position_y}},
        {"scale", {value.scale_x, value.scale_y}},
        {"rotationDeg", value.rotation_deg},
        {"anchor", {value.anchor_x, value.anchor_y}},
        {"opacity", value.opacity},
    };
}

transform parse_transform(const json& object) {
    transform result;
    if (!object.is_object()) {
        return result;
    }
    if (const auto it = object.find("position"); it != object.end()) {
        result.position_x = static_cast<float>(array_number_at(*it, 0, result.position_x));
        result.position_y = static_cast<float>(array_number_at(*it, 1, result.position_y));
    }
    if (const auto it = object.find("scale"); it != object.end()) {
        result.scale_x = static_cast<float>(array_number_at(*it, 0, result.scale_x));
        result.scale_y = static_cast<float>(array_number_at(*it, 1, result.scale_y));
    }
    if (const auto it = object.find("anchor"); it != object.end()) {
        result.anchor_x = static_cast<float>(array_number_at(*it, 0, result.anchor_x));
        result.anchor_y = static_cast<float>(array_number_at(*it, 1, result.anchor_y));
    }
    result.rotation_deg = get_float(object, "rotationDeg", result.rotation_deg);
    result.opacity = get_float(object, "opacity", result.opacity);
    return result;
}

json component_props_to_json(const component& value) {
    json props = json::object();
    if (!value.shape.empty()) {
        props["shape"] = value.shape;
    }
    if (!value.text.empty()) {
        props["text"] = value.text;
    }
    if (!value.fill.empty()) {
        props["fill"] = value.fill;
    }
    if (!value.asset_id.empty()) {
        props["assetId"] = value.asset_id;
    }
    if (!value.target.empty()) {
        props["target"] = value.target;
    }
    if (!value.script_asset_id.empty()) {
        props["scriptAssetId"] = value.script_asset_id;
    }
    if (!value.script_source.empty()) {
        props["script"] = value.script_source;
    }
    if (!value.script_entry.empty()) {
        props["entry"] = value.script_entry;
    }
    if (value.amount != 0.0f) {
        props["amount"] = value.amount;
    }
    return props;
}

json component_to_json(const component& value) {
    json result = {
        {"id", value.id},
        {"type", canonical_component_type(value.type)},
        {"enabled", value.enabled},
        {"props", component_props_to_json(value)},
    };
    return result;
}

component parse_component(const json& object) {
    component result;
    if (!object.is_object()) {
        return result;
    }
    result.id = get_string(object, "id");
    result.type = canonical_component_type(get_string(object, "type", result.type));
    result.enabled = get_bool(object, "enabled", true);

    const json* props = &object;
    if (const auto it = object.find("props"); it != object.end() && it->is_object()) {
        props = &*it;
    }
    result.shape = get_string(*props, "shape");
    result.text = get_string(*props, "text");
    result.fill = get_string(*props, "fill", result.fill);
    result.asset_id = get_string(*props, "assetId");
    result.target = get_string(*props, "target");
    result.script_asset_id = get_string(*props, "scriptAssetId");
    result.script_source = get_string(*props, "script");
    result.script_entry = get_string(*props, "entry");
    result.amount = get_float(*props, "amount");
    return result;
}

json keyframes_to_json(const std::vector<keyframe_track>& value, const std::string& fallback_object_id = {}) {
    json tracks = json::array();
    for (const keyframe_track& current : value) {
        json points = json::array();
        for (const keyframe& point : current.points) {
            points.push_back({
                {"timeMs", point.time_ms},
                {"value", point.value},
                {"easing", point.easing.empty() ? "linear" : point.easing},
            });
        }
        json track = {
            {"id", current.id},
            {"objectId", current.object_id.empty() ? fallback_object_id : current.object_id},
            {"componentId", current.component_id},
            {"target", current.target},
            {"valueType", current.value_type.empty() ? "number" : current.value_type},
            {"interpolation", current.interpolation.empty() ? "linear" : current.interpolation},
            {"keys", points},
        };
        tracks.push_back(std::move(track));
    }
    return tracks;
}

std::vector<keyframe_track> parse_keyframes(const json& value) {
    std::vector<keyframe_track> result;
    if (!value.is_array()) {
        return result;
    }
    for (const json& item : value) {
        if (!item.is_object()) {
            continue;
        }
        keyframe_track track;
        track.id = get_string(item, "id");
        track.object_id = get_string(item, "objectId");
        track.component_id = get_string(item, "componentId");
        track.target = get_string(item, "target");
        track.value_type = get_string(item, "valueType", "number");
        track.interpolation = get_string(item, "interpolation", "linear");
        const json* keys = nullptr;
        if (const auto it = item.find("keys"); it != item.end() && it->is_array()) {
            keys = &*it;
        } else if (const auto it = item.find("points"); it != item.end() && it->is_array()) {
            keys = &*it;
        }
        if (keys != nullptr) {
            for (const json& point_json : *keys) {
                if (!point_json.is_object()) {
                    continue;
                }
                track.points.push_back({
                    get_number(point_json, "timeMs", 0.0),
                    get_float(point_json, "value", 0.0f),
                    get_string(point_json, "easing", "linear"),
                });
            }
        }
        result.push_back(std::move(track));
    }
    return result;
}

json object_to_json(const object& value) {
    json components = json::array();
    for (const component& current : value.components) {
        if (canonical_component_type(current.type) != "transform") {
            components.push_back(component_to_json(current));
        }
    }

    const component* legacy_transform = transform_component(value);
    const transform object_transform = legacy_transform == nullptr
        ? value.transform_data
        : transform_from_component(*legacy_transform);

    return {
        {"id", value.id},
        {"name", value.name},
        {"active", value.active},
        {"visible", value.visible},
        {"locked", value.locked},
        {"order", value.order},
        {"timeline", {
            {"startMs", value.start_ms},
            {"durationMs", value.duration_ms},
        }},
        {"transform", transform_to_json(object_transform)},
        {"components", components},
    };
}

object parse_object(const json& value) {
    object result;
    result.id = get_string(value, "id");
    result.name = get_string(value, "name");
    result.active = get_bool(value, "active", true);
    result.visible = get_bool(value, "visible", true);
    result.locked = get_bool(value, "locked", false);
    result.order = static_cast<int>(get_number(value, "order", get_number(value, "z", 0.0)));
    if (const auto timeline = value.find("timeline"); timeline != value.end() && timeline->is_object()) {
        result.start_ms = get_number(*timeline, "startMs", 0.0);
        result.duration_ms = get_number(*timeline, "durationMs", 0.0);
    } else {
        result.start_ms = get_number(value, "startMs", 0.0);
        result.duration_ms = get_number(value, "durationMs", 0.0);
    }
    if (const auto transform_json = value.find("transform"); transform_json != value.end()) {
        result.transform_data = parse_transform(*transform_json);
    }
    result.components.push_back(component_from_transform(result.transform_data));
    if (const auto components = value.find("components"); components != value.end() && components->is_array()) {
        for (const json& item : *components) {
            component parsed = parse_component(item);
            if (canonical_component_type(parsed.type) != "transform") {
                result.components.push_back(std::move(parsed));
            }
        }
    }
    if (const auto legacy_keyframes = value.find("keyframes"); legacy_keyframes != value.end()) {
        result.keyframes = parse_keyframes(*legacy_keyframes);
        for (keyframe_track& track : result.keyframes) {
            if (track.object_id.empty()) {
                track.object_id = result.id;
            }
        }
    }
    return result;
}

void sync_object_keyframes_from_root(mv_composition& value) {
    for (object& current : value.objects) {
        current.keyframes.clear();
    }
    for (const keyframe_track& track : value.animation_tracks) {
        auto it = std::find_if(value.objects.begin(), value.objects.end(), [&](const object& current) {
            return current.id == track.object_id;
        });
        if (it != value.objects.end()) {
            it->keyframes.push_back(track);
        }
    }
}

std::vector<keyframe_track> collect_animation_tracks(const mv_composition& value) {
    std::vector<keyframe_track> result;
    for (const object& current : value.objects) {
        for (keyframe_track track : current.keyframes) {
            if (track.object_id.empty()) {
                track.object_id = current.id;
            }
            result.push_back(std::move(track));
        }
    }
    if (!result.empty()) {
        return result;
    }
    result = value.animation_tracks;
    return result;
}

std::vector<std::string> validate(const mv_composition& value) {
    std::vector<std::string> errors;
    if (value.format != kExpectedFormat) {
        errors.push_back("Unsupported MV composition format.");
    }
    if (value.format_version != kSupportedFormatVersion) {
        errors.push_back("Unsupported MV composition formatVersion.");
    }
    if (value.composition_id.empty()) {
        errors.push_back("Missing compositionId.");
    }
    if (value.canvas_data.width <= 0 || value.canvas_data.height <= 0) {
        errors.push_back("Canvas size must be positive.");
    }

    std::unordered_set<std::string> asset_ids;
    std::unordered_set<std::string> script_asset_ids;
    for (const asset_ref& asset : value.assets) {
        if (asset.id.empty()) {
            errors.push_back("Asset is missing id.");
        } else if (!asset_ids.insert(asset.id).second) {
            errors.push_back("Asset id must be unique.");
        } else if (asset.type == "script") {
            script_asset_ids.insert(asset.id);
        }
        if (asset.type.empty()) {
            errors.push_back("Asset is missing type.");
        }
        if (!is_relative_asset_path(asset.path)) {
            errors.push_back("Asset path must be package-relative.");
        }
    }

    std::unordered_set<std::string> object_ids;
    for (const object& current : value.objects) {
        if (current.id.empty()) {
            errors.push_back("Object is missing id.");
        } else if (!object_ids.insert(current.id).second) {
            errors.push_back("Object id must be unique.");
        }
        if (current.name.empty()) {
            errors.push_back("Object is missing name.");
        }
        if (current.duration_ms < 0.0) {
            errors.push_back("Object durationMs must not be negative.");
        }

        std::unordered_set<std::string> component_ids;
        std::unordered_set<std::string> single_component_types;
        int renderer_count = 0;
        for (const component& component_item : current.components) {
            const std::string type = canonical_component_type(component_item.type);
            if (type == "transform") {
                continue;
            }
            if (component_item.id.empty()) {
                errors.push_back("Component is missing id.");
            } else if (!component_ids.insert(component_item.id).second) {
                errors.push_back("Component id must be unique within an object.");
            }
            if (category_for_component_type(type) == component_category::unknown) {
                errors.push_back("Component has unknown type.");
            }
            if (const component_definition* definition = find_component_definition(type);
                definition != nullptr && definition->single_per_object &&
                !single_component_types.insert(definition->type).second) {
                errors.push_back(definition->type + " must be unique within an object.");
            }
            if (is_renderer_component_type(type)) {
                ++renderer_count;
            }
            if (type == "ImageRenderer") {
                if (component_item.asset_id.empty()) {
                    errors.push_back("ImageRenderer is missing assetId.");
                } else if (asset_ids.find(component_item.asset_id) == asset_ids.end()) {
                    errors.push_back("ImageRenderer references an unknown assetId.");
                }
            }
            if (type == "LuaBehaviour" && !component_item.script_asset_id.empty() &&
                script_asset_ids.find(component_item.script_asset_id) == script_asset_ids.end()) {
                errors.push_back("LuaBehaviour references an unknown scriptAssetId.");
            }
        }
        if (renderer_count > 1) {
            errors.push_back("Object must not have more than one Renderer component.");
        }
    }

    for (const keyframe_track& track : collect_animation_tracks(value)) {
        if (track.object_id.empty()) {
            errors.push_back("Animation track is missing objectId.");
        } else if (object_ids.find(track.object_id) == object_ids.end()) {
            errors.push_back("Animation track references an unknown objectId.");
        }
        if (track.target.empty()) {
            errors.push_back("Animation track is missing target.");
        }
        double previous_time = -1.0;
        for (const keyframe& point : track.points) {
            if (point.time_ms < 0.0) {
                errors.push_back("Keyframe timeMs must not be negative.");
            }
            if (point.time_ms < previous_time) {
                errors.push_back("Keyframe points must be sorted by timeMs.");
            }
            previous_time = point.time_ms;
        }
    }
    return errors;
}

}  // namespace

std::string serialize(const mv_composition& composition) {
    json objects = json::array();
    for (const object& current : composition.objects) {
        objects.push_back(object_to_json(current));
    }

    json assets = json::array();
    for (const asset_ref& current : composition.assets) {
        assets.push_back({
            {"id", current.id},
            {"type", current.type},
            {"path", current.path},
            {"sha256", current.sha256},
        });
    }

    json root = {
        {"format", composition.format},
        {"formatVersion", composition.format_version},
        {"compositionId", composition.composition_id},
        {"canvas", {
            {"width", composition.canvas_data.width},
            {"height", composition.canvas_data.height},
            {"background", composition.canvas_data.background},
        }},
        {"durationMs", composition.duration_ms},
        {"objects", objects},
        {"animationTracks", keyframes_to_json(collect_animation_tracks(composition))},
        {"assets", assets},
    };
    return root.dump(2) + "\n";
}

parse_result parse(const std::string& text) {
    parse_result result;
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        result.errors.push_back("MV composition is not valid JSON.");
        return result;
    }

    result.composition.format = get_string(root, "format");
    result.composition.format_version = static_cast<int>(get_number(root, "formatVersion", 0.0));
    result.composition.composition_id = get_string(root, "compositionId");
    if (const auto canvas = root.find("canvas"); canvas != root.end() && canvas->is_object()) {
        result.composition.canvas_data.width = static_cast<int>(get_number(*canvas, "width", 1920.0));
        result.composition.canvas_data.height = static_cast<int>(get_number(*canvas, "height", 1080.0));
        result.composition.canvas_data.background = get_string(*canvas, "background", "#101216");
    }
    result.composition.duration_ms = get_number(root, "durationMs", 0.0);

    const json* objects_json = nullptr;
    if (const auto objects = root.find("objects"); objects != root.end() && objects->is_array()) {
        objects_json = &*objects;
    } else if (const auto legacy_layers = root.find("layers"); legacy_layers != root.end() && legacy_layers->is_array()) {
        objects_json = &*legacy_layers;
    }
    if (objects_json != nullptr) {
        for (const json& item : *objects_json) {
            if (item.is_object()) {
                result.composition.objects.push_back(parse_object(item));
            }
        }
    }

    if (const auto tracks = root.find("animationTracks"); tracks != root.end()) {
        result.composition.animation_tracks = parse_keyframes(*tracks);
        sync_object_keyframes_from_root(result.composition);
    } else {
        result.composition.animation_tracks = collect_animation_tracks(result.composition);
    }

    if (const auto assets = root.find("assets"); assets != root.end() && assets->is_array()) {
        for (const json& item : *assets) {
            if (!item.is_object()) {
                continue;
            }
            result.composition.assets.push_back({
                get_string(item, "id"),
                get_string(item, "type"),
                get_string(item, "path"),
                get_string(item, "sha256"),
            });
        }
    }

    result.errors = validate(result.composition);
    result.success = result.errors.empty();
    return result;
}

std::string fingerprint(const mv_composition& composition) {
    return updater::compute_sha256_hex(std::string_view(serialize(composition)));
}

}  // namespace mv::composition
