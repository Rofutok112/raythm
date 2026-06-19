#include "title/create_unlock_rules_controller.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "virtual_screen.h"

namespace title_create_unlock_rules {
namespace {

constexpr Rectangle kDialogRect = {430.0f, 168.0f, 1060.0f, 744.0f};
constexpr Rectangle kTitleRect = {462.0f, 198.0f, 720.0f, 38.0f};
constexpr Rectangle kSubtitleRect = {462.0f, 238.0f, 860.0f, 28.0f};
constexpr Rectangle kRuleListRect = {462.0f, 296.0f, 368.0f, 456.0f};
constexpr Rectangle kEditorRect = {858.0f, 296.0f, 600.0f, 456.0f};
constexpr Rectangle kMessageRect = {462.0f, 766.0f, 996.0f, 34.0f};
constexpr Rectangle kCloseRect = {1326.0f, 198.0f, 132.0f, 42.0f};
constexpr Rectangle kAddRect = {462.0f, 812.0f, 160.0f, 48.0f};
constexpr Rectangle kRemoveRect = {638.0f, 812.0f, 160.0f, 48.0f};
constexpr Rectangle kValidateRect = {1064.0f, 812.0f, 180.0f, 48.0f};
constexpr Rectangle kSaveRect = {1262.0f, 812.0f, 196.0f, 48.0f};
constexpr float kRowHeight = 58.0f;
constexpr float kRowGap = 10.0f;

const char* rank_options[] = {"f", "c", "b", "a", "aa", "s", "ss"};

ui::button_state draw_modal_button(Rectangle rect, const char* label, int font_size) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect, ui::draw_layer::modal);
    const bool pressed = ui::is_pressed(rect, ui::draw_layer::modal);
    const bool clicked = ui::is_clicked(rect, ui::draw_layer::modal);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::draw_rect_f(visual, lerp_color(t.row, t.row_hover, hovered ? 1.0f : 0.0f));
    ui::draw_rect_lines(visual, 2.0f, t.border);
    ui::draw_text_in_rect(label, font_size, visual, t.text);
    return {hovered, pressed, clicked};
}

