#pragma once

#include "mv_bytecode.h"
#include "../api/mv_scene.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
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

struct mv_object {
    std::string type_name;
    std::unordered_map<std::string, mv_value> attrs;
    std::optional<scene_node> cached_scene_node;
    std::optional<vec2> cached_point;

    mv_value get_attr(const std::string& name) const {
        auto it = attrs.find(name);
        if (it != attrs.end()) return it->second;
        return std::monostate{};
    }

    const mv_value* find_attr(const std::string& name) const {
        auto it = attrs.find(name);
        return it != attrs.end() ? &it->second : nullptr;
    }

    void reserve_attrs(std::size_t capacity) {
        attrs.reserve(capacity);
    }

    void clear_cached_render_data() {
        cached_scene_node.reset();
        cached_point.reset();
    }

    void set_attr(const std::string& name, mv_value val) {
        attrs[name] = std::move(val);
        clear_cached_render_data();
    }

    void set_cached_scene_node(scene_node node) {
        cached_scene_node = std::move(node);
    }

    void set_cached_point(vec2 point) {
        cached_point = point;
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

    vm_result execute();
    std::optional<vm_error> run_instruction(call_frame& frame);
    void push(mv_value val);
    mv_value pop();
    mv_value& peek_ref(int offset = 0);
    const std::string& get_string_constant(uint32_t idx) const;
    std::optional<vm_error> check_number_result(double val, int line);
};

// Helpers
bool is_truthy(const mv_value& val);
std::string value_type_name(const mv_value& val);
std::string value_to_string(const mv_value& val);

} // namespace mv
