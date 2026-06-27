#include "title/create_unlock_rules_controller.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "title/create_unlock_rules_view.h"

namespace title_create_unlock_rules {
namespace {

const char* rank_options[] = {"f", "c", "b", "a", "aa", "s", "ss"};

int rank_index(const std::string& rank) {
    for (int i = 0; i < static_cast<int>(std::size(rank_options)); ++i) {
        if (rank == rank_options[i]) {
            return i;
        }
    }
    return 1;
}

unlock_rule_client::rule_type next_type(unlock_rule_client::rule_type value) {
    switch (value) {
    case unlock_rule_client::rule_type::clear_chart:
        return unlock_rule_client::rule_type::clear_rank_at_least;
    case unlock_rule_client::rule_type::clear_rank_at_least:
        return unlock_rule_client::rule_type::always_unlocked;
    case unlock_rule_client::rule_type::always_unlocked:
        return unlock_rule_client::rule_type::clear_chart;
    }
    return unlock_rule_client::rule_type::clear_chart;
}

unlock_rule_client::rule_type previous_type(unlock_rule_client::rule_type value) {
    switch (value) {
    case unlock_rule_client::rule_type::clear_chart:
        return unlock_rule_client::rule_type::always_unlocked;
    case unlock_rule_client::rule_type::clear_rank_at_least:
        return unlock_rule_client::rule_type::clear_chart;
    case unlock_rule_client::rule_type::always_unlocked:
        return unlock_rule_client::rule_type::clear_rank_at_least;
    }
    return unlock_rule_client::rule_type::clear_chart;
}

int selected_rule_index(int selected, size_t count) {
    if (count == 0) {
        return 0;
    }
    return std::clamp(selected, 0, static_cast<int>(count) - 1);
}

}  // namespace

void controller::open(std::string local_chart_id,
                      std::string remote_chart_id,
                      std::string chart_label,
                      std::vector<source_chart_candidate> source_candidates) {
    open_ = true;
    saved_ = false;
    loading_ = true;
    saving_ = false;
    validating_ = false;
    dirty_ = false;
    local_chart_id_ = std::move(local_chart_id);
    remote_chart_id_ = std::move(remote_chart_id);
    chart_label_ = std::move(chart_label);
    source_candidates_ = std::move(source_candidates);
    rules_.clear();
    validations_.clear();
    selected_rule_ = 0;
    rule_list_scroll_y_ = 0.0f;
    rule_list_scrollbar_dragging_ = false;
    rule_list_scrollbar_drag_offset_ = 0.0f;
    draft_mode_ = remote_chart_id_.empty();
    if (draft_mode_) {
        loading_ = false;
        if (draft_chart_id_ == local_chart_id_) {
            rules_ = draft_rules_;
        }
        if (rules_.empty()) {
            add_default_rule();
        }
        dirty_ = false;
        message_ = "Draft rules will be published after chart upload.";
    } else {
        message_ = "Loading unlock rules...";
        load_future_ = std::async(std::launch::async, [chart_id = remote_chart_id_]() {
            return unlock_rule_client::fetch_chart_rules(chart_id);
        });
    }
}

bool controller::open() const {
    return open_;
}

bool controller::consume_saved() {
    const bool result = saved_;
    saved_ = false;
    return result;
}

bool controller::has_draft_for(const std::string& local_chart_id) const {
    return !local_chart_id.empty() && draft_chart_id_ == local_chart_id && !draft_rules_.empty();
}

std::vector<unlock_rule_client::rule> controller::draft_rules_for(const std::string& local_chart_id) const {
    return has_draft_for(local_chart_id) ? draft_rules_ : std::vector<unlock_rule_client::rule>{};
}

void controller::clear_draft(const std::string& local_chart_id) {
    if (draft_chart_id_ != local_chart_id) {
        return;
    }
    draft_chart_id_.clear();
    draft_rules_.clear();
}

void controller::poll() {
    if (loading_ && load_future_.valid() &&
        load_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        loading_ = false;
        const unlock_rule_client::operation_result result = load_future_.get();
        if (result.success) {
            rules_ = result.rules;
            if (rules_.empty()) {
                add_default_rule();
                dirty_ = false;
            }
            message_ = rules_.empty() ? "No lock rules configured." : "Unlock rules loaded.";
        } else {
            message_ = result.message.empty() ? "Failed to load unlock rules." : result.message;
        }
    }
    if (validating_ && validate_future_.valid() &&
        validate_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        validating_ = false;
        const unlock_rule_client::operation_result result = validate_future_.get();
        validations_ = result.validations;
        message_ = result.success ? "Validation completed." : result.message;
    }
    if (saving_ && save_future_.valid() &&
        save_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        saving_ = false;
        const unlock_rule_client::operation_result result = save_future_.get();
        if (result.success) {
            rules_ = result.rules;
            validations_ = result.validations;
            dirty_ = false;
            saved_ = true;
            message_ = "Unlock rules saved.";
        } else {
            validations_ = result.validations;
            message_ = result.message.empty() ? "Failed to save unlock rules." : result.message;
        }
    }
}

void controller::update() {
    poll();
}

void controller::add_default_rule() {
    unlock_rule_client::rule rule;
    rule.type = unlock_rule_client::rule_type::clear_chart;
    rule.display_order = static_cast<int>(rules_.size());
    for (const source_chart_candidate& candidate : source_candidates_) {
        if (candidate.remote_chart_id != remote_chart_id_) {
            rule.source_chart_id = candidate.remote_chart_id;
            break;
        }
    }
    rule.lock_reason = "Clear " + source_label_for(rule.source_chart_id) + " to unlock.";
    rules_.push_back(std::move(rule));
    selected_rule_ = static_cast<int>(rules_.size()) - 1;
    rule_list_scroll_y_ = title_create_unlock_rules_view::max_rule_list_scroll(rules_.size());
}

void controller::normalize_rule(unlock_rule_client::rule& rule, size_t index) const {
    rule.display_order = static_cast<int>(index);
    if (rule.type == unlock_rule_client::rule_type::always_unlocked) {
        rule.source_chart_id.clear();
    } else if (rule.source_chart_id.empty() && !source_candidates_.empty()) {
        for (const source_chart_candidate& candidate : source_candidates_) {
            if (candidate.remote_chart_id != remote_chart_id_) {
                rule.source_chart_id = candidate.remote_chart_id;
                break;
            }
        }
    }
    if (rule.lock_reason.empty() && !rule.source_chart_id.empty()) {
        rule.lock_reason = "Clear " + source_label_for(rule.source_chart_id) + " to unlock.";
    }
}

void controller::start_validate() {
    if (loading_ || saving_ || validating_) {
        return;
    }
    for (size_t i = 0; i < rules_.size(); ++i) {
        normalize_rule(rules_[i], i);
    }
    if (draft_mode_) {
        validations_.clear();
        bool valid = true;
        for (const unlock_rule_client::rule& rule : rules_) {
            unlock_rule_client::validation validation;
            if (rule.type != unlock_rule_client::rule_type::always_unlocked &&
                rule.source_chart_id.empty()) {
                validation.valid = false;
                validation.errors.push_back("Choose a source chart before publishing.");
                valid = false;
            }
            validations_.push_back(std::move(validation));
        }
        message_ = valid ? "Draft validation OK." : "Draft validation failed.";
        return;
    }
    validating_ = true;
    message_ = "Validating unlock rules...";
    validate_future_ = std::async(std::launch::async, [chart_id = remote_chart_id_, rules = rules_]() {
        return unlock_rule_client::validate_chart_rules(chart_id, rules);
    });
}

void controller::start_save() {
    if (loading_ || saving_ || validating_) {
        return;
    }
    for (size_t i = 0; i < rules_.size(); ++i) {
        normalize_rule(rules_[i], i);
    }
    if (draft_mode_) {
        draft_chart_id_ = local_chart_id_;
        draft_rules_ = rules_;
        dirty_ = false;
        saved_ = true;
        open_ = false;
        return;
    }
    saving_ = true;
    message_ = "Saving unlock rules...";
    save_future_ = std::async(std::launch::async, [chart_id = remote_chart_id_, rules = rules_]() {
        return unlock_rule_client::replace_chart_rules(chart_id, rules);
    });
}

int controller::source_index_for(const std::string& remote_chart_id) const {
    for (int i = 0; i < static_cast<int>(source_candidates_.size()); ++i) {
        if (source_candidates_[static_cast<size_t>(i)].remote_chart_id == remote_chart_id) {
            return i;
        }
    }
    return -1;
}

std::string controller::source_label_for(const std::string& remote_chart_id) const {
    for (const source_chart_candidate& candidate : source_candidates_) {
        if (candidate.remote_chart_id == remote_chart_id) {
            return candidate.label;
        }
    }
    return remote_chart_id.empty() ? "another chart" : remote_chart_id;
}

std::string controller::summary_for(const unlock_rule_client::rule& rule) const {
    if (rule.type == unlock_rule_client::rule_type::always_unlocked) {
        return "No lock condition.";
    }
    std::string summary = "Clear " + source_label_for(rule.source_chart_id);
    if (rule.type == unlock_rule_client::rule_type::clear_rank_at_least) {
        summary += " with rank " + rule.min_clear_rank + " or better";
    }
    if (!rule.dependency_authorizations.empty()) {
        summary += " [" + rule.dependency_authorizations.front().status + "]";
    }
    return summary;
}

void controller::draw() {
    if (!open_) {
        return;
    }
    const bool busy = loading_ || saving_ || validating_;
    const int selected_index = selected_rule_index(selected_rule_, rules_.size());
    std::vector<title_create_unlock_rules_view::rule_item> rule_items;
    rule_items.reserve(rules_.size());
    for (int i = 0; i < static_cast<int>(rules_.size()); ++i) {
        const unlock_rule_client::rule& rule = rules_[static_cast<size_t>(i)];
        rule_items.push_back({
            .title = "Rule " + std::to_string(i + 1),
            .summary = summary_for(rule),
            .selected = i == selected_index,
        });
    }

    title_create_unlock_rules_view::editor_model editor;
    if (!loading_ && !rules_.empty()) {
        const unlock_rule_client::rule& rule = rules_[static_cast<size_t>(selected_index)];
        editor.visible = true;
        editor.show_source = rule.type != unlock_rule_client::rule_type::always_unlocked;
        editor.show_rank = rule.type == unlock_rule_client::rule_type::clear_rank_at_least;
        editor.type_label = unlock_rule_client::rule_type_label(rule.type);
        editor.source_label = source_label_for(rule.source_chart_id);
        editor.rank_label = rank_options[rank_index(rule.min_clear_rank)];
        editor.reason = rule.lock_reason;
        editor.summary = summary_for(rule);
        if (selected_index < static_cast<int>(validations_.size())) {
            const unlock_rule_client::validation& validation = validations_[static_cast<size_t>(selected_index)];
            editor.validation_message = validation.valid
                ? (validation.warnings.empty() ? "Validation OK." : validation.warnings.front())
                : (validation.errors.empty() ? "Validation failed." : validation.errors.front());
            editor.validation_is_error = !validation.valid;
        }
    }

    const std::string subtitle = draft_mode_
        ? chart_label_ + " - draft before upload"
        : chart_label_;
    const title_create_unlock_rules_view::draw_result view_result =
        title_create_unlock_rules_view::draw({
            .subtitle = subtitle,
            .message = message_,
            .close_label = dirty_ ? "CANCEL" : "CLOSE",
            .validate_label = validating_ ? "VALIDATING" : "VALIDATE",
            .save_label = saving_ ? "SAVING" : (draft_mode_ ? "SAVE DRAFT" : "SAVE RULES"),
            .rules = rule_items,
            .editor = editor,
            .loading = loading_,
            .dirty = dirty_,
            .busy = busy,
            .can_add = rules_.size() < 8,
            .can_remove = !rules_.empty(),
            .rule_list_scroll_y = rule_list_scroll_y_,
            .rule_list_scrollbar_dragging = rule_list_scrollbar_dragging_,
            .rule_list_scrollbar_drag_offset = rule_list_scrollbar_drag_offset_,
        });
    rule_list_scroll_y_ = view_result.rule_list_scroll_y;
    rule_list_scrollbar_dragging_ = view_result.rule_list_scrollbar_dragging;
    rule_list_scrollbar_drag_offset_ = view_result.rule_list_scrollbar_drag_offset;

    switch (view_result.action.type) {
    case title_create_unlock_rules_view::command_type::none:
        break;
    case title_create_unlock_rules_view::command_type::close:
        open_ = false;
        break;
    case title_create_unlock_rules_view::command_type::select_rule:
        if (view_result.action.index >= 0 && view_result.action.index < static_cast<int>(rules_.size())) {
            selected_rule_ = view_result.action.index;
        }
        break;
    case title_create_unlock_rules_view::command_type::type_previous:
    case title_create_unlock_rules_view::command_type::type_next:
        if (!rules_.empty()) {
            unlock_rule_client::rule& rule = rules_[static_cast<size_t>(selected_index)];
            rule.type = view_result.action.type == title_create_unlock_rules_view::command_type::type_previous
                ? previous_type(rule.type)
                : next_type(rule.type);
            dirty_ = true;
        }
        break;
    case title_create_unlock_rules_view::command_type::source_previous:
    case title_create_unlock_rules_view::command_type::source_next:
        if (!rules_.empty()) {
            unlock_rule_client::rule& rule = rules_[static_cast<size_t>(selected_index)];
            int source_index = source_index_for(rule.source_chart_id);
            const int count = static_cast<int>(source_candidates_.size());
            const int delta = view_result.action.type == title_create_unlock_rules_view::command_type::source_previous
                ? -1
                : 1;
            for (int step = 0; step < count; ++step) {
                source_index = (source_index + delta + count) % count;
                if (source_candidates_[static_cast<size_t>(source_index)].remote_chart_id != remote_chart_id_) {
                    rule.source_chart_id = source_candidates_[static_cast<size_t>(source_index)].remote_chart_id;
                    dirty_ = true;
                    break;
                }
            }
        }
        break;
    case title_create_unlock_rules_view::command_type::rank_previous:
    case title_create_unlock_rules_view::command_type::rank_next:
        if (!rules_.empty()) {
            unlock_rule_client::rule& rule = rules_[static_cast<size_t>(selected_index)];
            int index = rank_index(rule.min_clear_rank);
            const int delta = view_result.action.type == title_create_unlock_rules_view::command_type::rank_previous
                ? -1
                : 1;
            index = (index + delta + static_cast<int>(std::size(rank_options))) %
                    static_cast<int>(std::size(rank_options));
            rule.min_clear_rank = rank_options[index];
            dirty_ = true;
        }
        break;
    case title_create_unlock_rules_view::command_type::add_rule:
        if (!busy && rules_.size() < 8) {
            add_default_rule();
            dirty_ = true;
        }
        break;
    case title_create_unlock_rules_view::command_type::remove_rule:
        if (!busy && !rules_.empty()) {
            rules_.erase(rules_.begin() + selected_index);
            selected_rule_ = std::clamp(selected_index, 0, std::max(0, static_cast<int>(rules_.size()) - 1));
            rule_list_scroll_y_ = std::min(rule_list_scroll_y_,
                                           title_create_unlock_rules_view::max_rule_list_scroll(rules_.size()));
            dirty_ = true;
        }
        break;
    case title_create_unlock_rules_view::command_type::validate:
        if (!busy) {
            start_validate();
        }
        break;
    case title_create_unlock_rules_view::command_type::save:
        if (!busy) {
            start_save();
        }
        break;
    }
}

}  // namespace title_create_unlock_rules
