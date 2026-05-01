#include "scene_manager.h"

#include "scene.h"

void scene_manager::update(float dt) {
    apply_pending_transition();

    if (current_scene_ != nullptr) {
        current_scene_->update(dt);
    }

    apply_pending_transition();
}

void scene_manager::draw() {
    if (current_scene_ != nullptr) {
        current_scene_->draw();
    }
}

void scene_manager::change_scene(std::unique_ptr<scene> next_scene) {
    next_scene_ = std::move(next_scene);
}

void scene_manager::request_exit() {
    exit_requested_ = true;
}

bool scene_manager::exit_requested() const {
    return exit_requested_;
}

void scene_manager::set_initial_scene(std::unique_ptr<scene> initial_scene) {
    current_scene_ = std::move(initial_scene);

    if (current_scene_ != nullptr) {
        current_scene_->on_enter();
    }
}

void scene_manager::shutdown() {
    next_scene_.reset();
    if (current_scene_ != nullptr) {
        current_scene_->on_exit();
        current_scene_.reset();
    }
}

void scene_manager::apply_pending_transition() {
    if (next_scene_ == nullptr) {
        return;
    }

    if (current_scene_ != nullptr) {
        current_scene_->on_exit();
    }

    current_scene_ = std::move(next_scene_);
    current_scene_->on_enter();
}
