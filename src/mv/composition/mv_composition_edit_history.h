#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct edit_snapshot {
    mv_composition composition;
    std::string selected_layer_id;
    std::string label;
};

class edit_history {
public:
    explicit edit_history(std::size_t limit = 80);

    void reset(const mv_composition& composition,
               std::string selected_layer_id,
               std::string clean_fingerprint = {});

    void mark_clean(const mv_composition& composition);

    void commit(const mv_composition& composition,
                std::string selected_layer_id,
                std::string label);

    bool can_undo() const;
    bool can_redo() const;
    bool undo(edit_snapshot& out);
    bool redo(edit_snapshot& out);

    bool is_clean(const mv_composition& composition) const;

    const std::string& undo_label() const;
    const std::string& redo_label() const;
    std::size_t size() const;
    std::size_t index() const;

private:
    std::vector<edit_snapshot> snapshots_;
    std::size_t index_ = 0;
    std::size_t limit_ = 80;
    std::string clean_fingerprint_;
    static const std::string kEmptyLabel;
};

}  // namespace mv::composition
