---
name: raythm-architecture
description: Ideal architecture guidance for the raythm rhythm game. Use when designing, refactoring, reviewing, or planning code structure for raythm features, especially scene boundaries, gameplay systems, editor/song-select/title flows, data models, services, controllers, renderers, async work, and long-term maintainability.
---

# Raythm Architecture

このスキルは、raythm を「小さなゲーム実装の寄せ集め」ではなく、長く育てられるリズムゲームアプリケーションとして設計するための基準を与える。既存コードの都合に引っ張られず、理想形から逆算して判断すること。実装時は一度に大改造せず、触る機能の境界を理想形へ少しずつ近づける。

## 基本方針

raythm のアーキテクチャは、Scene ベースのアプリケーション構造を土台にしつつ、Scene を責務の中心にしない。Scene はライフサイクル、画面遷移、入力収集、各構成要素の呼び出し順をまとめる薄い組み立て役にする。ゲームの本質的なルール、画面固有の状態遷移、描画、永続化、通信、音声制御はそれぞれ別の層に分ける。

最優先する依存方向は、内側から外側へ流れる一方向の構造である。譜面、タイミング、判定、スコア、リザルトなどの Gameplay Domain は raylib、Scene、UI、ネットワーク、ファイルダイアログを知らない。Scene や View は Domain を使ってよいが、Domain が Scene や View を参照してはいけない。

巨大な汎用エンジンや ECS を先に導入しない。raythm では、Feature ごとに State、Controller、View/Renderer、Service を分ける構造を基本とする。この形は C++ と raylib の素直さを保ちつつ、リズムゲーム特有の時間、入力、譜面、音声同期、エディタ操作をテスト可能な単位へ分割できる。

## レイヤー

理想的な構成は次の責務分割に従う。

```text
core/
  アプリケーション起動、メインループ、Scene 管理、パス、プラットフォーム境界

models/
  曲、譜面、ノート、タイミング、判定結果などの純粋データ

gameplay/
  タイミング変換、判定、スコア、ゲージ、パフォーマンス計算、譜面解析補助

features/
  play/
    play_scene, play_session_state, play_flow_controller, play_renderer, play_session_loader
  editor/
    editor_scene, editor_state, controllers, services, views
  song_select/
    state, controllers, services, views
  title/
    state, controllers, services, views

infrastructure/
  audio, network, sqlite, filesystem, platform API, external process
```

この分類は物理ディレクトリ名よりも責務の境界として扱う。既存の場所に実装する場合でも、依存方向と命名はこの考え方に合わせる。

## Scene

Scene は Feature の入口であり、アプリケーションライフサイクルの接着剤である。Scene に入れてよいのは、初期化、終了処理、フレームごとの入力収集、Controller 呼び出し、Renderer 呼び出し、Scene 遷移の適用である。

Scene に入れてはいけないものは、判定ルール、スコア計算、カタログ読み込みの詳細、アップロード処理、DB 操作、JSON 変換、長い描画手続き、UI 部品ごとの状態遷移である。Scene が肥大化したら、まず「状態を変える処理」と「描くだけの処理」を分ける。

Scene は子要素の所有者であってよい。ただし、状態の意味を Scene の private 変数群だけで表現しない。Feature の状態は `*_state` にまとめ、処理のまとまりは `*_controller` または `*_service` に移す。

## Feature

Feature は、ユーザーから見た一つのまとまった画面または体験を表す。Play、Editor、Song Select、Title、Result、Settings などを Feature として考える。

各 Feature は原則として次の形に分ける。

```text
*_state
  画面の現在状態を表すデータ。選択、モード、ロード状態、ダイアログ状態、アニメーション値を持つ。

*_controller
  入力、時間経過、非同期結果を受け取り、state を更新し、外側へ必要な要求を返す。

*_view または *_renderer
  state と layout model を読み、描画だけを行う。永続化、通信、Scene 遷移をしない。

*_service
  ファイル、DB、ネットワーク、インポート、エクスポート、重い計算など、画面状態から独立した処理を行う。
```

Controller は可能な限り `context -> result` の形にする。現在のフレームで観測した入力や外部状態を context として渡し、音声再生、Scene 遷移、ファイル保存、通知表示などの副作用要求を result として返す。これにより Controller をテストしやすくし、Scene に副作用の適用場所を集める。

## Gameplay Domain

Gameplay Domain は raythm の中核である。ここには、譜面データ、タイミング変換、スクロール計算、入力イベント解釈、判定、スコア、ゲージ、リザルト集計を置く。これらは可能な限り決定的で、同じ入力なら同じ出力になるようにする。

Gameplay Domain は、音声デバイスから直接時刻を読まない。現在時刻や入力イベントは外から渡す。これにより、実プレイ、リプレイ、エディタ内再生、テストで同じ判定ロジックを使える。

判定システムは、描画フィードバックや効果音再生を直接行わない。判定イベントを発行し、Play Feature がそれをスコア、ゲージ、表示、効果音へ分配する。判定イベントには「ゲームプレイ効果を適用するか」「表示するか」「効果音を鳴らすか」のような意味を持たせてよいが、実際の副作用は外側で行う。

## Models

共有モデルは、アプリケーション全体で意味が安定しているデータだけを置く。曲、譜面、ノート、タイミング、スクロール、判定結果、リザルトなどは共有してよい。

