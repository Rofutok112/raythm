#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct lua_update_context {
    double song_time_ms = 0.0;
    double delta_ms = 0.0;
};

struct lua_update_result {
    bool success = true;
    std::vector<std::string> diagnostics;
};

class lua_behaviour_runtime final {
public:
    lua_behaviour_runtime();
    ~lua_behaviour_runtime();

    lua_behaviour_runtime(const lua_behaviour_runtime&) = delete;
    lua_behaviour_runtime& operator=(const lua_behaviour_runtime&) = delete;

    void reset();
    lua_update_result apply_lua_behaviours(object& target, const lua_update_context& context);

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

lua_update_result apply_lua_behaviours(object& target, const lua_update_context& context);

}  // namespace mv::composition
