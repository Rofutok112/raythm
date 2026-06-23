#include "mv/composition/mv_lua_runtime.h"

#include <memory>
#include <string>
#include <unordered_map>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace mv::composition {
namespace {

void push_vec2(lua_State* lua, float x, float y) {
    lua_createtable(lua, 0, 2);
    lua_pushnumber(lua, x);
    lua_setfield(lua, -2, "x");
    lua_pushnumber(lua, y);
    lua_setfield(lua, -2, "y");
}

float read_number_field(lua_State* lua, int index, const char* key, float fallback) {
    lua_getfield(lua, index, key);
    const float result = lua_isnumber(lua, -1) ? static_cast<float>(lua_tonumber(lua, -1)) : fallback;
    lua_pop(lua, 1);
    return result;
}

void read_vec2(lua_State* lua, int index, const char* key, float& x, float& y) {
    lua_getfield(lua, index, key);
    if (lua_istable(lua, -1)) {
        x = read_number_field(lua, -1, "x", x);
        y = read_number_field(lua, -1, "y", y);
    }
    lua_pop(lua, 1);
}

void push_transform(lua_State* lua, const transform& value) {
    lua_createtable(lua, 0, 5);
    push_vec2(lua, value.position_x, value.position_y);
    lua_setfield(lua, -2, "position");
    push_vec2(lua, value.scale_x, value.scale_y);
    lua_setfield(lua, -2, "scale");
    push_vec2(lua, value.anchor_x, value.anchor_y);
    lua_setfield(lua, -2, "anchor");
    lua_pushnumber(lua, value.rotation_deg);
    lua_setfield(lua, -2, "rotationDeg");
    lua_pushnumber(lua, value.opacity);
    lua_setfield(lua, -2, "opacity");
}

void push_self(lua_State* lua, const object& target) {
    lua_createtable(lua, 0, 3);
    lua_pushstring(lua, target.id.c_str());
    lua_setfield(lua, -2, "id");
    lua_pushstring(lua, target.name.c_str());
    lua_setfield(lua, -2, "name");
    push_transform(lua, target.transform_data);
    lua_setfield(lua, -2, "transform");
}

void push_context(lua_State* lua, const lua_update_context& context) {
    lua_createtable(lua, 0, 2);
    lua_pushnumber(lua, context.song_time_ms);
    lua_setfield(lua, -2, "songTimeMs");
    lua_pushnumber(lua, context.delta_ms);
    lua_setfield(lua, -2, "deltaMs");
}

bool read_self_transform(lua_State* lua, int self_index, object& target) {
    lua_getfield(lua, self_index, "transform");
    if (!lua_istable(lua, -1)) {
        lua_pop(lua, 1);
        return false;
    }

    transform next = target.transform_data;
    read_vec2(lua, -1, "position", next.position_x, next.position_y);
    read_vec2(lua, -1, "scale", next.scale_x, next.scale_y);
    read_vec2(lua, -1, "anchor", next.anchor_x, next.anchor_y);
    next.rotation_deg = read_number_field(lua, -1, "rotationDeg", next.rotation_deg);
    next.opacity = read_number_field(lua, -1, "opacity", next.opacity);
    lua_pop(lua, 1);
    apply_transform_to_object(target, next);
    return true;
}

std::string lua_error_message(lua_State* lua) {
    const char* message = lua_tostring(lua, -1);
    return message == nullptr ? std::string{"LuaBehaviour failed."} : std::string{message};
}

void push_context_and_self(lua_State* lua,
                           const lua_update_context& context,
                           int self_ref) {
    lua_rawgeti(lua, LUA_REGISTRYINDEX, self_ref);
    push_context(lua, context);
}

bool call_lua_function(lua_State* lua,
                       const std::string& function_name,
                       const object& target,
                       const lua_update_context& context,
                       int self_ref,
                       std::vector<std::string>& diagnostics) {
    lua_getglobal(lua, function_name.c_str());
    if (!lua_isfunction(lua, -1)) {
        lua_pop(lua, 1);
        diagnostics.push_back("LuaBehaviour entry function not found: " + function_name);
        return false;
    }

    push_context_and_self(lua, context, self_ref);
    if (lua_pcall(lua, 2, 0, 0) != LUA_OK) {
        diagnostics.push_back(lua_error_message(lua));
        return false;
    }
    return true;
}

bool call_optional_lua_function(lua_State* lua,
                                const std::string& function_name,
                                const object& target,
                                const lua_update_context& context,
                                int self_ref,
                                std::vector<std::string>& diagnostics) {
    lua_getglobal(lua, function_name.c_str());
    if (!lua_isfunction(lua, -1)) {
        lua_pop(lua, 1);
        return true;
    }

    push_context_and_self(lua, context, self_ref);
    if (lua_pcall(lua, 2, 0, 0) != LUA_OK) {
        diagnostics.push_back(lua_error_message(lua));
        return false;
    }
    return true;
}

std::string behaviour_key(const object& target, const component& behaviour) {
    return target.id + '\n' + behaviour.id;
}

std::string behaviour_signature(const component& behaviour) {
    return behaviour.script_asset_id + '\n' +
           (behaviour.script_entry.empty() ? "update" : behaviour.script_entry) + '\n' +
           behaviour.script_source;
}

struct lua_state_deleter {
    void operator()(lua_State* lua) const {
        if (lua != nullptr) {
            lua_close(lua);
        }
    }
};

struct runtime_entry {
    std::unique_ptr<lua_State, lua_state_deleter> lua;
    std::string signature;
    bool started = false;
};

std::unique_ptr<lua_State, lua_state_deleter> make_state_for_behaviour(const component& behaviour,
                                                                       const object& target,
                                                                       std::vector<std::string>& diagnostics) {
    if (behaviour.script_source.empty()) {
        diagnostics.push_back("LuaBehaviour has no script source.");
        return nullptr;
    }

    lua_State* lua = luaL_newstate();
    if (lua == nullptr) {
        diagnostics.push_back("Failed to create Lua state.");
        return nullptr;
    }
    std::unique_ptr<lua_State, lua_state_deleter> state(lua);
    luaL_openlibs(lua);

    const std::string chunk_name = target.id + ":" + behaviour.id;
    if (luaL_loadbuffer(lua,
                        behaviour.script_source.data(),
                        behaviour.script_source.size(),
                        chunk_name.c_str()) != LUA_OK ||
        lua_pcall(lua, 0, 0, 0) != LUA_OK) {
        diagnostics.push_back(lua_error_message(lua));
        return nullptr;
    }
    return state;
}

bool run_lua_behaviour(runtime_entry& entry,
                       const component& behaviour,
                       object& target,
                       const lua_update_context& context,
                       std::vector<std::string>& diagnostics) {
    lua_State* lua = entry.lua.get();
    const std::string entry_name = behaviour.script_entry.empty() ? "update" : behaviour.script_entry;
    push_self(lua, target);
    const int self_index = lua_gettop(lua);
    lua_pushvalue(lua, self_index);
    const int self_ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    bool success = true;
    if (!entry.started) {
        success = call_optional_lua_function(lua, "start", target, context, self_ref, diagnostics);
        entry.started = success;
    }
    if (success) {
        success = call_lua_function(lua, entry_name, target, context, self_ref, diagnostics);
    }
    if (!success) {
        luaL_unref(lua, LUA_REGISTRYINDEX, self_ref);
        return false;
    }

    lua_rawgeti(lua, LUA_REGISTRYINDEX, self_ref);
    read_self_transform(lua, -1, target);
    lua_pop(lua, 1);
    luaL_unref(lua, LUA_REGISTRYINDEX, self_ref);
    return true;
}

}  // namespace

