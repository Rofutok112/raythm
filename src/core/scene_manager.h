#pragma once

#include <memory>

class scene;

class scene_manager {
public:
    scene_manager() = default;

    void update(float dt);
    void draw();
    void change_scene(std::unique_ptr<scene> next_scene);
    void set_initial_scene(std::unique_ptr<scene> initial_scene);

private:
    void apply_pending_transition();

    std::unique_ptr<scene> current_scene_;
    std::unique_ptr<scene> next_scene_;
};
