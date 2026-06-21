#include "mv/composition/mv_composition_serializer.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <sstream>

#include <nlohmann/json.hpp>

#include "updater/update_verify.h"

namespace mv::composition {
namespace {

using json = nlohmann::json;

constexpr const char* kExpectedFormat = "raythm.mv.composition";
constexpr int kSupportedFormatVersion = 1;

double sane_number(double value, double fallback = 0.0) {
    return std::isfinite(value) ? value : fallback;
}

std::string get_string(const json& object, const char* key, const std::string& fallback = {}) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return fallback;
    }
    return it->get<std::string>();
}

double get_number(const json& object, const char* key, double fallback = 0.0) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return fallback;
    }
    return sane_number(it->get<double>(), fallback);
}

float get_float(const json& object, const char* key, float fallback = 0.0f) {
    return static_cast<float>(get_number(object, key, fallback));
}

bool get_bool(const json& object, const char* key, bool fallback = false) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_boolean()) {
        return fallback;
    }
    return it->get<bool>();
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
    return normalized.find("../") == std::string::npos &&
           normalized.rfind("..", 0) != 0;
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
    if (const auto it = object.find("position"); it != object.end() && it->is_array() && it->size() >= 2) {
        result.position_x = static_cast<float>(array_number_at(*it, 0, result.position_x));
        result.position_y = static_cast<float>(array_number_at(*it, 1, result.position_y));
    }
    if (const auto it = object.find("scale"); it != object.end() && it->is_array() && it->size() >= 2) {
        result.scale_x = static_cast<float>(array_number_at(*it, 0, result.scale_x));
        result.scale_y = static_cast<float>(array_number_at(*it, 1, result.scale_y));
    }
    if (const auto it = object.find("anchor"); it != object.end() && it->is_array() && it->size() >= 2) {
        result.anchor_x = static_cast<float>(array_number_at(*it, 0, result.anchor_x));
        result.anchor_y = static_cast<float>(array_number_at(*it, 1, result.anchor_y));
    }
    result.rotation_deg = get_float(object, "rotationDeg", result.rotation_deg);
    result.opacity = get_float(object, "opacity", result.opacity);
    return result;
}

json source_to_json(const source& value) {
    json result = {
        {"type", value.type},
    };
    if (!value.shape.empty()) {
        result["shape"] = value.shape;
    }
    if (!value.text.empty()) {
        result["text"] = value.text;
    }
    if (!value.fill.empty()) {
        result["fill"] = value.fill;
    }
    if (!value.asset_id.empty()) {
        result["assetId"] = value.asset_id;
    }
    return result;
}

source parse_source(const json& object) {
    source result;
    if (!object.is_object()) {
        return result;
    }
    result.type = get_string(object, "type", result.type);
    result.shape = get_string(object, "shape");
    result.text = get_string(object, "text");
    result.fill = get_string(object, "fill", result.fill);
    result.asset_id = get_string(object, "assetId");
    return result;
}

json layer_to_json(const layer& value) {
    json effects = json::array();
    for (const effect& current : value.effects) {
        effects.push_back({
            {"id", current.id},
            {"type", current.type},
            {"target", current.target},
            {"amount", current.amount},
        });
    }

    json keyframes = json::array();
    for (const keyframe_track& current : value.keyframes) {
        json points = json::array();
        for (const keyframe& point : current.points) {
            points.push_back({
                {"timeMs", point.time_ms},
                {"value", point.value},
                {"easing", point.easing.empty() ? "linear" : point.easing},
            });
        }
        keyframes.push_back({
            {"target", current.target},
            {"points", points},
        });
    }

    json triggers = json::array();
    for (const event_trigger& current : value.event_triggers) {
        json actions = json::array();
        for (const event_action& action : current.actions) {
            json action_json = {{"type", action.type}};
            if (!action.effect_id.empty()) {
                action_json["effectId"] = action.effect_id;
            }
            if (!action.property.empty()) {
                action_json["property"] = action.property;
            }
            if (!action.value.empty()) {
                action_json["value"] = action.value;
            }
            actions.push_back(action_json);
        }
        json trigger_json = {
            {"event", current.event},
            {"actions", actions},
        };
        if (current.time_ms >= 0.0) {
            trigger_json["timeMs"] = current.time_ms;
        }
        triggers.push_back(trigger_json);
    }

    return {
        {"id", value.id},
        {"name", value.name},
        {"visible", value.visible},
        {"locked", value.locked},
        {"z", value.z},
        {"startMs", value.start_ms},
        {"durationMs", value.duration_ms},
        {"source", source_to_json(value.source_data)},
        {"transform", transform_to_json(value.transform_data)},
        {"effects", effects},
        {"keyframes", keyframes},
        {"eventTriggers", triggers},
    };
}

