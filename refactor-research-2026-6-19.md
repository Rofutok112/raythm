# Refactor Research 2026-06-19

## 調査範囲と基準

このメモは raythm 全体を、Scene / Feature State / Controller / View / Service / Infrastructure / Gameplay Domain の境界で見直した調査結果である。完了要件として、すべての Scene と、それにまつわる Service、Infrastructure、非同期処理を確認した。

評価基準は `.codex/skills/raythm-architecture/SKILL.md` と `documents/ArchitectureBoundaries.md` に合わせる。理想は、Scene が lifecycle、入力収集、Controller 呼び出し、Renderer 呼び出し、Scene 遷移や音声などの副作用適用だけを行うこと。非同期は Scene ではなく Controller / Service に閉じ込め、Infrastructure の詳細は HTTP / SQLite / BASS / Win32 / filesystem などの境界へ寄せる。

`rg -n "class .*: public scene|class .* final : public scene" src -S` で確認した本番 Scene 派生は次の7つである。テスト用の `counting_scene` は除外した。

- `title_scene`
- `play_scene`
- `editor_scene`
- `song_create_scene`
- `mv_editor_scene`
- `result_scene`
- `multiplayer_result_scene`

Song Select、Settings、Multiplayer room list、Online Catalog は独立した `scene` 派生ではなく、TitleScene 配下または共有 overlay / feature として組み込まれている。そのため、このメモでは TitleScene の周辺 Feature として扱う。ただし責務量が大きいため、Song Select、Settings、Multiplayer、Online Catalog は個別に所見を記録している。

## 全体サマリ

現在の raythm は、Title / Song Select / Editor / Play ではすでに分割が進んでいる。特に Title Hub の friends / public profile、Song Select の data controller、Editor の note/timing services、Play の flow controller は、理想形に近づくための足場になっている。

一方で、次の領域はまだ境界が太い。

- `src/scenes/title/online_download_*.cpp`: Online catalog / download / remote fetch / local storage / UI state update / future 起動が密集している。
- `src/scenes/play_scene.*`: Play 本体は分割済みだが、multiplayer 同期と async future が Scene に残る。
- `src/scenes/result_scene.*` と `src/scenes/multiplayer_result_scene.*`: Result 系は Scene が永続化、通信、非同期、描画、入力を多く持つ。
- `src/scenes/song_select/song_select_state.*` と `src/scenes/title/online_download_view.h`: state が `Texture2D`、`std::future`、Service DTO、UI 型を含む。
- `src/services/ranking_service.cpp`: use case service と Infrastructure が一体化している。
- `src/gameplay/input_handler.*`: Gameplay 配下に raylib / Windows input adapter が混ざっている。
- `src/audio/audio_manager.*`: BASS device manager、preview async load、loudness cache、SE cache が一体化している。

## Scene 一覧と所見

### TitleScene

対象: `src/scenes/title_scene.*`, `src/scenes/title/*`, `src/scenes/song_select/*`, `src/scenes/shared/*`, `src/scenes/multiplayer/*`

現状:

- `title_scene` は `title_play_create_feature`、`title_browse_feature`、friends、profile、public profile、multiplayer、auth overlay、audio、settings overlay を所有する hub。
- `title_common_update_controller`、`title_mode_update_controller`、`title_command_dispatcher`、`title_shell_effects` などがあり、Scene はかなり薄くなっている。
- Friends は `title_friends_state` / `title_friends_reducer` / `title_friends_controller` / `title_friends_service` / `title_friends_view` に分かれており、今回範囲でかなり良い。
- Public profile も `public_profile_state` / controller / service / view / effects に分かれている。
- Song Select は `song_select::state`、`song_select::data_controller`、`song_select_ranking_loader`、`song_transfer_controller`、`song_catalog_service` があり、Title hub と standalone 的な利用を共有する構造になっている。

問題:

