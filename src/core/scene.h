#pragma once

class scene_manager;

class scene {
public:
    explicit scene(scene_manager& manager);
    virtual ~scene() = default;

    virtual void update(float dt) = 0;
    virtual void draw() = 0;
    virtual void on_enter();
    virtual void on_exit();

protected:
    scene_manager& manager_;
};