layer parse_layer(const json& object) {
    layer result;
    result.id = get_string(object, "id");
    result.name = get_string(object, "name");
    result.visible = get_bool(object, "visible", true);
    result.locked = get_bool(object, "locked", false);
    result.z = static_cast<int>(get_number(object, "z", 0.0));
    result.start_ms = get_number(object, "startMs", 0.0);
    result.duration_ms = get_number(object, "durationMs", 0.0);
    if (const auto it = object.find("source"); it != object.end()) {
        result.source_data = parse_source(*it);
    }
    if (const auto it = object.find("transform"); it != object.end()) {
        result.transform_data = parse_transform(*it);
    }
    if (const auto it = object.find("effects"); it != object.end() && it->is_array()) {
        for (const json& item : *it) {
            if (!item.is_object()) {
                continue;
            }
            effect effect_item;
            effect_item.id = get_string(item, "id");
            effect_item.type = get_string(item, "type");
            effect_item.target = get_string(item, "target");
            effect_item.amount = get_float(item, "amount");
            result.effects.push_back(std::move(effect_item));
        }
    }
    if (const auto it = object.find("keyframes"); it != object.end() && it->is_array()) {
        for (const json& item : *it) {
            if (item.is_object()) {
                keyframe_track track;
                track.target = get_string(item, "target");
                if (const auto points = item.find("points"); points != item.end() && points->is_array()) {
                    for (const json& point_json : *points) {
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
                result.keyframes.push_back(std::move(track));
            }
        }
    }
    if (const auto it = object.find("eventTriggers"); it != object.end() && it->is_array()) {
        for (const json& item : *it) {
            if (!item.is_object()) {
                continue;
            }
            event_trigger trigger;
            trigger.event = get_string(item, "event");
            trigger.time_ms = get_number(item, "timeMs", -1.0);
            if (const auto actions = item.find("actions"); actions != item.end() && actions->is_array()) {
                for (const json& action_json : *actions) {
                    if (!action_json.is_object()) {
                        continue;
                    }
                    trigger.actions.push_back({
                        get_string(action_json, "type"),
                        get_string(action_json, "effectId"),
                        get_string(action_json, "property"),
                        get_string(action_json, "value"),
                    });
                }
            }
            result.event_triggers.push_back(std::move(trigger));
        }
    }
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
    for (const asset_ref& asset : value.assets) {
        if (!asset.id.empty()) {
            const auto [_, inserted] = asset_ids.insert(asset.id);
            if (!inserted) {
                errors.push_back("Asset id must be unique.");
            }
        }
    }
    std::unordered_set<std::string> layer_ids;
    for (const layer& current : value.layers) {
        if (current.id.empty()) {
            errors.push_back("Layer is missing id.");
        } else {
            const auto [_, inserted] = layer_ids.insert(current.id);
            if (!inserted) {
                errors.push_back("Layer id must be unique.");
            }
        }
        if (current.name.empty()) {
            errors.push_back("Layer is missing name.");
        }
        if (current.duration_ms < 0.0) {
            errors.push_back("Layer durationMs must not be negative.");
        }
        if (current.source_data.type == "image") {
            if (current.source_data.asset_id.empty()) {
                errors.push_back("Image layer is missing assetId.");
            } else if (asset_ids.find(current.source_data.asset_id) == asset_ids.end()) {
                errors.push_back("Image layer references an unknown assetId.");
            }
        }
        std::unordered_set<std::string> effect_ids;
        for (const effect& current_effect : current.effects) {
            if (current_effect.id.empty()) {
                errors.push_back("Effect is missing id.");
            } else {
                const auto [_, inserted] = effect_ids.insert(current_effect.id);
                if (!inserted) {
                    errors.push_back("Effect id must be unique within a layer.");
                }
            }
            if (current_effect.type.empty()) {
                errors.push_back("Effect is missing type.");
            }
        }
        for (const keyframe_track& track : current.keyframes) {
            if (track.target.empty()) {
                errors.push_back("Keyframe track is missing target.");
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
        for (const event_trigger& trigger : current.event_triggers) {
            if (trigger.event.empty() && trigger.time_ms < 0.0) {
                errors.push_back("EventTrigger must have event or timeMs.");
            }
            if (trigger.time_ms < 0.0 && trigger.time_ms != -1.0) {
                errors.push_back("EventTrigger timeMs must be non-negative when present.");
            }
            for (const event_action& action : trigger.actions) {
                if (action.type.empty()) {
                    errors.push_back("EventTrigger action is missing type.");
                }
                if (action.type == "triggerEffect") {
                    if (action.effect_id.empty()) {
                        errors.push_back("triggerEffect action is missing effectId.");
                    } else if (effect_ids.find(action.effect_id) == effect_ids.end()) {
                        errors.push_back("triggerEffect action references an unknown effectId.");
                    }
                }
                if (action.type == "setProperty" && action.property.empty()) {
                    errors.push_back("setProperty action is missing property.");
                }
            }
        }
    }
    for (const asset_ref& asset : value.assets) {
        if (asset.id.empty()) {
            errors.push_back("Asset is missing id.");
        }
        if (asset.type.empty()) {
            errors.push_back("Asset is missing type.");
        }
        if (!is_relative_asset_path(asset.path)) {
            errors.push_back("Asset path must be package-relative.");
        }
    }
    return errors;
}

}  // namespace

std::string serialize(const mv_composition& composition) {
    json layers = json::array();
    for (const layer& current : composition.layers) {
        layers.push_back(layer_to_json(current));
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
        {"layers", layers},
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

    if (const auto layers = root.find("layers"); layers != root.end() && layers->is_array()) {
        for (const json& item : *layers) {
            if (item.is_object()) {
                result.composition.layers.push_back(parse_layer(item));
            }
        }
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
