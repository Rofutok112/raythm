#pragma once

#include <optional>
#include <vector>

namespace title {

enum class modal_id {
    friends,
    rating_rankings,
    self_profile,
    public_profile,
};

class modal_stack {
public:
    void clear();
    void bring_to_front(modal_id id);
    void remove(modal_id id);

    [[nodiscard]] bool contains(modal_id id) const;
    [[nodiscard]] bool is_top(modal_id id) const;
    [[nodiscard]] std::optional<modal_id> top() const;
    [[nodiscard]] const std::vector<modal_id>& order() const;

private:
    std::vector<modal_id> order_;
};

}  // namespace title
