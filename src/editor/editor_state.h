#pragma once

#include <cstddef>
#include <optional>
#include <string>

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
    bool remove_note(size_t index);
    bool modify_note(size_t index, note_data note);

    void add_timing_event(timing_event event);
    bool remove_timing_event(size_t index);
    bool modify_timing_event(size_t index, timing_event event);

    void modify_metadata(chart_meta meta);

    const chart_data& data() const;

    const timing_engine& engine() const;

    int snap_tick(int raw_tick, int division) const;
    bool has_note_overlap(const note_data& note, std::optional<size_t> ignore_index = std::nullopt) const;

    bool is_dirty() const;
    const std::string& file_path() const;
    void set_file_path(std::string file_path);

private:
    void rebuild_timing_engine();
    void sync_dirty_flag();

    chart_data chart_;
    command_history history_;
    timing_engine timing_engine_;
    bool dirty_ = false;
    std::string file_path_;
    size_t saved_history_index_ = 0;
};
