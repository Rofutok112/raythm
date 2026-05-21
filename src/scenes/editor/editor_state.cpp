#include "editor_state.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "chart_difficulty.h"

namespace {
struct note_span {
    int start_tick = 0;
    int end_tick = 0;
    note_type type = note_type::tap;
};

note_span make_note_span(const note_data& note) {
    if (note.type == note_type::hold) {
        return {std::min(note.tick, note.end_tick), std::max(note.tick, note.end_tick), note.type};
    }

    return {note.tick, note.tick, note.type};
}

bool spans_overlap(const note_span& left, const note_span& right) {
    return left.start_tick <= right.end_tick && right.start_tick <= left.end_tick;
}

bool overlap_allowed(const note_data& left, const note_data& right) {
    if (left.type == note_type::stay || right.type == note_type::stay) {
        return true;
    }
    if ((left.type == note_type::tap && right.type == note_type::release) ||
        (left.type == note_type::release && right.type == note_type::tap)) {
        return left.tick == right.tick;
    }
    if (left.type == note_type::release && right.type == note_type::release) {
        return left.tick == right.tick;
    }
    if (left.type == note_type::hold && right.type == note_type::tap) {
        return left.tick == right.tick;
    }
    if (left.type == note_type::tap && right.type == note_type::hold) {
        return left.tick == right.tick;
    }
    if (left.type == note_type::hold && right.type == note_type::release) {
        return left.end_tick == right.tick;
    }
    if (left.type == note_type::release && right.type == note_type::hold) {
        return left.tick == right.end_tick;
    }
    return false;
}

bool lanes_overlap(const note_data& left, const note_data& right) {
    return left.lane <= note_last_lane(right) && right.lane <= note_last_lane(left);
}

bool same_timing_event(const timing_event& left, const timing_event& right) {
    return left.type == right.type &&
        left.tick == right.tick &&
        left.bpm == right.bpm &&
        left.numerator == right.numerator &&
        left.denominator == right.denominator;
}

bool same_scroll_automation_point(const scroll_automation_point& left, const scroll_automation_point& right) {
    return left.tick == right.tick &&
        left.multiplier == right.multiplier &&
        left.curve_to_next == right.curve_to_next;
}

bool same_scroll_automation_guides(const scroll_automation_guides& left, const scroll_automation_guides& right) {
    return left.values == right.values;
}

bool same_scroll_automation_position(const scroll_automation_point& left,
                                     const scroll_automation_point& right) {
    return left.tick == right.tick && std::fabs(left.multiplier - right.multiplier) < 0.0001f;
}

bool has_scroll_automation_point_at_position(const chart_data& chart,
                                             const scroll_automation_point& point,
                                             std::optional<size_t> ignore_index = std::nullopt) {
    for (size_t index = 0; index < chart.scroll_automation.size(); ++index) {
        if (ignore_index.has_value() && *ignore_index == index) {
            continue;
        }
        if (same_scroll_automation_position(chart.scroll_automation[index], point)) {
            return true;
        }
    }
    return false;
}

bool same_note_data(const note_data& left, const note_data& right) {
    return left.type == right.type &&
        left.tick == right.tick &&
        left.lane == right.lane &&
        left.end_tick == right.end_tick &&
        left.is_ray == right.is_ray &&
        note_lane_width(left) == note_lane_width(right);
}

bool same_chart_meta(const chart_meta& left, const chart_meta& right) {
    return left.chart_id == right.chart_id &&
        left.song_id == right.song_id &&
        left.chart_version == right.chart_version &&
        left.key_count == right.key_count &&
        left.difficulty == right.difficulty &&
        left.level == right.level &&
        left.chart_author == right.chart_author &&
        left.format_version == right.format_version &&
        left.resolution == right.resolution &&
        left.offset == right.offset;
}

class add_note_command final : public editor_command {
public:
    add_note_command(chart_data& chart, note_data note) : chart_(chart), note_(std::move(note)) {}

    void execute() override {
        chart_.notes.push_back(note_);
    }

    void undo() override {
        chart_.notes.pop_back();
    }

private:
    chart_data& chart_;
    note_data note_;
};

class add_notes_command final : public editor_command {
public:
    add_notes_command(chart_data& chart, std::vector<note_data> notes) : chart_(chart), notes_(std::move(notes)) {}

