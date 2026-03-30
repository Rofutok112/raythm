#pragma once

#include <cstdint>
#include <vector>

#include "data_models.h"

struct native_key_event {
    int key = 0;
    input_event_type type = input_event_type::press;
    double timestamp_ms = 0.0;
    std::uint64_t sequence = 0;
};

class windows_input_source final {
public:
    static windows_input_source& instance();

    bool initialize(void* native_window_handle);
    void shutdown();
    bool is_available() const;

    std::vector<native_key_event> drain_events();

    void enable_test_mode();
    void push_test_event(native_key_event event);

private:
    windows_input_source() = default;
};