ui::row_state draw_modal_rule_row(Rectangle rect, bool selected) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect, ui::draw_layer::modal);
    const bool pressed = ui::is_pressed(rect, ui::draw_layer::modal);
    const bool clicked = ui::is_clicked(rect, ui::draw_layer::modal);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    const Color bg = selected ? t.row_selected : t.row;
    const Color bg_hover = selected ? t.row_active : t.row_hover;
    const Color border = selected ? t.border_active : t.border;
    ui::draw_rect_f(visual, lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f));
    ui::draw_rect_lines(visual, 1.4f, border);
    return {hovered, pressed, clicked, visual};
}

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
    const auto& t = *g_theme;
    ui::register_hit_region({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                            ui::draw_layer::modal);
    ui::draw_fullscreen_overlay({0, 0, 0, 168});
    ui::draw_panel(kDialogRect);
    ui::draw_text_in_rect("UNLOCK RULES", 26, kTitleRect, t.text, ui::text_align::left);
    const std::string subtitle = draft_mode_
        ? chart_label_ + " - draft before upload"
        : chart_label_;
    ui::draw_text_in_rect(subtitle.c_str(), 15, kSubtitleRect, t.text_muted, ui::text_align::left);

    if (draw_modal_button(kCloseRect, dirty_ ? "CANCEL" : "CLOSE", 16).clicked) {
        open_ = false;
        return;
    }

    ui::draw_section(kRuleListRect);
    Rectangle row{kRuleListRect.x + 12.0f, kRuleListRect.y + 12.0f, kRuleListRect.width - 24.0f, kRowHeight};
    for (int i = 0; i < static_cast<int>(rules_.size()); ++i) {
        const bool selected = i == selected_rule_;
        const ui::row_state state = draw_modal_rule_row(row, selected);
        ui::draw_text_in_rect(("Rule " + std::to_string(i + 1)).c_str(), 15,
                              {row.x + 12.0f, row.y + 6.0f, row.width - 24.0f, 20.0f},
                              selected ? t.text : t.text_dim, ui::text_align::left);
        ui::draw_text_in_rect(summary_for(rules_[static_cast<size_t>(i)]).c_str(), 11,
                              {row.x + 12.0f, row.y + 31.0f, row.width - 24.0f, 18.0f},
                              t.text_muted, ui::text_align::left);
        if (state.clicked) {
            selected_rule_ = i;
        }
        row.y += kRowHeight + kRowGap;
    }

    ui::draw_section(kEditorRect);
    if (loading_) {
        ui::draw_text_in_rect("Loading...", 18, kEditorRect, t.text_muted);
    } else if (rules_.empty()) {
        ui::draw_text_in_rect("No lock rules. Add one to lock this chart.", 16, kEditorRect, t.text_muted);
    } else {
        selected_rule_ = std::clamp(selected_rule_, 0, static_cast<int>(rules_.size()) - 1);
        unlock_rule_client::rule& rule = rules_[static_cast<size_t>(selected_rule_)];
        Rectangle control{kEditorRect.x + 18.0f, kEditorRect.y + 20.0f, kEditorRect.width - 36.0f, 54.0f};
        const ui::selector_state type_selector =
            ui::draw_value_selector(control, "Type", unlock_rule_client::rule_type_label(rule.type).c_str(),
                                    ui::draw_layer::modal, 18, 34.0f, 154.0f);
        if (type_selector.left.clicked) {
            rule.type = previous_type(rule.type);
            dirty_ = true;
        }
        if (type_selector.right.clicked) {
            rule.type = next_type(rule.type);
            dirty_ = true;
        }
        control.y += 68.0f;
        if (rule.type != unlock_rule_client::rule_type::always_unlocked) {
            int source_index = source_index_for(rule.source_chart_id);
            const std::string source_label = source_label_for(rule.source_chart_id);
            const ui::selector_state source_selector =
                ui::draw_value_selector(control, "Source", source_label.c_str(),
                                        ui::draw_layer::modal, 16, 34.0f, 154.0f);
            const int count = static_cast<int>(source_candidates_.size());
            if (count > 0 && (source_selector.left.clicked || source_selector.right.clicked)) {
                const int delta = source_selector.left.clicked ? -1 : 1;
                for (int step = 0; step < count; ++step) {
                    source_index = (source_index + delta + count) % count;
                    if (source_candidates_[static_cast<size_t>(source_index)].remote_chart_id != remote_chart_id_) {
                        rule.source_chart_id = source_candidates_[static_cast<size_t>(source_index)].remote_chart_id;
                        dirty_ = true;
                        break;
                    }
                }
            }
            control.y += 68.0f;
        }
        if (rule.type == unlock_rule_client::rule_type::clear_rank_at_least) {
            int index = rank_index(rule.min_clear_rank);
            const ui::selector_state rank_selector =
                ui::draw_value_selector(control, "Min rank", rank_options[index],
                                        ui::draw_layer::modal, 18, 34.0f, 154.0f);
            if (rank_selector.left.clicked || rank_selector.right.clicked) {
                const int delta = rank_selector.left.clicked ? -1 : 1;
                index = (index + delta + static_cast<int>(std::size(rank_options))) %
                        static_cast<int>(std::size(rank_options));
                rule.min_clear_rank = rank_options[index];
                dirty_ = true;
            }
            control.y += 68.0f;
        }
        ui::draw_label_value({control.x, control.y, control.width, 44.0f}, "Reason",
                             rule.lock_reason.c_str(), 15, t.text, t.text_dim, 154.0f);
        control.y += 64.0f;
        ui::draw_text_in_rect(summary_for(rule).c_str(), 14,
                              {control.x, control.y, control.width, 54.0f},
                              t.text_muted, ui::text_align::left);
        control.y += 70.0f;
        if (selected_rule_ < static_cast<int>(validations_.size())) {
            const unlock_rule_client::validation& validation = validations_[static_cast<size_t>(selected_rule_)];
            const std::string text = validation.valid
                ? (validation.warnings.empty() ? "Validation OK." : validation.warnings.front())
                : (validation.errors.empty() ? "Validation failed." : validation.errors.front());
            ui::draw_text_in_rect(text.c_str(), 13, {control.x, control.y, control.width, 44.0f},
                                  validation.valid ? t.success : t.error, ui::text_align::left);
        }
    }

    const bool busy = loading_ || saving_ || validating_;
    if (draw_modal_button(kAddRect, "ADD", 16).clicked && !busy && rules_.size() < 8) {
        add_default_rule();
        dirty_ = true;
    }
    if (draw_modal_button(kRemoveRect, "REMOVE", 16).clicked && !busy && !rules_.empty()) {
        rules_.erase(rules_.begin() + selected_rule_);
        selected_rule_ = std::clamp(selected_rule_, 0, std::max(0, static_cast<int>(rules_.size()) - 1));
        dirty_ = true;
    }
    if (draw_modal_button(kValidateRect, validating_ ? "VALIDATING" : "VALIDATE", 16).clicked && !busy) {
        start_validate();
    }
    if (draw_modal_button(kSaveRect, saving_ ? "SAVING" : (draft_mode_ ? "SAVE DRAFT" : "SAVE RULES"), 16).clicked && !busy) {
        start_save();
    }
    ui::draw_text_in_rect(message_.c_str(), 14, kMessageRect,
                          dirty_ ? t.accent : t.text_muted, ui::text_align::left);
}

}  // namespace title_create_unlock_rules
