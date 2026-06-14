## 概要
`src/scenes/song_select_scene.cpp` は現在 518 行あり、曲一覧ロード、譜面メタ収集、プレビュー音声、ジャケット管理、スクロール付き選択 UI、アクション遷移、描画アニメーションまでを 1 scene に持っている。データロード、メディア管理、UI 状態管理を分離して見通しを改善する。

## 背景
- `on_enter()` が曲パッケージロード、譜面パース、ソート、プレビュー予約、ジャケット読込まで担っている
- `update_preview()` が preview の fade in/out とループ再開ロジックを scene 状態として管理している
- `update()` がキー操作、ホイールスクロール、スクロールバー drag、行クリック、譜面選択、play/edit/new/settings/title への遷移を一括で処理している
- `draw()` 系が詳細パネル、アクション群、展開式の曲リスト、譜面リスト、フェード演出まで保持している

## 分離したい責務
- 曲/譜面データのロードと整形
  - song_loader 呼び出し
  - chart meta 収集
  - song/charts のソート
  - load error 管理
- 選択状態とスクロール状態の管理
  - selected song
  - selected chart
  - expanded row height
  - scrollbar drag / target scroll
- preview/jacket などのメディア管理
  - preview queue
  - fade 制御
  - texture load/unload
- 画面遷移アクション
  - play / edit / new
  - settings / title
- 描画
  - left detail panel
  - song list
  - chart rows
  - scene fade

## 提案
- `song_select_scene` を orchestration のみへ寄せる
- 例として以下の単位へ分割する
  - `song_catalog_service`
  - `song_select_state`
  - `song_preview_controller`
  - `song_select_list_view` / `song_select_detail_view`
- `play_scene` / `editor_scene` への遷移パラメータ生成を helper 化し、選択中 song/chart の参照ロジックを一箇所へ集約する

## 影響範囲
- `src/scenes/song_select_scene.cpp`
- `src/scenes/song_select_scene.h`
- `src/gameplay/song_loader.*`
- `src/audio/audio_manager.cpp`
- `src/scenes/play_scene.cpp`
- `src/scenes/editor_scene.cpp`
- `src/scenes/settings_scene.cpp`
- `src/scenes/title_scene.cpp`
- `src/scenes/theme.h`
- `src/ui/ui_draw.*`
- `src/ui/ui_clip.h`
- `src/scenes/virtual_screen.*`
- `src/core/data_models.h`

## リスク
- プレビューの fade out/in と次曲切り替え順が崩れると音切れや二重再生が起きやすい
- 選択行の展開高さを扱う責務を動かすと scroll offset と hit test がずれやすい
- `selected_song()` / `filtered_charts_for_selected_song()` の参照タイミングが変わると scene 遷移時の譜面選択が壊れやすい

## 非目標
- 曲選択画面の UX 変更
- editor / play 画面側の機能追加
- song loader のファイルフォーマット変更

## 完了条件
- データロード、メディア管理、UI 状態管理、描画責務が分離されている
- scroll と hit test の対応が単独で追える
- preview/jacket のライフサイクルが scene 本体から切り離されている
- play / edit / new / settings / title への遷移仕様が維持されている
