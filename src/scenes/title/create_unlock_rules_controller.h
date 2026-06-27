#pragma once

#include <future>
#include <string>
#include <vector>

#include "network/unlock_rule_client.h"

namespace title_create_unlock_rules {

struct source_chart_candidate {
    std::string remote_chart_id;
    std::string label;
};

class controller {
public:
    void open(std::string local_chart_id,
              std::string remote_chart_id,
              std::string chart_label,
              std::vector<source_chart_candidate> source_candidates);
    void update();
    void draw();
    [[nodiscard]] bool open() const;
    [[nodiscard]] bool consume_saved();
    [[nodiscard]] bool has_draft_for(const std::string& local_chart_id) const;
    [[nodiscard]] std::vector<unlock_rule_client::rule> draft_rules_for(const std::string& local_chart_id) const;
    void clear_draft(const std::string& local_chart_id);

private:
    void poll();
    void start_save();
    void start_validate();
    void add_default_rule();
    void normalize_rule(unlock_rule_client::rule& rule, size_t index) const;
    [[nodiscard]] int source_index_for(const std::string& remote_chart_id) const;
    [[nodiscard]] std::string source_label_for(const std::string& remote_chart_id) const;
    [[nodiscard]] std::string summary_for(const unlock_rule_client::rule& rule) const;

    bool open_ = false;
    bool saved_ = false;
    bool loading_ = false;
    bool saving_ = false;
    bool validating_ = false;
    bool dirty_ = false;
    bool draft_mode_ = false;
    std::string local_chart_id_;
    std::string draft_chart_id_;
    std::vector<unlock_rule_client::rule> draft_rules_;
    std::string remote_chart_id_;
    std::string chart_label_;
    std::string message_;
    std::vector<source_chart_candidate> source_candidates_;
    std::vector<unlock_rule_client::rule> rules_;
    std::vector<unlock_rule_client::validation> validations_;
    int selected_rule_ = 0;
    float rule_list_scroll_y_ = 0.0f;
    bool rule_list_scrollbar_dragging_ = false;
    float rule_list_scrollbar_drag_offset_ = 0.0f;
    std::future<unlock_rule_client::operation_result> load_future_;
    std::future<unlock_rule_client::operation_result> save_future_;
    std::future<unlock_rule_client::operation_result> validate_future_;
};

}  // namespace title_create_unlock_rules
