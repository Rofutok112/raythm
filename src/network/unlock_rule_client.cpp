#include "network/unlock_rule_client.h"

#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "network/auth_client.h"
#include "network/http_client.h"
#include "network/json_helpers.h"
#include "network/network_error.h"

namespace {
namespace json = network::json;
using http_response = network::http::response;

std::optional<auth::session> load_active_session() {
    std::optional<auth::session> stored = auth::load_saved_session();
    if (stored.has_value()) {
        return stored;
    }
    const auth::operation_result restored = auth::restore_saved_session();
    if (restored.success && restored.session_data.has_value()) {
        return restored.session_data;
    }
    return std::nullopt;
}

http_response send_session_request(const auth::session& session_data,
                                   const std::string& method,
                                   const std::string& path,
                                   const std::string& body = {}) {
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + session_data.access_token},
        {"User-Agent", "raythm/0.1"},
    };
    if (!body.empty()) {
        headers.emplace_back("Content-Type", "application/json");
    }
    return network::http::send_request(
        method,
        auth::normalize_server_url(session_data.server_url) + path,
        headers,
        body);
}

template <typename Parser>
unlock_rule_client::operation_result send_with_refresh(const std::string& method,
                                                       const std::string& path,
                                                       const std::string& body,
                                                       const std::string& fallback,
                                                       Parser parse_success) {
    std::optional<auth::session> session = load_active_session();
    if (!session.has_value()) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in before editing unlock rules.",
        };
    }

    http_response response = send_session_request(*session, method, path, body);
    if (response.error_message.empty() && response.status_code == 401) {
        const auth::operation_result restored = auth::restore_saved_session();
        if (restored.success && restored.session_data.has_value()) {
            session = restored.session_data;
            response = send_session_request(*session, method, path, body);
        }
    }

    if (!response.error_message.empty()) {
        return {.success = false, .message = response.error_message};
    }
    if (response.status_code == 401) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in before editing unlock rules.",
        };
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        const network::error_classification error =
            network::classify_http_error(response.status_code, response.body, fallback, response.retry_after);
        return {
            .success = false,
            .maintenance = error.is_maintenance(),
            .message = error.message,
            .retry_after = error.retry_after,
        };
    }

    unlock_rule_client::operation_result result = parse_success(response.body);
    result.success = true;
    return result;
}

unlock_rule_client::rule_type parse_rule_type(const std::string& value) {
    if (value == "always_unlocked") {
        return unlock_rule_client::rule_type::always_unlocked;
    }
    if (value == "clear_rank_at_least") {
        return unlock_rule_client::rule_type::clear_rank_at_least;
    }
    return unlock_rule_client::rule_type::clear_chart;
}

unlock_rule_client::rule parse_rule_object(const std::string& object) {
    unlock_rule_client::rule rule;
    rule.id = json::extract_string(object, "id").value_or("");
    rule.type = parse_rule_type(json::extract_string(object, "ruleType").value_or("clear_chart"));
    rule.active = json::extract_bool(object, "active").value_or(true);
    rule.display_order = json::extract_int(object, "displayOrder").value_or(0);
    rule.lock_reason = json::extract_string(object, "lockReason").value_or("");
    if (const std::optional<std::string> payload = json::extract_object(object, "rulePayload")) {
        rule.source_chart_id = json::extract_string(*payload, "sourceChartId").value_or("");
        rule.min_clear_rank = json::extract_string(*payload, "minClearRank").value_or(rule.min_clear_rank);
    }
    if (const std::optional<std::string> deps = json::extract_array(object, "dependencyAuthorizations")) {
        for (const std::string& dep_object : json::extract_objects_from_array(*deps)) {
            rule.dependency_authorizations.push_back({
                .id = json::extract_string(dep_object, "id").value_or(""),
                .source_chart_id = json::extract_string(dep_object, "sourceChartId").value_or(""),
                .status = json::extract_string(dep_object, "status").value_or(""),
            });
        }
    }
    return rule;
}