- `src/scenes/title/online_download_catalog.cpp`, `online_download_transfer.cpp`, `online_download_remote_client.cpp`, `online_download_update.cpp`, `online_download_render.cpp` が大きい。名前は View 寄りだが、実際は catalog service、download service、remote client、state mutation、future 起動、render が分散しつつ密結合している。
- `src/scenes/title/online_download_view.h` の `title_online_view::state` が ranking listing、download progress、future-backed jacket cache、remote payload、UI state をまとめて持っている。
- `src/scenes/title/online_download_remote_client.cpp` は WinHTTP / JSON / API DTO の Infrastructure だが `src/scenes/title` 配下にある。
- `src/scenes/title/title_profile_controller.cpp` は controller が auth API、profile upload/delete、avatar save、ranking fetch、file dialog まで持つ。
- `src/scenes/shared/auth_overlay_controller.cpp` は auth use case と `std::thread` 起動を controller 内で持つ。
- `src/scenes/multiplayer/multiplayer_state.h` は `std::future` と `friend_client` DTO を state が所有している。

優先リファクタ:

1. `online_catalog_service` / `online_download_service` / `online_content_gateway` を作り、`online_download_catalog.cpp` と `online_download_transfer.cpp` の通信・保存・future 起動を Service / Infrastructure へ寄せる。
2. `online_download_remote_client.*` を network / infrastructure 側に移し、Title 側は use case API だけを見る。
3. `song_select_state` と `title_online_view::state` から `Texture2D` / `std::future` / Service DTO を追い出し、loader/cache/controller に寄せる。
4. `title_profile_service` を作り、profile controller は input と UI state の更新に集中させる。
5. `multiplayer_service` または task controller を作り、`multiplayer_state` から future 所有を外す。

### PlayScene

対象: `src/scenes/play_scene.*`, `src/scenes/play/*`, `src/gameplay/*`, `src/audio/*`

現状:

- `play_flow_controller` は frame context を受け、判定、スコア、ゲージ、BGM 要求、遷移要求を result で返す。副作用分離の足場として良い。
- `play_session_state` は score system、performance system、gauge、input handler、timing engine、judge system、scroll map をまとめて持つ。
- `play_renderer`, `play_note_draw_queue`, `play_mv_controller`, `play_hitsound_service`, `play_session_loader` がある。

問題:

- `play_scene` が `load_future_`、`multiplayer_loaded_future_`、`multiplayer_score_future_`、`multiplayer_room_future_` を持つ。非同期の管理が Scene に漏れている。
- `start_async_load()` と `load_future_` は名前上 async だが、現状は `play_session_loader::load()` を同期実行して `apply_loaded_session()` している。設計の名残が残っている。
- `update_start_gate()` と `sync_multiplayer_score()` が Scene 内で `std::async`、WebSocket command、JSON 手組み、HTTP fallback、DTO 変換を行う。
- `play_session_loader` は譜面ロード、音声ロード、managed content、ranking preparation、waveform、SE preload までまとめて持ち、Service + Infrastructure + Domain assembly になっている。
- `gameplay/input_handler.*` が raylib と `windows_input_source` を参照し、Gameplay Domain と Platform adapter が混ざっている。

優先リファクタ:

1. `play_multiplayer_sync_controller` / `play_multiplayer_service` を作り、start gate、match loaded、score sync、room fetch を Scene から移す。
2. Play load の async 方針を決める。同期ロードなら `load_future_` を消す。非同期ロードなら `play_session_load_controller` に `start/poll/cancel/progress` を閉じ込める。
3. `play_session_loader` を session assembly、chart loading、audio loading、ranking preparation に分ける。
4. `input_handler` を Gameplay の入力イベント変換と platform/raylib polling adapter に分け、Domain から `KeyboardKey` と Windows singleton を外す。

### ResultScene

対象: `src/scenes/result_scene.*`, `src/scenes/result/*`, `src/services/ranking_service.*`

