#include "mv/composition/mv_composition_edit_history.h"

#include <algorithm>
#include <utility>

#include "mv/composition/mv_composition_serializer.h"

namespace mv::composition {

const std::string edit_history::kEmptyLabel;

edit_history::edit_history(std::size_t limit)
    : limit_(std::max<std::size_t>(2, limit)) {
}

void edit_history::reset(const mv_composition& composition,
                         std::string selected_layer_id,
                         std::string clean_fingerprint) {
    snapshots_.clear();
    snapshots_.push_back({composition, std::move(selected_layer_id), "Loaded"});
    index_ = 0;
    clean_fingerprint_ = clean_fingerprint.empty() ? fingerprint(composition) : std::move(clean_fingerprint);
}

void edit_history::mark_clean(const mv_composition& composition) {
    clean_fingerprint_ = fingerprint(composition);
}

void edit_history::commit(const mv_composition& composition,
                          std::string selected_layer_id,
                          std::string label) {
    if (snapshots_.empty()) {
        reset(composition, std::move(selected_layer_id), {});
        snapshots_.back().label = std::move(label);
        return;
    }

    if (fingerprint(snapshots_[index_].composition) == fingerprint(composition) &&
        snapshots_[index_].selected_layer_id == selected_layer_id) {
        return;
    }

    snapshots_.erase(snapshots_.begin() + static_cast<std::ptrdiff_t>(index_ + 1), snapshots_.end());
    snapshots_.push_back({composition, std::move(selected_layer_id), std::move(label)});
    index_ = snapshots_.size() - 1;

    while (snapshots_.size() > limit_) {
        snapshots_.erase(snapshots_.begin());
        --index_;
    }
}

bool edit_history::can_undo() const {
    return !snapshots_.empty() && index_ > 0;
}

bool edit_history::can_redo() const {
    return !snapshots_.empty() && index_ + 1 < snapshots_.size();
}

bool edit_history::undo(edit_snapshot& out) {
    if (!can_undo()) {
        return false;
    }
    --index_;
    out = snapshots_[index_];
    return true;
}

bool edit_history::redo(edit_snapshot& out) {
    if (!can_redo()) {
        return false;
    }
    ++index_;
    out = snapshots_[index_];
    return true;
}

bool edit_history::is_clean(const mv_composition& composition) const {
    return !clean_fingerprint_.empty() && fingerprint(composition) == clean_fingerprint_;
}

const std::string& edit_history::undo_label() const {
    return can_undo() ? snapshots_[index_].label : kEmptyLabel;
}

const std::string& edit_history::redo_label() const {
    return can_redo() ? snapshots_[index_ + 1].label : kEmptyLabel;
}

std::size_t edit_history::size() const {
    return snapshots_.size();
}

std::size_t edit_history::index() const {
    return index_;
}

}  // namespace mv::composition