unlock_rule_client::validation parse_validation_object(const std::string& object) {
    unlock_rule_client::validation validation;
    validation.valid = json::extract_bool(object, "valid").value_or(false);
    if (const std::optional<std::string> errors = json::extract_array(object, "errors")) {
        validation.errors = json::extract_strings_from_array(*errors);
    }
    if (const std::optional<std::string> warnings = json::extract_array(object, "warnings")) {
        validation.warnings = json::extract_strings_from_array(*warnings);
    }
    return validation;
}

unlock_rule_client::operation_result parse_rules_response(const std::string& body) {
    unlock_rule_client::operation_result result;
    if (const std::optional<std::string> rules = json::extract_array(body, "rules")) {
        for (const std::string& object : json::extract_objects_from_array(*rules)) {
            result.rules.push_back(parse_rule_object(object));
        }
    }
    if (const std::optional<std::string> validations = json::extract_array(body, "validations")) {
        for (const std::string& object : json::extract_objects_from_array(*validations)) {
            result.validations.push_back(parse_validation_object(object));
        }
    }
    return result;
}

std::string rule_json(const unlock_rule_client::rule& rule) {
    std::ostringstream out;
    out << "{\"active\":" << (rule.active ? "true" : "false")
        << ",\"displayOrder\":" << rule.display_order
        << ",\"lockReason\":\"" << json::escape_string(rule.lock_reason) << "\""
        << ",\"ruleType\":\"" << unlock_rule_client::rule_type_wire(rule.type) << "\""
        << ",\"rulePayload\":{";
    bool has_field = false;
    if (!rule.source_chart_id.empty()) {
        out << "\"sourceChartId\":\"" << json::escape_string(rule.source_chart_id) << "\"";
        has_field = true;
    }
    if (rule.type == unlock_rule_client::rule_type::clear_rank_at_least) {
        if (has_field) {
            out << ',';
        }
        out << "\"minClearRank\":\"" << json::escape_string(rule.min_clear_rank) << "\"";
    }
    out << "}}";
    return out.str();
}

std::string chart_rules_path(const std::string& remote_chart_id, std::string suffix = {}) {
    return "/charts/" + remote_chart_id + "/unlock-rules" + suffix;
}

}  // namespace

namespace unlock_rule_client {

std::string rule_type_wire(rule_type value) {
    switch (value) {
    case rule_type::always_unlocked:
        return "always_unlocked";
    case rule_type::clear_rank_at_least:
        return "clear_rank_at_least";
    case rule_type::clear_chart:
        return "clear_chart";
    }
    return "clear_chart";
}

std::string rule_type_label(rule_type value) {
    switch (value) {
    case rule_type::always_unlocked:
        return "Always unlocked";
    case rule_type::clear_rank_at_least:
        return "Clear rank at least";
    case rule_type::clear_chart:
        return "Clear chart";
    }
    return "Clear chart";
}

std::string rules_json_payload(const std::vector<rule>& rules) {
    std::ostringstream out;
    out << "{\"rules\":[";
    for (size_t i = 0; i < rules.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << rule_json(rules[i]);
    }
    out << "]}";
    return out.str();
}

operation_result fetch_chart_rules(const std::string& remote_chart_id) {
    return send_with_refresh("GET", chart_rules_path(remote_chart_id), {}, "Failed to load unlock rules.",
                             parse_rules_response);
}

operation_result validate_chart_rules(const std::string& remote_chart_id,
                                      const std::vector<rule>& rules) {
    return send_with_refresh("POST", chart_rules_path(remote_chart_id, "/validate"), rules_json_payload(rules),
                             "Failed to validate unlock rules.", parse_rules_response);
}

operation_result replace_chart_rules(const std::string& remote_chart_id,
                                     const std::vector<rule>& rules) {
    return send_with_refresh("PUT", chart_rules_path(remote_chart_id), rules_json_payload(rules),
                             "Failed to save unlock rules.", parse_rules_response);
}

}  // namespace unlock_rule_client
