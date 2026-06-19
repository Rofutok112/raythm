#pragma once

#include <string>
#include <vector>

namespace unlock_rule_client {

enum class rule_type {
    always_unlocked,
    clear_chart,
    clear_rank_at_least,
};

struct dependency_authorization {
    std::string id;
    std::string source_chart_id;
    std::string status;
};

struct rule {
    std::string id;
    rule_type type = rule_type::clear_chart;
    std::string source_chart_id;
    std::string min_clear_rank = "c";
    std::string lock_reason;
    int display_order = 0;
    bool active = true;
    std::vector<dependency_authorization> dependency_authorizations;
};

struct validation {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct operation_result {
    bool success = false;
    bool unauthorized = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    std::vector<rule> rules;
    std::vector<validation> validations;
};

operation_result fetch_chart_rules(const std::string& remote_chart_id);
operation_result validate_chart_rules(const std::string& remote_chart_id,
                                      const std::vector<rule>& rules);
operation_result replace_chart_rules(const std::string& remote_chart_id,
                                     const std::vector<rule>& rules);

std::string rule_type_wire(rule_type value);
std::string rule_type_label(rule_type value);
std::string rules_json_payload(const std::vector<rule>& rules);

}  // namespace unlock_rule_client