現状:

- 描画は `result_scene_view` に切り出されている。
- Scene は local ranking submit、online submit、result BGM、jacket texture、input、retry / song select 遷移を持つ。

問題:

- online submit は `std::thread(...).detach()`、`atomic`、`mutex`、shared task state で Scene が直接管理する。キャンセル、世代、join の統一がない。
- `result_scene_view::model` が `ranking_service::local_submit_result` と `ranking_service::online_submit_result` に直接依存している。
- Result の永続化・送信 use case が Result Scene と `ranking_service` の巨大関数群に挟まっている。

優先リファクタ:

1. `result_submission_service` または `result_controller` を作り、local submit と online submit の `start/poll/result` を Scene から分離する。
2. detached thread を全体の async job パターンへ統一する。
3. `result_scene_view` へ渡す View model を作り、Service DTO 依存を外す。

### MultiplayerResultScene

対象: `src/scenes/multiplayer_result_scene.*`

現状:

- Dedicated な `multiplayer_result_state` / controller / view / service はまだない。
- Scene が result 表示、score list、scrollbar、room refresh、complete match、jacket texture、BGM、input、draw を全部持つ。

問題:

- `room_future_` と `complete_future_` が Scene にあり、`std::async` で room refresh / complete match を直接起動する。
- ResultScene と Multiplayer feature の重複が多い。順位計算、score row update、result 表示、room fetch が独自実装。
- `draw()` が大きく、UI 部品の意味単位で分かれていない。

優先リファクタ:

1. `multiplayer_result_state` / `multiplayer_result_controller` / `multiplayer_result_view` / `multiplayer_result_service` を作る。
2. 最初の一手は `draw()` の View 化と `poll_room_refresh()` / `request_return_to_room()` の service 化。
3. ResultScene と共通化できる score/result presentation model を検討する。

### EditorScene

対象: `src/scenes/editor_scene.*`, `src/scenes/editor/*`

現状:

- `editor_session_loader` で起動時 state を組み立てる。
- `editor_state` が譜面データ、Undo/Redo、ノート編集、timing/scroll 編集、dirty、level refresh generation を持つ。
- `editor_note_edit_service`, `editor_timing_edit_service`, `editor_metadata_service`, `editor_transport_service` などがある。
- `editor_runtime_controller`, `editor_screen_controller`, `editor_timeline_screen_controller`, `editor_timing_action_controller` が入力と画面操作を分担する。
- 非同期の直接利用はこの範囲では見つからず、主な問題は責務混在。

問題:

- `editor_flow_controller.cpp` が `chart_serializer`、`managed_content_storage`、filesystem、SHA256、保存先判定を直接実行している。Controller が Infrastructure を知りすぎている。
- `editor_scene::open_unlock_rules_from_metadata()` が auth、server URL 正規化、local catalog 読み込み、remote chart ID 探索を Scene 内で行う。
- `editor_transport_service` は Service 名だが `audio_manager::instance()` を直接呼ぶ。理想は audio request を返して Scene が適用する形。
- `editor_daw_view.cpp` が巨大。timeline body、automation lane、minimap、side panel、modal などで意味単位分割すると安全。

優先リファクタ:

1. `editor_chart_save_service` を作り、保存処理、serializer、managed storage、hash、filesystem を Controller から外す。
2. `editor_unlock_rules_service` を作り、remote chart 解決を Scene から外す。
3. `editor_transport_service` を audio request を返す controller/service に寄せる。
4. `editor_daw_view.cpp` を状態意味単位で分割する。

### SongCreateScene

対象: `src/scenes/song_create_scene.*`, `src/scenes/song_create/*`

現状:

- `song_create_service`、`song_create_timing_service`、`song_create_timing_controller`、`song_create_midi_importer`、`song_create_form_panel`、`song_create_tag_editor`、`song_create_timing_panel`、`song_create_saved_view` がある。
- 以前より分割は進んでいるが、Scene private 変数が画面状態そのものになっている。
- 非同期の直接利用は見つからず、主な問題は Scene / View / Infrastructure の混在。

