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
    void suspend();
    void resume();
    void restart();

    [[nodiscard]] phase current_phase() const;

private:
    std::string intro_path_;
    std::string loop_path_;
    phase phase_ = phase::stopped;
    bool suspended_ = false;
    bool paused_for_suspend_ = false;
    float current_volume_ = 1.0f;
    float target_volume_ = 1.0f;
};
