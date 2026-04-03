#include "editor_state.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace {
struct note_span {
    int start_tick = 0;
    int end_tick = 0;
};

note_span make_note_span(const note_data& note) {
    if (note.type == note_type::hold) {
        return {std::min(note.tick, note.end_tick), std::max(note.tick, note.end_tick)};
    }

    return {note.tick, note.tick};
}

bool spans_overlap(const note_span& left, const note_span& right) {
    return left.start_tick <= right.end_tick && right.start_tick <= left.end_tick;
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
    rebuild_timing_engine();
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
        sync_dirty_flag();
    }
    return changed;
}

bool editor_state::redo() {
    const bool changed = history_.redo();
    if (changed) {
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
    sync_dirty_flag();
}

bool editor_state::remove_note(size_t index) {
    if (index >= chart_.notes.size()) {
        return false;
    }

    history_.push(std::make_unique<remove_note_command>(chart_, index));
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_note(size_t index, note_data note) {
    if (index >= chart_.notes.size()) {
        return false;
    }

    history_.push(std::make_unique<modify_note_command>(chart_, index, std::move(note)));
    sync_dirty_flag();
    return true;
}

void editor_state::add_timing_event(timing_event event) {
    history_.push(std::make_unique<add_timing_event_command>(chart_, timing_engine_, std::move(event)));
    sync_dirty_flag();
}

bool editor_state::remove_timing_event(size_t index) {
    if (index >= chart_.timing_events.size()) {
        return false;
    }

    history_.push(std::make_unique<remove_timing_event_command>(chart_, timing_engine_, index));
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_timing_event(size_t index, timing_event event) {
    if (index >= chart_.timing_events.size()) {
        return false;
    }

    history_.push(std::make_unique<modify_timing_event_command>(chart_, timing_engine_, index, std::move(event)));
    sync_dirty_flag();
    return true;
}

bool editor_state::modify_metadata(chart_meta meta, bool clear_notes) {
    if (meta.key_count != chart_.meta.key_count && !clear_notes && !chart_.notes.empty()) {
        return false;
    }

    history_.push(std::make_unique<modify_metadata_command>(chart_, timing_engine_, std::move(meta), clear_notes));
    sync_dirty_flag();
    return true;
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

bool editor_state::has_note_overlap(const note_data& note, std::optional<size_t> ignore_index) const {
    if (note.lane < 0 || note.lane >= chart_.meta.key_count) {
        return true;
    }

    const note_span candidate = make_note_span(note);
    for (size_t i = 0; i < chart_.notes.size(); ++i) {
        if (ignore_index.has_value() && *ignore_index == i) {
            continue;
        }

        const note_data& existing = chart_.notes[i];
        if (existing.lane != note.lane) {
            continue;
        }

        if (spans_overlap(candidate, make_note_span(existing))) {
            return true;
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

void editor_state::sync_dirty_flag() {
    dirty_ = history_.current_index() != saved_history_index_;
}