問題:

- `song_create_scene` が入力 state 群、file dialog、audio preview、metronome、保存、Scene 遷移を広く持つ。
- `start_timing_preview()` と `update_metronome()` が file existence、`audio_manager`、SE path、timing engine 生成を Scene 内で行う。
- `draw_song_metadata()` が draw 中 callback で file dialog、保存、Scene 遷移を起動できる。
- `song_create_timing_panel.cpp` が `file_dialog::open_midi_file()` を直接呼ぶ。View から Platform Infrastructure が漏れている。
- `song_create_service` は use case だが、file copy、directory create、`song.json` write まで直接扱う。

優先リファクタ:

1. `song_create_state` を作り、Scene private の入力群と timing/modal state を集約する。
2. `song_create_timing_preview_controller` を作り、preview/metronome/audio request を Scene から外す。
3. Timing panel は `request_import_midi` command を返すだけにし、file dialog と importer 呼び出しは Controller / Scene 側で行う。
4. `song_create_service` を use case と filesystem writer に分ける。

### MVEditorScene

対象: `src/scenes/mv_editor_scene.*`, `src/mv/*`

現状:

- `mv_editor_scene` は小さいが、MV package load、text editor UI、compile、save、metadata modal を直接統合している。
- `src/mv/lang` は lexer/parser/compiler/vm/sandbox、`src/mv/api` は context/builtins、`src/mv/render` は renderer/validator、`mv_storage` は filesystem 永続化。

問題:

- Scene が sandbox 作成、builtins 登録、compile、error marker 反映を持つ。
- `mv_storage` が `src/mv` 直下にあり、Domain / Runtime / Infrastructure の物理境界が曖昧。

優先リファクタ:

1. `mv_editor_controller` と `mv_editor_service` を作り、compile/save/load を Scene から分離する。
2. `mv_storage` を Infrastructure として明示し、lang/runtime/render の純粋処理と filesystem を分ける。

### Settings Overlay / Settings Modules

対象: `src/scenes/settings/*`, `title_settings_overlay.*`, `title_settings_flow_controller.*`

現状:

- Settings は独立 Scene ではなく Title / Editor から overlay として使われる。
- `settings_page_stack`, `settings_pages`, `settings_key_config_state`, `settings_gameplay_preview`, `settings_runtime_applier`, `settings_shell_view` に分かれている。
- `settings_runtime_applier` が runtime side effects をまとめる役割を持つ。

問題:

- 直接の大きな非同期問題は見当たらない。
- `settings_gameplay_preview` は render texture と play renderer 的な表示を持つため、Play 側の renderer / geometry 変更と一緒に壊れやすい。
- Settings の永続化は `gameplay/settings_io.*` にあり、`gameplay` 配下に filesystem adapter がある点は全体境界としては要注意。

優先リファクタ:

1. `settings_io` を Gameplay から infrastructure/settings storage へ寄せる。
2. Preview は Play geometry / renderer と依存関係を明示し、共有できる model を薄く保つ。

## Service / Infrastructure 所見

### Ranking

対象: `src/services/ranking_service.*`, `src/network/ranking_client.*`

問題:

- `ranking_service.cpp` は local ranking repository、online ranking gateway、ruleset cache、manifest verification、auth/session restore、DPAPI/file I/O、network client adapter が一体化している。
- Result / Song Select / Title / Play から広く呼ばれるため、巨大化の影響範囲が大きい。

優先:

- `local_ranking_repository`
- `ranking_ruleset_cache`
- `ranking_submission_service`
- `ranking_manifest_verifier`
- `ranking_client_adapter`

のように段階的に分ける。最初は online submit と local file/DPAPI を切るのが効果大。

### Network