struct lua_behaviour_runtime::impl {
    std::unordered_map<std::string, runtime_entry> entries;
};

lua_behaviour_runtime::lua_behaviour_runtime()
    : impl_(std::make_unique<impl>()) {}

lua_behaviour_runtime::~lua_behaviour_runtime() = default;

void lua_behaviour_runtime::reset() {
    impl_->entries.clear();
}

lua_update_result lua_behaviour_runtime::apply_lua_behaviours(object& target, const lua_update_context& context) {
    lua_update_result result;
    for (const component& current : target.components) {
        if (!current.enabled || canonical_component_type(current.type) != "LuaBehaviour") {
            continue;
        }
        const std::string key = behaviour_key(target, current);
        const std::string signature = behaviour_signature(current);
        runtime_entry& entry = impl_->entries[key];
        if (entry.lua == nullptr || entry.signature != signature) {
            entry = {};
            entry.signature = signature;
            entry.lua = make_state_for_behaviour(current, target, result.diagnostics);
        }
        if (entry.lua == nullptr || !run_lua_behaviour(entry, current, target, context, result.diagnostics)) {
            result.success = false;
        }
    }
    return result;
}

lua_update_result apply_lua_behaviours(object& target, const lua_update_context& context) {
    lua_behaviour_runtime runtime;
    return runtime.apply_lua_behaviours(target, context);
}

}  // namespace mv::composition