    void execute() override {
        chart_.notes.insert(chart_.notes.end(), notes_.begin(), notes_.end());
    }

    void undo() override {
        if (notes_.size() <= chart_.notes.size()) {
            chart_.notes.erase(chart_.notes.end() - static_cast<std::ptrdiff_t>(notes_.size()), chart_.notes.end());
        }
    }

private:
    chart_data& chart_;
    std::vector<note_data> notes_;
};

class remove_note_command final : public editor_command {
public:
    remove_note_command(chart_data& chart, size_t index) : chart_(chart), index_(index), removed_(chart.notes[index]) {}

    void execute() override {
        chart_.notes.erase(chart_.notes.begin() + static_cast<std::ptrdiff_t>(index_));
    }

    void undo() override {
        chart_.notes.insert(chart_.notes.begin() + static_cast<std::ptrdiff_t>(index_), removed_);
    }

private:
    chart_data& chart_;
    size_t index_ = 0;
    note_data removed_;
};

class remove_notes_command final : public editor_command {
public:
    remove_notes_command(chart_data& chart, std::vector<size_t> indices) : chart_(chart), indices_(std::move(indices)) {
        std::sort(indices_.begin(), indices_.end());
        indices_.erase(std::unique(indices_.begin(), indices_.end()), indices_.end());
        removed_.reserve(indices_.size());
        for (const size_t index : indices_) {
            removed_.push_back(chart_.notes[index]);
        }
    }