対象: `src/network/http_client.*`, `auth_client.*`, `friend_client.*`, `ranking_client.*`, `unlock_rule_client.*`, `websocket_client.*`

現状:

- `http_client` は WinHTTP 境界。
- 各 client は API DTO、JSON parse、auth refresh、session restore を持つ。
- `websocket_client` は receive thread を内包し、poll 型 API を提供する。

問題:

- `auth_client` が HTTP client と session/device file storage を密結合している。
- Feature 配下にも `src/scenes/title/online_download_remote_client.*`, `create_upload_client.*`, `src/scenes/multiplayer/multiplayer_client.*` のような client があり、network 境界の物理配置が散っている。

優先:

1. `auth_session_store` を分離し、auth client は HTTP/API DTO に寄せる。
2. Feature 配下 client のうち WinHTTP/JSON/API DTO 変換を network/infrastructure 側へ移す。

### Local Catalog / SQLite

対象: `src/core/local_sqlite.*`, `src/scenes/song_select/local_catalog_database.*`, `src/scenes/title/local_content_database.*`

現状:

- `core/local_sqlite` は共通 RAII と metadata schema。
- song_select / title 配下に SQLite database 実装がある。

問題:

- SQLite Infrastructure が Feature 配下にある。Feature 用 query result / mapper は残してよいが、DB 技術境界としては硬い。

優先:

- `infrastructure/sqlite` または `services/catalog_repository` へ寄せる。
- Feature 側には query request と mapper だけ残す。

### Audio

対象: `src/audio/audio_manager.*`, `audio_loudness.*`, `audio_waveform.*`

現状:

- `audio_manager` が BASS initialization、BGM/preview/SE、FFT、clock、preview async load、loudness cache、SE sample cache を持つ。
- owner thread guard があり、BASS 操作を main/owner thread に寄せる意図がある。

問題:

- preview async load と stale future 管理、loudness cache、BASS stream 操作が同じ manager にある。
- Feature services から `audio_manager::instance()` へ直接触る箇所が多く、audio request の境界が一定ではない。

優先:

1. `audio_preview_load_service` / `audio_loudness_cache` を分ける。
2. Feature controller/service は `play_audio_request`, `editor_audio_request`, `song_create_audio_request` のような結果を返し、Scene が audio manager に適用する。

### Managed Content / Content Availability

対象: `src/services/managed_content_storage.*`, `content_authorization_service.*`, `online_content_availability.*`, `content_sync_service.*`

現状:

- `managed_content_storage` は manifest、encrypted asset、content cache filesystem をまとめる。
- `content_sync_service` と `content_authorization_service` は比較的純粋。

問題:

- `online_content_availability` が `title/local_content_index` と `song_select::song_entry` に依存し、Feature 間共有 Service として硬い。
- managed storage は Infrastructure だが多くの Feature Service から直接触られる。

優先:

- availability 判定用の中立 DTO を作り、Title/SongSelect 型への依存を薄める。
- managed storage は repository/gateway として扱い、Feature Service で直接細部を触る量を減らす。

### Updater / Platform

対象: `src/updater/*`, `src/platform/*`, `src/core/window_dialog_support.*`, `src/core/file_dialog.*`

現状:

- updater は WinHTTP、process、extract、apply、workflow がある。
- platform は window chrome と Windows input source が singleton / mutex で外部境界を持つ。

問題:

- updater は app 本体からやや独立しているが、`update_release` の WinHTTP/JSON と `update_workflow` の use case 境界はさらに分けられる。
- `file_dialog` を View が直接呼ぶ箇所があり、Platform Infrastructure が View に漏れる。

優先:

1. File dialog は View command から Scene/Controller で呼ぶルールに統一する。
2. updater は必要になったタイミングで release client と workflow を分ける。

## 非同期処理の地図

現在の非同期は、次の複数パターンが混在している。

