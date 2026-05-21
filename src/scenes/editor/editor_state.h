#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "editor_command.h"
#include "timing_engine.h"

class editor_state {
public:
    editor_state();
    explicit editor_state(chart_data data, std::string file_path = "");

    void load(chart_data data, std::string file_path = "");
    void mark_saved(std::string file_path = "");

    bool undo();
    bool redo();
    bool can_undo() const;
    bool can_redo() const;

    void add_note(note_data note);
    void add_notes(std::vector<note_data> notes);
    bool remove_note(size_t index);
    bool remove_notes(std::vector<size_t> indices);
    bool modify_note(size_t index, note_data note);
    bool modify_notes(std::vector<std::pair<size_t, note_data>> updates);

    void add_timing_event(timing_event event);
    bool remove_timing_event(size_t index);
    bool modify_timing_event(size_t index, timing_event event);

    bool add_scroll_automation_point(scroll_automation_point point);
    bool remove_scroll_automation_point(size_t index);
    bool modify_scroll_automation_point(size_t index, scroll_automation_point point);
    bool modify_scroll_automation_guides(scroll_automation_guides guides);

    bool modify_metadata(chart_meta meta, bool clear_notes = false);

    void refresh_auto_level();
    bool level_needs_refresh() const;
    size_t level_refresh_generation() const;

    const chart_data& data() const;

    const timing_engine& engine() const;

    int snap_tick(int raw_tick, int division) const;
    int max_note_tick() const;
    std::vector<size_t> note_indices_in_tick_range(int min_tick, int max_tick) const;
    bool has_note_overlap(const note_data& note, std::optional<size_t> ignore_index = std::nullopt) const;
    bool has_note_overlap(const note_data& note, const std::vector<size_t>& ignore_indices) const;
    bool has_note_overlap(const std::vector<note_data>& notes, const std::vector<size_t>& ignore_indices = {}) const;

    bool is_dirty() const;
    const std::string& file_path() const;
    void set_file_path(std::string file_path);

private:
    struct note_index_entry {
        int start_tick = 0;
        int end_tick = 0;
        size_t index = 0;
    };

    void rebuild_timing_engine();
    void invalidate_note_index() const;
    void rebuild_note_index() const;
    void mark_level_dirty();
    void sync_dirty_flag();

    chart_data chart_;
    command_history history_;
    timing_engine timing_engine_;
    bool dirty_ = false;
    bool level_dirty_ = false;
    size_t level_refresh_generation_ = 0;
    std::string file_path_;
    size_t saved_history_index_ = 0;
    mutable bool note_index_dirty_ = true;
    mutable int max_note_tick_ = 0;
    mutable std::vector<std::vector<note_index_entry>> note_entries_by_lane_;
    mutable std::vector<std::vector<note_index_entry>> hold_entries_by_lane_;
};