    void execute() override {
        for (size_t i = indices_.size(); i > 0; --i) {
            const size_t index = indices_[i - 1];
            chart_.notes.erase(chart_.notes.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void undo() override {
        for (size_t i = 0; i < indices_.size(); ++i) {
            chart_.notes.insert(chart_.notes.begin() + static_cast<std::ptrdiff_t>(indices_[i]), removed_[i]);
        }
    }

private:
    chart_data& chart_;
    std::vector<size_t> indices_;
    std::vector<note_data> removed_;
};

class modify_note_command final : public editor_command {
public:
    modify_note_command(chart_data& chart, size_t index, note_data updated)
        : chart_(chart), index_(index), before_(chart.notes[index]), after_(std::move(updated)) {}

    void execute() override {
        chart_.notes[index_] = after_;
    }

    void undo() override {
        chart_.notes[index_] = before_;
    }

private:
    chart_data& chart_;
    size_t index_ = 0;
    note_data before_;
    note_data after_;
};

class modify_notes_command final : public editor_command {
public:
    modify_notes_command(chart_data& chart, std::vector<std::pair<size_t, note_data>> updates)
        : chart_(chart), updates_(std::move(updates)) {
        std::sort(updates_.begin(), updates_.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
        before_.reserve(updates_.size());
        for (const auto& update : updates_) {
            before_.push_back(chart_.notes[update.first]);
        }
    }

    void execute() override {
        for (const auto& update : updates_) {
            chart_.notes[update.first] = update.second;
        }
    }

    void undo() override {
        for (size_t i = 0; i < updates_.size(); ++i) {
            chart_.notes[updates_[i].first] = before_[i];
        }
    }

private:
    chart_data& chart_;
    std::vector<std::pair<size_t, note_data>> updates_;
    std::vector<note_data> before_;
};

class add_timing_event_command final : public editor_command {
public:
    add_timing_event_command(chart_data& chart, timing_engine& engine, timing_event event)
        : chart_(chart), engine_(engine), event_(std::move(event)) {}

    void execute() override {
        chart_.timing_events.push_back(event_);
        rebuild();
    }

    void undo() override {
        chart_.timing_events.pop_back();
        rebuild();
    }

private:
    void rebuild() {
        engine_.init(chart_.timing_events, chart_.meta.resolution, chart_.meta.offset);
    }

    chart_data& chart_;
    timing_engine& engine_;
    timing_event event_;
};

class remove_timing_event_command final : public editor_command {
public:
    remove_timing_event_command(chart_data& chart, timing_engine& engine, size_t index)
        : chart_(chart), engine_(engine), index_(index), removed_(chart.timing_events[index]) {}

    void execute() override {
        chart_.timing_events.erase(chart_.timing_events.begin() + static_cast<std::ptrdiff_t>(index_));
        rebuild();
    }

    void undo() override {
        chart_.timing_events.insert(chart_.timing_events.begin() + static_cast<std::ptrdiff_t>(index_), removed_);
        rebuild();
    }

private:
    void rebuild() {
        engine_.init(chart_.timing_events, chart_.meta.resolution, chart_.meta.offset);
    }

    chart_data& chart_;
    timing_engine& engine_;
    size_t index_ = 0;
    timing_event removed_;
};

class modify_timing_event_command final : public editor_command {
public:
    modify_timing_event_command(chart_data& chart, timing_engine& engine, size_t index, timing_event updated)
        : chart_(chart), engine_(engine), index_(index), before_(chart.timing_events[index]), after_(std::move(updated)) {}

    void execute() override {
        chart_.timing_events[index_] = after_;
        rebuild();
    }

    void undo() override {
        chart_.timing_events[index_] = before_;
        rebuild();
    }

private:
    void rebuild() {
        engine_.init(chart_.timing_events, chart_.meta.resolution, chart_.meta.offset);
    }

    chart_data& chart_;
    timing_engine& engine_;
    size_t index_ = 0;
    timing_event before_;
    timing_event after_;
};

class add_scroll_automation_point_command final : public editor_command {
public:
    add_scroll_automation_point_command(chart_data& chart, scroll_automation_point point)
        : chart_(chart), point_(std::move(point)) {}

    void execute() override {
        chart_.scroll_automation.push_back(point_);
    }

    void undo() override {
        chart_.scroll_automation.pop_back();
    }

private:
    chart_data& chart_;
    scroll_automation_point point_;
};

class remove_scroll_automation_point_command final : public editor_command {
public:
    remove_scroll_automation_point_command(chart_data& chart, size_t index)
        : chart_(chart), index_(index), removed_(chart.scroll_automation[index]) {}

    void execute() override {
        chart_.scroll_automation.erase(chart_.scroll_automation.begin() + static_cast<std::ptrdiff_t>(index_));
    }

    void undo() override {
        chart_.scroll_automation.insert(chart_.scroll_automation.begin() + static_cast<std::ptrdiff_t>(index_),
                                        removed_);
    }

private:
    chart_data& chart_;
    size_t index_ = 0;
    scroll_automation_point removed_;
};

class modify_scroll_automation_point_command final : public editor_command {
public:
    modify_scroll_automation_point_command(chart_data& chart, size_t index, scroll_automation_point updated)
        : chart_(chart), index_(index), before_(chart.scroll_automation[index]), after_(std::move(updated)) {}

    void execute() override {
        chart_.scroll_automation[index_] = after_;
    }

    void undo() override {
        chart_.scroll_automation[index_] = before_;
    }

private:
    chart_data& chart_;
    size_t index_ = 0;
    scroll_automation_point before_;
    scroll_automation_point after_;
};

class modify_scroll_automation_guides_command final : public editor_command {
public:
    modify_scroll_automation_guides_command(chart_data& chart, scroll_automation_guides updated)
        : chart_(chart), before_(chart.scroll_guides), after_(std::move(updated)) {}

    void execute() override {
        chart_.scroll_guides = after_;
    }

    void undo() override {
        chart_.scroll_guides = before_;
    }

private:
    chart_data& chart_;
    scroll_automation_guides before_;
    scroll_automation_guides after_;
};

class modify_metadata_command final : public editor_command {
public:
    modify_metadata_command(chart_data& chart, timing_engine& engine, chart_meta updated, bool clear_notes)
        : chart_(chart), engine_(engine), before_(chart), after_(chart) {
        after_.meta = std::move(updated);
        if (clear_notes) {
            after_.notes.clear();
        }
    }

    void execute() override {
        chart_ = after_;
        rebuild();
    }

    void undo() override {
        chart_ = before_;
        rebuild();
    }

private:
    void rebuild() {
        engine_.init(chart_.timing_events, chart_.meta.resolution, chart_.meta.offset);
    }

    chart_data& chart_;
    timing_engine& engine_;
    chart_data before_;
    chart_data after_;
};
}

editor_state::editor_state() {
    chart_.meta.resolution = 480;
    rebuild_timing_engine();
}

editor_state::editor_state(chart_data data, std::string file_path) {
    load(std::move(data), std::move(file_path));
}

void editor_state::load(chart_data data, std::string file_path) {
    chart_ = std::move(data);
    file_path_ = std::move(file_path);
    history_.clear();
    saved_history_index_ = 0;
    dirty_ = false;
    invalidate_note_index();
    rebuild_timing_engine();
    refresh_auto_level();
}

void editor_state::mark_saved(std::string file_path) {
    if (!file_path.empty()) {
        file_path_ = std::move(file_path);
    }

    saved_history_index_ = history_.current_index();
    sync_dirty_flag();
}

bool editor_state::undo() {
    const bool changed = history_.undo();
    if (changed) {
        invalidate_note_index();
        mark_level_dirty();
        sync_dirty_flag();
    }
    return changed;
}

bool editor_state::redo() {
    const bool changed = history_.redo();
    if (changed) {
        invalidate_note_index();
        mark_level_dirty();
        sync_dirty_flag();
    }
    return changed;
}

bool editor_state::can_undo() const {
    return history_.can_undo();
}

bool editor_state::can_redo() const {
    return history_.can_redo();
}

void editor_state::add_note(note_data note) {
    history_.push(std::make_unique<add_note_command>(chart_, std::move(note)));
    invalidate_note_index();
    mark_level_dirty();
    sync_dirty_flag();
}

void editor_state::add_notes(std::vector<note_data> notes) {
    if (notes.empty()) {
        return;
    }

    history_.push(std::make_unique<add_notes_command>(chart_, std::move(notes)));
    invalidate_note_index();
    mark_level_dirty();
    sync_dirty_flag();
}

bool editor_state::remove_note(size_t index) {
    if (index >= chart_.notes.size()) {
        return false;
    }

    history_.push(std::make_unique<remove_note_command>(chart_, index));
    invalidate_note_index();
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::remove_notes(std::vector<size_t> indices) {
    if (indices.empty()) {
        return false;
    }

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (std::any_of(indices.begin(), indices.end(), [this](size_t index) {
            return index >= chart_.notes.size();
        })) {
        return false;
    }

    history_.push(std::make_unique<remove_notes_command>(chart_, std::move(indices)));
    invalidate_note_index();
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_note(size_t index, note_data note) {
    if (index >= chart_.notes.size()) {
        return false;
    }

    if (same_note_data(chart_.notes[index], note)) {
        return true;
    }

    history_.push(std::make_unique<modify_note_command>(chart_, index, std::move(note)));
    invalidate_note_index();
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_notes(std::vector<std::pair<size_t, note_data>> updates) {
    if (updates.empty()) {
        return false;
    }

    std::sort(updates.begin(), updates.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    for (size_t i = 0; i < updates.size(); ++i) {
        if (updates[i].first >= chart_.notes.size() ||
            (i > 0 && updates[i - 1].first == updates[i].first)) {
            return false;
        }
    }
    updates.erase(std::remove_if(updates.begin(), updates.end(), [this](const auto& update) {
        return same_note_data(chart_.notes[update.first], update.second);
    }), updates.end());
    if (updates.empty()) {
        return true;
    }

    history_.push(std::make_unique<modify_notes_command>(chart_, std::move(updates)));
    invalidate_note_index();
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

void editor_state::add_timing_event(timing_event event) {
    history_.push(std::make_unique<add_timing_event_command>(chart_, timing_engine_, std::move(event)));
    mark_level_dirty();
    sync_dirty_flag();
}

bool editor_state::remove_timing_event(size_t index) {
    if (index >= chart_.timing_events.size()) {
        return false;
    }

    history_.push(std::make_unique<remove_timing_event_command>(chart_, timing_engine_, index));
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_timing_event(size_t index, timing_event event) {
    if (index >= chart_.timing_events.size()) {
        return false;
    }

    if (same_timing_event(chart_.timing_events[index], event)) {
        return true;
    }

    history_.push(std::make_unique<modify_timing_event_command>(chart_, timing_engine_, index, std::move(event)));
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::add_scroll_automation_point(scroll_automation_point point) {
    if (has_scroll_automation_point_at_position(chart_, point)) {
        return false;
    }
    history_.push(std::make_unique<add_scroll_automation_point_command>(chart_, std::move(point)));
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::remove_scroll_automation_point(size_t index) {
    if (index >= chart_.scroll_automation.size()) {
        return false;
    }

    history_.push(std::make_unique<remove_scroll_automation_point_command>(chart_, index));
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_scroll_automation_point(size_t index, scroll_automation_point point) {
    if (index >= chart_.scroll_automation.size()) {
        return false;
    }

    if (same_scroll_automation_point(chart_.scroll_automation[index], point)) {
        return true;
    }

    history_.push(std::make_unique<modify_scroll_automation_point_command>(chart_, index, std::move(point)));
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_scroll_automation_guides(scroll_automation_guides guides) {
    if (same_scroll_automation_guides(chart_.scroll_guides, guides)) {
        return true;
    }

    history_.push(std::make_unique<modify_scroll_automation_guides_command>(chart_, std::move(guides)));
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_metadata(chart_meta meta, bool clear_notes) {
    if (meta.key_count != chart_.meta.key_count && !clear_notes && !chart_.notes.empty()) {
        return false;
    }

    if (!clear_notes && same_chart_meta(chart_.meta, meta)) {
        return true;
    }

    history_.push(std::make_unique<modify_metadata_command>(chart_, timing_engine_, std::move(meta), clear_notes));
    if (clear_notes) {
        invalidate_note_index();
    }
    mark_level_dirty();
    sync_dirty_flag();
    return true;
}

void editor_state::refresh_auto_level() {
    chart_difficulty::apply_auto_level(chart_);
    level_dirty_ = false;
}

bool editor_state::level_needs_refresh() const {
    return level_dirty_;
}

size_t editor_state::level_refresh_generation() const {
    return level_refresh_generation_;
}

const chart_data& editor_state::data() const {
    return chart_;
}

const timing_engine& editor_state::engine() const {
    return timing_engine_;
}

int editor_state::snap_tick(int raw_tick, int division) const {
    const int clamped_division = std::max(1, division);
    const int interval = std::max(1, chart_.meta.resolution * 4 / clamped_division);
    return std::max(0, static_cast<int>(std::lround(static_cast<double>(raw_tick) / interval)) * interval);
}

int editor_state::max_note_tick() const {
    rebuild_note_index();
    return max_note_tick_;
}

std::vector<size_t> editor_state::note_indices_in_tick_range(int min_tick, int max_tick) const {
    if (max_tick < min_tick) {
        return {};
    }
    rebuild_note_index();

    std::vector<size_t> indices;
    for (size_t lane = 0; lane < note_entries_by_lane_.size(); ++lane) {
        const std::vector<note_index_entry>& lane_entries = note_entries_by_lane_[lane];
        const auto start_upper = std::upper_bound(
            lane_entries.begin(), lane_entries.end(), max_tick,
            [](int tick, const note_index_entry& entry) {
                return tick < entry.start_tick;
            });
        for (auto it = start_upper; it != lane_entries.begin();) {
            --it;
            if (it->start_tick < min_tick) {
                break;
            }
            if (it->end_tick >= min_tick) {
                indices.push_back(it->index);
            }
        }

        const std::vector<note_index_entry>& hold_entries = hold_entries_by_lane_[lane];
        const auto end_lower = std::lower_bound(
            hold_entries.begin(), hold_entries.end(), min_tick,
            [](const note_index_entry& entry, int tick) {
                return entry.end_tick < tick;
            });
        for (auto it = end_lower; it != hold_entries.end(); ++it) {
            if (it->start_tick >= min_tick) {
                continue;
            }
            if (it->start_tick <= max_tick) {
                indices.push_back(it->index);
            }
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

bool editor_state::has_note_overlap(const note_data& note, std::optional<size_t> ignore_index) const {
    std::vector<size_t> ignore_indices;
    if (ignore_index.has_value()) {
        ignore_indices.push_back(*ignore_index);
    }
    return has_note_overlap(note, ignore_indices);
}

bool editor_state::has_note_overlap(const note_data& note, const std::vector<size_t>& ignore_indices) const {
    if (note.lane < 0 || note.lane >= chart_.meta.key_count ||
        note_lane_width(note) <= 0 || note_last_lane(note) >= chart_.meta.key_count) {
        return true;
    }

    std::vector<size_t> ignored = ignore_indices;
    std::sort(ignored.begin(), ignored.end());
    ignored.erase(std::unique(ignored.begin(), ignored.end()), ignored.end());
    const note_span candidate = make_note_span(note);
    const std::vector<size_t> nearby_indices = note_indices_in_tick_range(candidate.start_tick, candidate.end_tick);
    for (const size_t index : nearby_indices) {
        if (std::binary_search(ignored.begin(), ignored.end(), index)) {
            continue;
        }

        const note_data& existing = chart_.notes[index];
        if (!lanes_overlap(existing, note)) {
            continue;
        }

        if (spans_overlap(candidate, make_note_span(existing)) &&
            !overlap_allowed(note, existing)) {
            return true;
        }
    }

    return false;
}

bool editor_state::has_note_overlap(const std::vector<note_data>& notes, const std::vector<size_t>& ignore_indices) const {
    for (size_t i = 0; i < notes.size(); ++i) {
        if (has_note_overlap(notes[i], ignore_indices)) {
            return true;
        }
        for (size_t j = i + 1; j < notes.size(); ++j) {
            if (lanes_overlap(notes[i], notes[j]) &&
                spans_overlap(make_note_span(notes[i]), make_note_span(notes[j])) &&
                !overlap_allowed(notes[i], notes[j])) {
                return true;
            }
        }
    }
    return false;
}

bool editor_state::is_dirty() const {
    return dirty_;
}

const std::string& editor_state::file_path() const {
    return file_path_;
}

void editor_state::set_file_path(std::string file_path) {
    file_path_ = std::move(file_path);
}

void editor_state::rebuild_timing_engine() {
    timing_engine_.init(chart_.timing_events, chart_.meta.resolution, chart_.meta.offset);
}

void editor_state::invalidate_note_index() const {
    note_index_dirty_ = true;
}

void editor_state::rebuild_note_index() const {
    if (!note_index_dirty_) {
        return;
    }

    const int key_count = std::max(1, chart_.meta.key_count);
    max_note_tick_ = 0;
    note_entries_by_lane_.assign(static_cast<size_t>(key_count), {});
    hold_entries_by_lane_.assign(static_cast<size_t>(key_count), {});
    for (size_t index = 0; index < chart_.notes.size(); ++index) {
        const note_data& note = chart_.notes[index];
        const note_span span = make_note_span(note);
        max_note_tick_ = std::max(max_note_tick_, span.end_tick);
        if (note.lane < 0 || note.lane >= key_count ||
            note_lane_width(note) <= 0 || note_last_lane(note) >= key_count) {
            continue;
        }
        const note_index_entry entry{span.start_tick, span.end_tick, index};
        for (int lane = note.lane; lane <= note_last_lane(note); ++lane) {
            note_entries_by_lane_[static_cast<size_t>(lane)].push_back(entry);
            if (span.end_tick > span.start_tick) {
                hold_entries_by_lane_[static_cast<size_t>(lane)].push_back(entry);
            }
        }
    }

    for (std::vector<note_index_entry>& lane_entries : note_entries_by_lane_) {
        std::stable_sort(lane_entries.begin(), lane_entries.end(), [](const note_index_entry& left,
                                                                      const note_index_entry& right) {
            if (left.start_tick != right.start_tick) {
                return left.start_tick < right.start_tick;
            }
            return left.index < right.index;
        });
    }
    for (std::vector<note_index_entry>& lane_entries : hold_entries_by_lane_) {
        std::stable_sort(lane_entries.begin(), lane_entries.end(), [](const note_index_entry& left,
                                                                      const note_index_entry& right) {
            if (left.end_tick != right.end_tick) {
                return left.end_tick < right.end_tick;
            }
            return left.index < right.index;
        });
    }
    note_index_dirty_ = false;
}

void editor_state::mark_level_dirty() {
    level_dirty_ = true;
    ++level_refresh_generation_;
}

void editor_state::sync_dirty_flag() {
    dirty_ = history_.current_index() != saved_history_index_;
}
