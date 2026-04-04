#pragma once

#include <string>

class title_bgm_controller final {
public:
    enum class phase {
        stopped,
        intro,
        loop,
    };

    void configure(std::string intro_path, std::string loop_path);
    void on_enter();
    void on_exit();
    void update();

    [[nodiscard]] phase current_phase() const;

private:
    std::string intro_path_;
    std::string loop_path_;
    phase phase_ = phase::stopped;
};
