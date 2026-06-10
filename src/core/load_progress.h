#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>

struct load_progress {
    std::string message;
    float progress = 0.0f;
    bool active = false;
};

class shared_load_progress {
public:
    void set(std::string message, float progress, bool active = true) {
        std::lock_guard lock(mutex_);
        snapshot_.message = std::move(message);
        snapshot_.progress = std::clamp(progress, 0.0f, 1.0f);
        snapshot_.active = active;
    }

    [[nodiscard]] load_progress snapshot() const {
        std::lock_guard lock(mutex_);
        return snapshot_;
    }

private:
    mutable std::mutex mutex_;
    load_progress snapshot_;
};
