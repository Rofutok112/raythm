# Content Storage And Song Catalog

`raythm` では、楽曲と譜面を次の 3 系統で扱います。

- 配布物として同梱される `assets/`
- 起動時に `assets/` から再生成される `AppData/Local/raythm/official/`
- ユーザー追加コンテンツを置く `AppData/Local/raythm/songs/` と `AppData/Local/raythm/charts/`

このドキュメントは、どこに何が保存され、song select がどこを参照しているかを整理したものです。

## Path Layout

主なパス定義は [app_paths.h](C:/Users/rento/CLionProjects/raythm/src/core/app_paths.h) と [app_paths.cpp](C:/Users/rento/CLionProjects/raythm/src/core/app_paths.cpp) にあります。

- `assets_root()`
  - 配布版では `executable_dir()/assets`
  - 開発中は repo の `assets/`
- `app_data_root()`
  - `AppData/Local/raythm/`
- `songs_root()`
  - `AppData/Local/raythm/songs/`
- `charts_root()`
  - `AppData/Local/raythm/charts/`
- `official_root()`
  - `AppData/Local/raythm/official/`
- `official_songs_root()`
  - `AppData/Local/raythm/official/songs/`
- `official_charts_root()`
  - `AppData/Local/raythm/official/charts/`

## Official Content Flow

公式楽曲の原本は `assets/` にあります。

- 楽曲メタデータと音声:
  - `assets/songs/{song_id}/song.json`
  - `assets/songs/{song_id}/...audio...`
- 公式譜面:
  - `assets/songs/{song_id}/charts/*.rchart`

起動時に [main.cpp](C:/Users/rento/CLionProjects/raythm/src/main.cpp) から [official_content_sync.cpp](C:/Users/rento/CLionProjects/raythm/src/core/official_content_sync.cpp) の `official_content_sync::synchronize()` を呼びます。

この同期では:

- `AppData/Local/raythm/official/` をいったん削除
- `assets/songs/` を `official/songs/` へコピー
- `assets/charts/` を `official/charts/` へコピー

を行います。

`official/` はキャッシュ扱いです。ユーザーデータではないので、起動時に再生成されます。

## User Content Flow

ユーザーが追加した楽曲と譜面は `AppData` 側に保存されます。

- 楽曲:
  - `AppData/Local/raythm/songs/{song_id}/song.json`
  - `AppData/Local/raythm/songs/{song_id}/...audio...`
  - 必要なら jacket
- 外部譜面:
  - `AppData/Local/raythm/charts/{chart_id}.rchart`

新規作成と import/export まわりは主に [song_import_export_service.cpp](C:/Users/rento/CLionProjects/raythm/src/scenes/song_select/song_import_export_service.cpp) と editor 保存処理から入ります。

現在の方針:

- 譜面拡張子は `.rchart`
- 楽曲パッケージ拡張子は `.rpack`
- imported song は `songs_root()`
- imported chart は `charts_root()`

## Song Catalog Build

song select の一覧は [song_catalog_service.cpp](C:/Users/rento/CLionProjects/raythm/src/scenes/song_select/song_catalog_service.cpp) の `load_catalog()` が組み立てています。

読み込み順は次です。

1. `official_songs_root()` から公式楽曲を読む
2. `songs_root()` からユーザー楽曲を読む
3. `official_charts_root()` の外部譜面を song_id で公式楽曲へ紐付ける
4. `charts_root()` の外部譜面を song_id でユーザー楽曲/公式楽曲へ紐付ける

その後:

- 曲名でソート
- 各楽曲の譜面を level / key_count / difficulty でソート

して song select state に渡します。

## What The Loader Expects

実際の読み込みは [song_loader.cpp](C:/Users/rento/CLionProjects/raythm/src/gameplay/song_loader.cpp) が担当します。

重要な前提は次です。

- 譜面拡張子は `.rchart` のみ
- 楽曲ディレクトリには `song.json` が必要
- `song.json` には `songId`, `title`, `artist`, `audioFile`, `jacketFile`, `baseBpm`, `chorusStartSeconds`, `songVersion` が必要
- 外部譜面は `chart.meta.song_id` が一致した楽曲にだけ紐付きます

## Why Official Songs Can Disappear

公式楽曲は `AppData/Local/raythm/official/` をそのまま読むのではなく、`assets/` と一致している前提で扱っています。

そのため、以前は次のような状態で song list から公式曲が消えることがありました。

- `assets/` は `.rchart` に移行済み
- `AppData/Local/raythm/official/` に古い `.chart` が残っていた
- loader の一致判定で「assets と違う」と判断され、公式曲が読み飛ばされた

現在は official キャッシュを起動時に作り直すので、この不整合は起きにくくなっています。

## Import And Export Summary

- `IMPORT SONG`
  - `.rpack` を展開
  - `song.json` を読む
  - `songs_root()/song_id/` へ配置
- `EXPORT SONG`
  - 楽曲ディレクトリを一時 staging に集める
  - `.rpack` を作る
- `IMPORT CHART`
  - `.rchart` を読む
  - `chart.meta.song_id` と一致する楽曲を自動で探す
  - `charts_root()/chart_id.rchart` に保存
- `EXPORT CHART`
  - 既存譜面を `.rchart` として書き出す

## Files To Check When Something Looks Wrong

- 公式曲が出ない
  - `assets/songs/`
  - `AppData/Local/raythm/official/songs/`
- imported song が出ない
  - `AppData/Local/raythm/songs/`
  - 対象の `song.json`
- imported chart が出ない
  - `AppData/Local/raythm/charts/`
  - `chart.meta.song_id`
- アップデート後に様子がおかしい
  - `AppData/Local/raythm/updater/update.log`
