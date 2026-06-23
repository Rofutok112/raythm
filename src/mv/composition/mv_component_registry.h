#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct component_property_definition {
    std::string path;
    std::string value_type;
    bool keyframeable = false;
};

struct component_definition {
    std::string type;
    component_category category = component_category::unknown;
    std::string display_name;
    bool single_per_object = false;
    component defaults;
    std::vector<std::string> aliases;
    std::vector<component_property_definition> properties;
};

const std::vector<component_definition>& component_definitions();
const component_definition* find_component_definition(const std::string& type);
component make_default_component(std::string type);
std::string display_name_for_component_type(const std::string& type);

}  // namespace mv::composition
