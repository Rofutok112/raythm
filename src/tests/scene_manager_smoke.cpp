#include <cstdlib>
#include <iostream>
#include <memory>

#include "scene.h"
#include "scene_manager.h"

namespace {

struct counters {
    int enter = 0;
    int exit = 0;
    int update = 0;
    int draw = 0;
};

class counting_scene final : public scene {
public:
    counting_scene(scene_manager& manager, counters& own_counters, std::unique_ptr<scene> next = nullptr)
        : scene(manager), counters_(own_counters), next_(std::move(next)) {
    }

    void on_enter() override {
        ++counters_.enter;
    }

    void on_exit() override {
        ++counters_.exit;
    }

    void update(float) override {
        ++counters_.update;
        if (next_) {
            manager_.change_scene(std::move(next_));
        }
    }

    void draw() override {
        ++counters_.draw;
    }

private:
    counters& counters_;
    std::unique_ptr<scene> next_;
};

}  // namespace

int main() {
    scene_manager manager;
    counters first;
    counters second;

    auto next = std::make_unique<counting_scene>(manager, second);
    manager.set_initial_scene(std::make_unique<counting_scene>(manager, first, std::move(next)));

    manager.update(0.016f);
    manager.draw();
    if (first.enter != 1 || first.update != 1 || first.draw != 1 || first.exit != 0 ||
        second.enter != 0 || second.update != 0 || second.draw != 0) {
        std::cerr << "Scene transition requested during update should not draw the next scene in the same frame\n";
        return EXIT_FAILURE;
    }

    manager.update(0.016f);
    manager.draw();
    if (first.exit != 1 || second.enter != 1 || second.update != 1 || second.draw != 1) {
        std::cerr << "Pending scene transition should apply before the next frame update\n";
        return EXIT_FAILURE;
    }

    std::cout << "scene_manager smoke test passed\n";
    return EXIT_SUCCESS;
}
