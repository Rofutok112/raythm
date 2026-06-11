## 概要
`src/scenes/settings_scene.cpp` は現在 434 行あり、ページ管理、各設定項目の入力処理、即時反映、副作用実行、設定保存、描画までを 1 scene に集約している。ページ単位と設定カテゴリ単位で責務を切り出し、保守しやすい構成へ整理する。

## 背景
- `update()` がページ切り替え、戻る遷移、保存、各ページへの分岐を一括で処理している
- Gameplay / Audio / Video / Key Config の各ページがそれぞれ入力処理と描画を scene 直下のメソッド群で持っている
- Audio / Video ページでは `audio_manager`, `SetWindowSize`, `ToggleFullscreen`, `set_theme` などの副作用が scene 内に直接書かれている
- Key Config は選択状態、リスニング状態、重複チェック、エラー表示まで独自状態を多く持っている

## 分離したい責務
- 設定画面全体のナビゲーション
  - page 切り替え
  - 戻り先制御
  - 保存タイミング
- 設定カテゴリごとの入力処理
  - gameplay sliders
  - audio sliders
  - video selectors
  - key config assignment
- 設定変更の副作用適用
  - `audio_manager` 反映
  - 解像度変更
  - fullscreen 切り替え
  - theme 切り替え
- 描画ロジック
  - sidebar / header
  - page 固有 UI
  - key config のエラー表示

## 提案
- `settings_scene` は page routing と保存導線の管理に寄せる
- 各ページを `settings_gameplay_page`, `settings_audio_page`, `settings_video_page`, `settings_key_config_page` のような単位に分割する
- runtime 副作用は `settings_runtime_applier` のような補助層へ寄せ、scene から直接 raylib / audio API を叩く箇所を減らす
- Key Config の状態機械は独立クラスに分離し、入力待ちと重複検証を局所化する

## 影響範囲
- `src/scenes/settings_scene.cpp`
- `src/scenes/settings_scene.h`
- `src/gameplay/settings_io.*`
- `src/gameplay/game_settings.h`
- `src/audio/audio_manager.cpp`
- `src/scenes/song_select_scene.cpp`
- `src/scenes/title_scene.cpp`
- `src/scenes/theme.h`
- `src/ui/ui_draw.*`
- `src/scenes/virtual_screen.*`
- `src/input/key_names.h`

## リスク
- 設定保存タイミングが変わると title / song select 復帰時の挙動差分が出やすい
- fullscreen と window size の適用順を崩すと表示更新が不安定になりやすい
- key config の duplicate check を分離する際に ESC キャンセルや選択状態の遷移が壊れやすい

## 非目標
- 設定項目の追加
- UI レイアウトやデザインの全面変更
- 設定保存フォーマットの変更

## 完了条件
- ページごとの入力/描画責務が scene 本体から分離されている
- runtime 副作用の適用箇所が整理され、scene 直下の直接呼び出しが減っている
- key config の状態管理が単独で追える
- 戻る導線、保存、即時反映の挙動が現状維持になっている
