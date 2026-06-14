## 概要
`src/scenes/play_scene.cpp` は現在 710 行あり、譜面ロード、ゲーム進行、入力判定、スコア/ゲージ更新、演出状態遷移、3D/2D 描画、シーン遷移までを 1 クラスに集約している。責務を分離し、変更影響を局所化できる構成へ整理する。

## 背景
- `on_enter()` がサンプル曲ロード、譜面選択、音声ロード、判定系初期化、描画キュー初期化まで担っている
- `update()` がポーズ制御、フォーカス喪失時の自動ポーズ、イントロ、失敗遷移、リザルト遷移、入力更新、判定反映、SE 再生、シーン遷移を一括で処理している
- `draw()` 系が 3D レーン/ノート描画、HUD、ポーズ UI、ジャッジ表示、フェード演出をまとめて保持している
- 描画用スライディングウィンドウ更新 (`update_draw_queues`) も scene 本体の状態と密結合している

## 分離したい責務
- プレイセッション初期化
  - 曲/譜面解決
  - 判定/スコア/ゲージ/描画キューの初期化
  - オーディオロード
- プレイ中の状態遷移
  - intro / paused / failure / result transition の制御
  - フォーカス喪失時の自動ポーズ
  - scene_manager への遷移条件集約
- ゲーム進行ロジック
  - audio clock からの時刻取得
  - input_handler / judge_system / score_system / gauge の更新
  - hitsound 再生と judge feedback 更新
- 描画ロジック
  - 3D カメラ計算
  - レーン/ノート描画
  - HUD / overlay 描画
- 描画キュー管理
  - inactive / active ノートの昇格と除去
  - visual_ms に基づく可視範囲更新

## 提案
- `play_scene` は scene のライフサイクルと高水準のオーケストレーションに絞る
- 以下のような補助コンポーネントへ分割する
  - `play_session_loader` または `play_session_state`
  - `play_flow_controller`
  - `play_renderer_3d` / `play_hud_renderer`
  - `play_note_draw_queue`
- 依存の方向を明確にし、`scene_manager` と `audio_manager` への直接依存を最小の層へ寄せる

## 影響範囲
- `src/scenes/play_scene.cpp`
- `src/scenes/play_scene.h`
- `src/scenes/result_scene.cpp`
- `src/scenes/song_select_scene.cpp`
- `src/audio/audio_manager.cpp`
- `src/gameplay/judge_system.*`
- `src/gameplay/score_system.*`
- `src/gameplay/timing_engine.*`
- `src/gameplay/song_loader.*`
- `src/core/scene_manager.*`
- `src/scenes/scene_common.*`
- `src/scenes/theme.h`
- `src/ui/ui_draw.*`
- `src/scenes/virtual_screen.*`

## リスク
- pause / intro / result transition の順序が崩れるとプレイフローが壊れやすい
- draw queue の責務を動かすと可視ノートの出入りや hold 描画の境界が崩れやすい
- audio clock と `current_ms_` の同期点を変えると判定タイミングに影響する

## 非目標
- 判定幅やスコア計算仕様の変更
- UI デザインの変更
- note 表示仕様の変更

## 完了条件
- `play_scene` から少なくとも初期化、ゲーム進行、描画、演出/遷移制御の責務が分離されている
- 主要な scene 遷移条件が一箇所で追える
- 描画キュー更新と判定更新の責務境界が明確になっている
- 既存のプレイ開始、ポーズ、失敗、リザルト遷移の挙動が維持されている