- `std::async(std::launch::async)`: Play multiplayer、Multiplayer controller、MultiplayerResult、create unlock rules、title permission refresh、avatar cache など。
- `std::promise + std::thread(...).detach()`: auth overlay、public profile service、title friends service、title profile controller、song_select data/ranking/transfer/jacket/audio、online catalog/download、result submit など。
- `std::future` を state が保持: `song_select_state`, `title_online_view::state`, `multiplayer_state`, `play_scene`, `multiplayer_result_scene` など。
- Infrastructure 内部 thread: `network/websocket_client`, `audio_manager` preview load future, `windows_input_source` mutex-protected state。
- Global cache + mutex: `chart_level_memory_cache`, `scoring_ruleset_runtime`, `ranking_service` cache, `load_progress`。

問題:

- `future` が Scene / state / controller / service に散らばっており、キャンセル、世代違い、queued reload、失敗、retry の表現が統一されていない。
- detached thread は join/cancel しないため、Scene 破棄や app exit 時の扱いが個別設計になっている。
- async result に stale / generation を持つものと持たないものがある。

提案:

1. 小さな `async_job<T>` または feature-local task wrapper を導入し、`start`, `poll`, `cancel_generation`, `is_running`, `result` を統一する。
2. Scene は `future` を直接持たず、`*_data_controller` / `*_service` が async handle を持つ。
3. すべての非同期結果は `completed`, `success`, `stale`, `message`, `retry_after` のような最小共通語彙を持つ。
4. detached thread を新規追加しない。既存 detached thread は触る範囲から `promise + detached` を wrapper へ包む。

## 優先順位

### P0: 境界効果が大きく、Scene/async の漏れが強いもの

1. Online catalog/download 分割
   - 対象: `src/scenes/title/online_download_catalog.cpp`, `online_download_transfer.cpp`, `online_download_remote_client.cpp`, `online_download_view.h`
   - ゴール: catalog service、download service、remote gateway、view state を分離。

2. Play multiplayer sync 分離
   - 対象: `src/scenes/play_scene.cpp`, `src/scenes/play_scene.h`
   - ゴール: `update_start_gate()` / `sync_multiplayer_score()` / match loaded / score upload / room fetch を service/controller 化。

3. Result submit 分離
   - 対象: `src/scenes/result_scene.*`, `src/services/ranking_service.*`
   - ゴール: detached thread を消し、result submission service に統一。

4. MultiplayerResult 分割
   - 対象: `src/scenes/multiplayer_result_scene.*`
   - ゴール: state/controller/view/service を作る。

### P1: Shared state / Service / Infrastructure の重い混在

1. `ranking_service.cpp` 分割。
2. `song_select_state` から texture/future/service DTO を退避。
3. `multiplayer_state` から future/client DTO を退避。
4. `editor_chart_save_service` と `editor_unlock_rules_service` の追加。
5. `song_create_state` と timing preview controller の追加。
6. `gameplay/input_handler` の adapter 分離。

### P2: UI/Renderer と補助 Infrastructure の整理

1. `editor_daw_view.cpp` 分割。
2. `result_scene_view` の Service DTO 依存削除。
3. `avatar_texture_cache` を image fetch と texture draw helper に分ける。
4. `auth_overlay_controller` から auth async service を分離。
5. `mv_editor_controller` / `mv_editor_service` を追加。
6. `settings_io` と `mv_storage` の Infrastructure 明示。

## 次の進め方

安全な進め方は、大規模移動ではなく、各 Feature の既存入口を保ったまま service/controller を挿入すること。

最初の実装候補は Result submit 分離がよい。範囲が狭く、detached thread という明確な非同期問題があり、`result_scene_view` の DTO 分離まで小さく完結できる。

次に Play multiplayer sync を切ると、PlayScene の async 漏れが大きく減る。そのあと Online catalog/download に入るのがよい。Online は影響範囲が広いため、先に async wrapper と service result の語彙を揃えてから触ると安全。