画面表示専用、通信 API 専用、DB 専用、エディタ UI 専用の項目を安易に共有モデルへ混ぜない。必要なら変換用の DTO や Feature 固有モデルを作る。共有モデルが膨らむほど、依存関係は静かに硬くなる。

単位はモデル内で明示する。時間は `ms`、`seconds`、`tick` を名前で区別する。曲作者オフセット、譜面オフセット、プレイヤー補正、表示補正は同じ整数として混ぜず、関数名と構造体名で意味を分ける。

## View And Renderer

View/Renderer は状態を読んで描画する層である。raylib 呼び出し、フォント、テクスチャ、色、座標、レイアウト計算はここに置いてよい。ただし、ゲームルールや永続化、通信、Scene 遷移をここで実行しない。

クリック領域やホバー判定は描画と密接なので View に近いが、状態変更までは行わない。View は hit region や command を返し、Controller または Scene がそれを解釈する。

長い描画ファイルは、見た目の部品単位で分割する。分割単位は「関数の長さ」ではなく「状態の意味」で決める。例として、曲リスト、詳細パネル、ランキング、ログインダイアログ、確認ダイアログ、ヘッダーは別 View にできる。

## Services And Infrastructure

Service は Feature から呼ばれるアプリケーション処理であり、Infrastructure は外部世界との境界である。Service はユースケースを表し、Infrastructure は HTTP、SQLite、BASS、ファイルシステム、Windows API などの具体技術を隠す。

ネットワーク、DB、ファイル形式、音声ライブラリの詳細を Scene や Controller に漏らさない。Controller は「カタログを読み込む」「ランキングを更新する」「曲をインポートする」といった意図を Service に依頼し、HTTP メソッドや SQL や JSON フィールド名を直接扱わない。

非同期処理は Controller または Service に閉じ込める。Scene に `future` やロード世代管理が増え始めたら、専用の data controller を作る。非同期結果は、成功、失敗、キャンセル、再試行、世代違いを明示できる結果型で返す。

## Audio And Timing

リズムゲームでは、音声時刻とゲーム時刻を最上位の設計対象として扱う。Play Feature は、音声クロック、譜面時刻、視覚時刻、入力時刻を区別する。音声マネージャはデバイスやストリームの実時刻を提供するが、判定システムはその取得方法を知らない。

入力は「現在押されているか」だけでなく、「いつ押されたか」「いつ離されたか」をイベントとして扱う。判定はフレームレートに依存しないように、入力イベントの timestamp と譜面イベントの target time を比較する。

再生中の一時停止、フォーカス喪失、イントロ、失敗演出、リザルト遷移は Play Session の状態機械として扱う。音声再生や停止は Controller が直接実行せず、結果として要求を返し、Scene が audio layer に適用する。

## Editor

Editor は Gameplay Domain の別利用者である。Play と Editor は譜面、タイミング、スクロール、判定可能なイベント表現を共有する。ただし Editor UI の選択状態、ドラッグ状態、パネル入力状態、保存ダイアログ状態は Editor Feature に閉じ込める。

編集操作は command または service として表す。ノート追加、削除、移動、タイミング変更、スクロールイベント編集、メタデータ変更は、操作前後の状態が明確になるようにする。Undo/Redo を導入しやすい粒度で設計する。

Editor の保存処理は、UI 状態から chart/song モデルを組み立てる段階と、実際にファイルへ書く段階を分ける。保存前の検証、ID 生成、メタデータ正規化も service に寄せる。

## Refactoring Rules

設計改善を行うときは、理想形への大規模な一括移行を狙わない。触っている Feature の中で、次の順に境界を作る。

1. まず状態を `*_state` に集める。
2. 次に状態変更を `*_controller` または `*_service` へ移す。
3. 描画だけを `*_view` または `*_renderer` へ移す。
4. Scene は context を作り、result の副作用を適用するだけに近づける。
5. Gameplay Domain に置けるロジックを raylib や Scene 依存から切り離す。

新しいコードを書くときは、既存の肥大化した Scene に直接足さない。最初から小さな state、controller、view、service のどれに属するかを決める。属する場所が曖昧な処理は、たいてい責務が混ざっている。

## Testing

テストは内側の層ほど厚くする。Gameplay Domain、Controller、Service は smoke test ではなく、入力と出力を確認するテストを書きやすい形に保つ。Renderer と Scene は完全な単体テストより、起動、遷移、代表操作の smoke test でよい。

判定、スコア、タイミング、譜面変換、エディタ操作、インポート/エクスポート、ランキング送信データは回帰しやすいので、純粋関数または小さな状態機械としてテストする。

## Decision Checklist

設計判断に迷ったら、次を確認する。

- その処理はゲームルールか、画面操作か、描画か、外部入出力か。
- その処理は raylib がなくてもテストできるべきか。
- Scene が知らなくてよい詳細を Scene が知っていないか。
- View が状態を変更していないか。
- Service が UI の選択状態を持っていないか。
- 共有モデルに Feature 固有の都合を混ぜていないか。
- 時間単位、ID、オフセットの意味が名前で区別されているか。
- 副作用を直接実行するより、Controller の result として返せないか。

このチェックに多く引っかかる場合は、処理を分ける。少しの冗長さより、境界の一貫性を優先する。
