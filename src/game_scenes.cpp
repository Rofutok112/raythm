#include "game_scenes.h"

#include <array>
#include <memory>

#include "raylib.h"
#include "scene_manager.h"

namespace {
constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;

void draw_scene_frame(const char* title, const char* subtitle, Color accent) {
    ClearBackground({18, 24, 38, 255});

    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {28, 36, 54, 255}, {12, 16, 26, 255});
    DrawRectangleRounded({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, {245, 247, 250, 235});
    DrawRectangleRoundedLinesEx({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, 3.0f, accent);

    DrawText(title, 130, 130, 44, accent);
    DrawText(subtitle, 130, 190, 24, DARKGRAY);
}
}

title_scene::title_scene(scene_manager& manager) : scene(manager) {
}

void title_scene::update(float dt) {
    (void)dt;

    if (IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    } else if (IsKeyPressed(KEY_S)) {
        manager_.change_scene(std::make_unique<settings_scene>(manager_));
    }
}

void title_scene::draw() {
    draw_scene_frame("Title", "ENTER: Song Select    S: Settings", {32, 129, 226, 255});
    DrawText("raythm", 130, 300, 92, BLACK);
    DrawText("Scene management bootstrap", 136, 400, 28, GRAY);
}

song_select_scene::song_select_scene(scene_manager& manager) : scene(manager) {
}

void song_select_scene::update(float dt) {
    (void)dt;

    constexpr std::array<const char*, 3> kDifficulties = {"Normal", "Hyper", "Another"};
    constexpr std::array<const char*, 2> kKeyModes = {"4 Keys", "6 Keys"};

    if (IsKeyPressed(KEY_LEFT)) {
        difficulty_index_ = (difficulty_index_ + static_cast<int>(kDifficulties.size()) - 1) %
                            static_cast<int>(kDifficulties.size());
    } else if (IsKeyPressed(KEY_RIGHT)) {
        difficulty_index_ = (difficulty_index_ + 1) % static_cast<int>(kDifficulties.size());
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        key_mode_index_ = (key_mode_index_ + 1) % static_cast<int>(kKeyModes.size());
    }

    if (IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<play_scene>(manager_));
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
    }
}

void song_select_scene::draw() {
    constexpr std::array<const char*, 3> kDifficulties = {"Normal", "Hyper", "Another"};
    constexpr std::array<const char*, 2> kKeyModes = {"4 Keys", "6 Keys"};

    draw_scene_frame("Song Select", "LEFT/RIGHT: Difficulty    UP/DOWN: Key Mode    ENTER: Play", {14, 146, 108, 255});
    DrawText("Selected Song: Placeholder Track", 130, 290, 36, BLACK);
    DrawText(TextFormat("Difficulty: %s", kDifficulties[difficulty_index_]), 130, 360, 30, DARKGRAY);
    DrawText(TextFormat("Mode: %s", kKeyModes[key_mode_index_]), 130, 405, 30, DARKGRAY);
    DrawText("ESC: Back to Title", 130, 490, 24, GRAY);
}

play_scene::play_scene(scene_manager& manager) : scene(manager) {
}

void play_scene::update(float dt) {
    elapsed_seconds_ += dt;

    if (IsKeyPressed(KEY_R)) {
        manager_.change_scene(std::make_unique<result_scene>(manager_));
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    }
}

void play_scene::draw() {
    draw_scene_frame("Play", "R: Result    ESC: Song Select", {220, 38, 38, 255});

    DrawRectangle(150, 260, 780, 260, {30, 41, 59, 255});
    DrawRectangleLinesEx({150.0f, 260.0f, 780.0f, 260.0f}, 3.0f, {148, 163, 184, 255});

    for (int lane = 1; lane < 4; ++lane) {
        DrawLine(150 + lane * 195, 260, 150 + lane * 195, 520, {71, 85, 105, 255});
    }

    DrawCircle(245, 330, 18.0f, {96, 165, 250, 255});
    DrawText(TextFormat("Elapsed: %.2f s", elapsed_seconds_), 980, 290, 28, BLACK);
    DrawText("Score: 0000000", 980, 350, 28, BLACK);
    DrawText("Combo: 000", 980, 395, 28, BLACK);
    DrawText("Gauge: 100%", 980, 440, 28, BLACK);
}

result_scene::result_scene(scene_manager& manager) : scene(manager) {
}

void result_scene::update(float dt) {
    (void)dt;

    if (IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
    }
}

void result_scene::draw() {
    draw_scene_frame("Result", "ENTER: Retry Flow    ESC: Title", {202, 138, 4, 255});
    DrawText("Score: 987654", 130, 300, 40, BLACK);
    DrawText("Rank: A", 130, 360, 40, BLACK);
    DrawText("Perfect 321  Great 24  Good 3  Miss 1", 130, 420, 28, DARKGRAY);
}

settings_scene::settings_scene(scene_manager& manager) : scene(manager) {
}

void settings_scene::update(float dt) {
    (void)dt;

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
    }
}

void settings_scene::draw() {
    draw_scene_frame("Settings", "ENTER or ESC: Back to Title", {124, 58, 237, 255});
    DrawText("Audio Offset: 0 ms", 130, 300, 36, BLACK);
    DrawText("Lane Speed: 6.5", 130, 360, 36, BLACK);
    DrawText("Key Config: pending", 130, 420, 36, BLACK);
}
