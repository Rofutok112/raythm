#pragma once

#include "game_settings.h"

// settings.json からゲーム設定を読み込む。ファイルが存在しない場合はデフォルト値のまま。
void load_settings(game_settings& settings);

// ゲーム設定を settings.json に書き出す。
void save_settings(const game_settings& settings);
