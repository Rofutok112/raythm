#pragma once

#include <string_view>

namespace updater {

class progress_window {
public:
    progress_window();
    ~progress_window();

    void set_title(std::string_view title);
    void set_status(std::string_view status);
    void set_progress(float progress);
    void process_events();
    void show_error(std::string_view message);

private:
    struct impl;
    impl* impl_ = nullptr;
};

}  // namespace updater
