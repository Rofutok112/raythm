#pragma once

#include "scene.h"

class title_scene final : public scene {
public:
    explicit title_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;
};

class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;

private:
    int difficulty_index_ = 0;
    int key_mode_index_ = 0;
};

class play_scene final : public scene {
public:
    explicit play_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;

private:
    float elapsed_seconds_ = 0.0f;
};

class result_scene final : public scene {
public:
    explicit result_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;
};

class settings_scene final : public scene {
public:
    explicit settings_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;
};
