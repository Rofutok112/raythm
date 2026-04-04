#pragma once

#include "game_settings.h"

// settings.json からゲーム設定を読み込む。ファイルが存在しない場合はデフォルト値のまま。
void load_settings(game_settings& settings);

// 初回起動向けに AppData と settings.json を用意する。
void initialize_settings_storage(const game_settings& defaults);

// ゲーム設定を settings.json に書き出す。
void save_settings(const game_settings& settings);
