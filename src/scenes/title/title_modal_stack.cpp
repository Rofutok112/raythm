#include "title/title_modal_stack.h"

#include <algorithm>

namespace title {

void modal_stack::clear() {
    order_.clear();
}

void modal_stack::bring_to_front(modal_id id) {
    remove(id);
    order_.push_back(id);
}

void modal_stack::remove(modal_id id) {
    order_.erase(std::remove(order_.begin(), order_.end(), id), order_.end());
}

bool modal_stack::contains(modal_id id) const {
    return std::find(order_.begin(), order_.end(), id) != order_.end();
}

bool modal_stack::is_top(modal_id id) const {
    return !order_.empty() && order_.back() == id;
}

std::optional<modal_id> modal_stack::top() const {
    if (order_.empty()) {
        return std::nullopt;
    }
    return order_.back();
}

const std::vector<modal_id>& modal_stack::order() const {
    return order_;
}

}  // namespace title
