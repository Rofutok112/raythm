#include "mv/composition/mv_component_registry.h"

#include <algorithm>
#include <utility>

namespace mv::composition {
namespace {

component make_renderer_defaults(std::string type, std::string fill = {}) {
    component result;
    result.type = std::move(type);
    result.fill = std::move(fill);
    return result;
}

component make_modifier_defaults(std::string type, std::string target, float amount) {
    component result;
    result.type = std::move(type);
    result.target = std::move(target);
    result.amount = amount;
    return result;
}

component make_lua_defaults() {
    component result;
    result.type = "LuaBehaviour";
    result.script_entry = "update";
    result.script_source =
        "function update(self, ctx)\n"
        "end\n";
    return result;
}

std::vector<component_definition> build_definitions() {
    return {
        {
            "BackgroundRenderer",
            component_category::renderer,
            "Background Renderer",
            true,
            make_renderer_defaults("BackgroundRenderer", "#101216"),
            {"background"},
            {{"props.fill", "color", true}},
        },
        {
            "TextRenderer",
            component_category::renderer,
            "Text Renderer",
            true,
            [] {
                component result = make_renderer_defaults("TextRenderer", "#d8d4ff");
                result.text = "Text";
                return result;
            }(),
            {"text"},
            {
                {"props.text", "string", true},
                {"props.fill", "color", true},
            },
        },
        {
            "ShapeRenderer",
            component_category::renderer,
            "Shape Renderer",
            true,
            [] {
                component result = make_renderer_defaults("ShapeRenderer", "#6ee7b7");
                result.shape = "rect";
                return result;
            }(),
            {"shape"},
            {
                {"props.shape", "enum", false},
                {"props.fill", "color", true},
            },
        },
        {
            "ImageRenderer",
            component_category::renderer,
            "Image Renderer",
            true,
            make_renderer_defaults("ImageRenderer", "#ffffff"),
            {"image"},
            {
                {"props.assetId", "asset", false},
                {"props.fill", "color", true},
            },
        },
        {
            "BeatGridRenderer",
            component_category::renderer,
            "Beat Grid Renderer",
            true,
            make_renderer_defaults("BeatGridRenderer", "#8b7cf6"),
            {"beatGrid"},
            {{"props.fill", "color", true}},
        },
        {
            "WaveformRenderer",
            component_category::renderer,
            "Waveform Renderer",
            true,
            make_renderer_defaults("WaveformRenderer", "#6ee7b7"),
            {"waveform"},
            {{"props.fill", "color", true}},
        },
        {
            "SpectrumRenderer",
            component_category::renderer,
            "Spectrum Renderer",
            true,
            [] {
                component result = make_renderer_defaults("SpectrumRenderer", "#a855f7");
                result.shape = "title";
                return result;
            }(),
            {"spectrum"},
            {
                {"props.fill", "color", true},
                {"props.shape", "enum", false},
            },
        },
        {
            "Fade",
            component_category::modifier,
            "Fade",
            false,
            make_modifier_defaults("Fade", "transform.opacity", 650.0f),
            {"fade"},
            {{"props.amount", "number", true}},
        },
        {
            "Pulse",
            component_category::modifier,
            "Pulse",
            false,
            make_modifier_defaults("Pulse", "transform.scale", 0.08f),
            {"pulse"},
            {
                {"props.amount", "number", true},
                {"props.target", "enum", false},
            },
        },
        {
            "BeatReactive",
            component_category::modifier,
            "Beat Reactive",
            false,
            make_modifier_defaults("BeatReactive", "transform.scale", 0.08f),
            {"beatPulse"},
            {
                {"props.amount", "number", true},
                {"props.target", "enum", false},
            },
        },
        {
            "Flash",
            component_category::modifier,
            "Flash",
            false,
            make_modifier_defaults("Flash", "transform.opacity", 0.35f),
            {"flash"},
            {{"props.amount", "number", true}},
        },
        {
            "Shake",
            component_category::modifier,
            "Shake",
            false,
            make_modifier_defaults("Shake", "transform.position", 18.0f),
            {"shake"},
            {
                {"props.amount", "number", true},
                {"props.target", "enum", false},
            },
        },
        {
            "AudioReactive",
            component_category::modifier,
            "Audio Reactive",
            false,
            make_modifier_defaults("AudioReactive", "transform.scale", 0.12f),
            {},
            {
                {"props.amount", "number", true},
                {"props.target", "enum", false},
            },
        },
        {
            "ColorShift",
            component_category::modifier,
            "Color Shift",
            false,
            make_modifier_defaults("ColorShift", "props.fill", 0.35f),
            {},
            {{"props.amount", "number", true}},
        },
        {
            "LuaBehaviour",
            component_category::behaviour,
            "Lua Behaviour",
            false,
            make_lua_defaults(),
            {"lua", "script"},
            {
                {"props.scriptAssetId", "asset", false},
                {"props.script", "string", false},
                {"props.entry", "string", false},
            },
        },
    };
}

}  // namespace

const std::vector<component_definition>& component_definitions() {
    static const std::vector<component_definition> definitions = build_definitions();
    return definitions;
}

const component_definition* find_component_definition(const std::string& type) {
    const std::string canonical = canonical_component_type(type);
    const auto& definitions = component_definitions();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const component_definition& definition) {
        if (definition.type == canonical || definition.type == type) {
            return true;
        }
        return std::find(definition.aliases.begin(), definition.aliases.end(), type) != definition.aliases.end();
    });
    return it == definitions.end() ? nullptr : &*it;
}

component make_default_component(std::string type) {
    if (const component_definition* definition = find_component_definition(type); definition != nullptr) {
        component result = definition->defaults;
        result.type = definition->type;
        return result;
    }
    component result;
    result.type = std::move(type);
    return result;
}

std::string display_name_for_component_type(const std::string& type) {
    if (const component_definition* definition = find_component_definition(type); definition != nullptr) {
        return definition->display_name;
    }
    return canonical_component_type(type);
}

}  // namespace mv::composition
