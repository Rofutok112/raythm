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

    void add_scroll_event(scroll_event event);
    bool remove_scroll_event(size_t index);
    bool modify_scroll_event(size_t index, scroll_event event);
    void add_scroll_automation_point(scroll_automation_point point);
    bool remove_scroll_automation_point(size_t index);
    bool modify_scroll_automation_point(size_t index, scroll_automation_point point);

    bool modify_metadata(chart_meta meta, bool clear_notes = false);

    void refresh_auto_level();
    bool level_needs_refresh() const;
    size_t level_refresh_generation() const;

    const chart_data& data() const;

    const timing_engine& engine() const;

    int snap_tick(int raw_tick, int division) const;
    bool has_note_overlap(const note_data& note, std::optional<size_t> ignore_index = std::nullopt) const;
    bool has_note_overlap(const note_data& note, const std::vector<size_t>& ignore_indices) const;
    bool has_note_overlap(const std::vector<note_data>& notes, const std::vector<size_t>& ignore_indices = {}) const;

    bool is_dirty() const;
    const std::string& file_path() const;
    void set_file_path(std::string file_path);

private:
    void rebuild_timing_engine();
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
};
