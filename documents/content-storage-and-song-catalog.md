# Content Storage And Song Catalog

`raythm` では、楽曲・譜面・MV を次のローカル保存領域で扱います。

- 配布物として同梱される `assets/`
- ローカル楽曲を置く `AppData/Local/raythm/songs/`
- ローカル譜面を置く `AppData/Local/raythm/charts/`
- ローカル MV を置く `AppData/Local/raythm/mvs/`

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
- `mvs_root()`
  - `AppData/Local/raythm/mvs/`
- `mv_dir(mv_id)`
  - `AppData/Local/raythm/mvs/{mv_id}/`

## Bundled Asset Flow

同梱アセットの原本は `assets/` にあります。

- 楽曲メタデータと音声:
  - `assets/songs/{song_id}/song.json`
  - `assets/songs/{song_id}/...audio...`
- 同梱譜面:
  - `assets/songs/{song_id}/charts/*.rchart`

これらは開発中や配布物として直接参照する同梱データであり、`AppData/Local/raythm/official/` のような mirror キャッシュは現在は使いません。

## User Content Flow

ユーザーが追加した楽曲・譜面・MV は `AppData` 側に保存されます。

- 楽曲:
  - `AppData/Local/raythm/songs/{song_id}/song.json`
  - `AppData/Local/raythm/songs/{song_id}/...audio...`
  - 必要なら jacket
- 外部譜面:
  - `AppData/Local/raythm/charts/{chart_id}.rchart`
- MV:
  - `AppData/Local/raythm/mvs/{mv_id}/mv.json`
  - `AppData/Local/raythm/mvs/{mv_id}/script.rmv`

新規作成と import/export まわりは主に [song_import_export_service.cpp](C:/Users/rento/CLionProjects/raythm/src/scenes/song_select/song_import_export_service.cpp) と editor 保存処理から入ります。

現在の方針:

- 譜面拡張子は `.rchart`
- 楽曲パッケージ拡張子は `.rpack`
- imported song は `songs_root()`
- imported chart は `charts_root()`
- MV は `mvs_root()`

## Song Catalog Build

song select の一覧は [song_catalog_service.cpp](C:/Users/rento/CLionProjects/raythm/src/scenes/song_select/song_catalog_service.cpp) の `load_catalog()` が組み立てています。

読み込み順は次です。

1. `songs_root()` からローカル楽曲を読む
2. `charts_root()` の外部譜面を `song_id` でローカル楽曲へ紐付ける

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

## MV Package Layout

MV は曲から独立したパッケージとして保存されます。

- `mv.json`
  - `mvId`
  - `songId`
  - `name`
  - `author`
  - `scriptFile`
- `script.rmv`
  - MV script 本体

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

- imported song が出ない
  - `AppData/Local/raythm/songs/`
  - 対象の `song.json`
- imported chart が出ない
  - `AppData/Local/raythm/charts/`
  - `chart.meta.song_id`
- MV が出ない / 読まれない
  - `AppData/Local/raythm/mvs/`
  - 対象の `mv.json`
  - 対象の `script.rmv`
- アップデート後に様子がおかしい
  - `AppData/Local/raythm/updater/update.log`
