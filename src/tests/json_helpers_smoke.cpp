#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "network/json_helpers.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    const std::string nested_ids =
        "{"
        "\"child\":{\"id\":\"nested-id\"},"
        "\"id\":\"top-level-id\","
        "\"items\":[{\"id\":\"item-a\"},{\"id\":\"item-b\"}],"
        "\"labels\":[\"one\",\"two\"],"
        "\"enabled\":true,"
        "\"count\":7,"
        "\"ratio\":1.25"
        "}";

    expect(network::json::extract_string(nested_ids, "id").value_or("") == "top-level-id",
           "String extraction should prefer the current object's key over nested keys.", ok);
    expect(network::json::extract_string(nested_ids, "missing") == std::nullopt,
           "Missing string fields should be absent.", ok);
    expect(network::json::extract_bool(nested_ids, "enabled").value_or(false),
           "Boolean extraction should parse true.", ok);
    expect(network::json::extract_int(nested_ids, "count").value_or(0) == 7,
           "Integer extraction should parse integers.", ok);
    expect(network::json::extract_float(nested_ids, "ratio").value_or(0.0f) > 1.24f,
           "Float extraction should parse numeric values.", ok);

    const std::optional<std::string> child = network::json::extract_object(nested_ids, "child");
    expect(child.has_value(), "Object extraction should return object content.", ok);
    expect(child.has_value() && network::json::extract_string(*child, "id").value_or("") == "nested-id",
           "Object extraction should preserve nested object fields.", ok);

    const std::optional<std::string> items = network::json::extract_array(nested_ids, "items");
    expect(items.has_value(), "Array extraction should return array content.", ok);
    const std::vector<std::string> item_objects =
        items.has_value() ? network::json::extract_objects_from_array(*items) : std::vector<std::string>{};
    expect(item_objects.size() == 2, "Object array extraction should return direct object elements.", ok);
    expect(item_objects.size() == 2 &&
               network::json::extract_string(item_objects.back(), "id").value_or("") == "item-b",
           "Object array extraction should preserve direct object values.", ok);

    const std::optional<std::string> labels = network::json::extract_array(nested_ids, "labels");
    const std::vector<std::string> label_values =
        labels.has_value() ? network::json::extract_strings_from_array(*labels) : std::vector<std::string>{};
    expect(label_values.size() == 2 && label_values.front() == "one" && label_values.back() == "two",
           "String array extraction should return direct string elements.", ok);

    const std::string escaped = "quote \" slash \\ newline \n tab \t";
    const std::string escaped_json = "{\"value\":\"" + network::json::escape_string(escaped) + "\"}";
    expect(network::json::extract_string(escaped_json, "value").value_or("") == escaped,
           "Escaped strings should round-trip through JSON parsing.", ok);

    expect(!network::json::extract_string("{\"value\":\"unterminated", "value").has_value(),
           "Malformed JSON should not throw and should not return partial values.", ok);
    expect(network::json::extract_objects_from_array("[1,{\"id\":\"object\"},\"text\"]").size() == 1,
           "Object array extraction should ignore non-object array elements.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "json_helpers smoke test passed\n";
    return EXIT_SUCCESS;
}
