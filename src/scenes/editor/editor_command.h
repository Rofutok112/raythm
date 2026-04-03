#pragma once

#include <cstddef>
#include <memory>
#include <vector>

class editor_command {
public:
    virtual ~editor_command() = default;

    virtual void execute() = 0;
    virtual void undo() = 0;
};

class command_history {
public:
    void push(std::unique_ptr<editor_command> command) {
        if (!command) {
            return;
        }

        if (cursor_ < commands_.size()) {
            commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
        }

        command->execute();
        commands_.push_back(std::move(command));
        cursor_ = commands_.size();
    }

    bool undo() {
        if (!can_undo()) {
            return false;
        }

        --cursor_;
        commands_[cursor_]->undo();
        return true;
    }

    bool redo() {
        if (!can_redo()) {
            return false;
        }

        commands_[cursor_]->execute();
        ++cursor_;
        return true;
    }

    bool can_undo() const {
        return cursor_ > 0;
    }

    bool can_redo() const {
        return cursor_ < commands_.size();
    }

    void clear() {
        commands_.clear();
        cursor_ = 0;
    }

    size_t current_index() const {
        return cursor_;
    }

private:
    std::vector<std::unique_ptr<editor_command>> commands_;
    size_t cursor_ = 0;
};
