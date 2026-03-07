#include "raylib.h"

int main() {
    // ウィンドウ設定
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Rhythm Game - Raylib 5.5");

    // 音楽ゲームに必須：オーディオデバイスの初期化
    InitAudioDevice();

    SetTargetFPS(60);

    // メインループ
    while (!WindowShouldClose()) {
        // --- 更新処理 (Update) ---
        // ここにノーツの落下処理やキー入力判定を追加します

        // --- 描画処理 (Draw) ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Rhythm Game Engine Initialized!", 220, 280, 20, DARKGRAY);
        DrawText("Ready to load some music...", 250, 320, 20, LIGHTGRAY);

        EndDrawing();
    }

    // 終了処理
    CloseAudioDevice();
    CloseWindow();

    return 0;
}