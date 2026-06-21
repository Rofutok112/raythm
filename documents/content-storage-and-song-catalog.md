# Content Storage And Song Catalog

`raythm` では、楽曲・譜面・MV を次のローカル保存領域で扱います。

- 配布物として同梱される `assets/`
- ローカル楽曲を置く `AppData/Local/raythm/songs/`
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
- 譜面:
  - `AppData/Local/raythm/songs/{song_id}/charts/{chart_id}.rchart`
- MV:
  - `AppData/Local/raythm/mvs/{mv_id}/mv.json`
  - `AppData/Local/raythm/mvs/{mv_id}/composition.rmvcomp`
  - `AppData/Local/raythm/mvs/{mv_id}/assets/`

新規作成と import/export まわりは主に [song_import_export_service.cpp](C:/Users/rento/CLionProjects/raythm/src/scenes/song_select/song_import_export_service.cpp) と editor 保存処理から入ります。

現在の方針:

- 譜面拡張子は `.rchart`
- 楽曲パッケージ拡張子は `.rpack`
- imported song は `songs_root()`
- imported chart は対象楽曲の `songs_root()/{song_id}/charts/`
- MV は `mvs_root()`

## Song Catalog Build

song select の一覧は [song_catalog_service.cpp](C:/Users/rento/CLionProjects/raythm/src/scenes/song_select/song_catalog_service.cpp) の `load_catalog()` が組み立てています。

読み込み順は次です。

1. `songs_root()` からローカル楽曲を読む
2. 各楽曲ディレクトリの `charts/*.rchart` を読み、親楽曲IDを実行時メタに反映する
3. ローカルカタログDBへ `local_charts.song_id` として保存する

その後:

- 曲名でソート
- 各楽曲の譜面を calculated level / key_count / difficulty でソート

して song select state に渡します。

ローカルカタログDBは `song.json` 側の制作者設定 `offset` と `timingEvents`
もキャッシュします。BPM範囲や難易度計算は、楽曲側タイミングがある場合は
それを優先し、旧 `.rchart` 内タイミングは後方互換のフォールバックとして扱います。

## What The Loader Expects

実際の読み込みは [song_loader.cpp](C:/Users/rento/CLionProjects/raythm/src/gameplay/song_loader.cpp) が担当します。

重要な前提は次です。

- 譜面拡張子は `.rchart` のみ
- 楽曲ディレクトリには `song.json` が必要
- `song.json` には `songId`, `title`, `artist`, `audioFile`, `jacketFile`, `baseBpm`, `previewStartMs`, `songVersion` が必要
- `song.json` には任意で `offset`, `timingEvents` を持てる。楽曲タイミングは480 PPQ固定で、旧 `timingResolution` は参照しない
- `.rchart` には `chartId`, `keyCount`, `difficulty`, `chartAuthor`, `formatVersion`, `resolution`, `offset` が必要
- `.rchart` の `resolution`, `offset`, `[Timing]` は旧形式互換として残り、楽曲側の `offset` / `timingEvents` がない場合に使われる
- 譜面と曲の紐づけは `songs/<songId>/charts/*.rchart` の配置とローカルカタログDBで決まり、`.rchart` 内の `songId` は使いません

## MV Package Layout

MV は曲に付随するパッケージとして保存されます。`songId` が所有関係を表し、MV package の identity、Browse 公開、解禁状態、暗号化状態は曲単位で決まります。譜面情報は MV の保存形式や選択条件に入りません。

- `mv.json`
  - `mvId`
  - `songId`
  - `name`
  - `author`
  - `compositionFile`
  - `formatVersion`
- `composition.rmvcomp`
  - MV Composition authoring data
  - JSON-based UTF-8 text
  - Layer / Source / Transform / Effect / Keyframe / EventTrigger / Asset references
  - `EventTrigger.timeMs` uses song timeline milliseconds for MV-local cues; it is not tied to chart selection
  - `.rmvcomp` export is composition-only and does not carry image/generated assets
- `.rmvpack`
  - zip-compatible portable MV package
  - contains `mv.json`, `composition.rmvcomp`, and `assets/`
  - use this for user-facing import/export, Browse upload payloads, and any MV that must move with assets
- Package paths
  - `compositionFile` and composition asset paths must be package-relative
  - absolute paths, Windows drive-relative paths, and `..` traversal are invalid
- `assets/`
  - package-local images and generated assets used by the composition
  - imported images are copied under `assets/images/`
  - composition files store package-relative asset paths only; absolute source paths are invalid
  - image layers must reference an existing composition asset id; dangling image asset references are invalid

Community / Official MV should follow the same managed-content pattern as downloaded content:

```text
content-cache/{community|official}/mvs/{managed_mv_id}/
  managed-package.json
  .encrypted/
    assets/
      *.renc
```

The logical MV files remain:

- `mv.json`
- `composition.rmvcomp`
- `assets/...`

Managed MV editing is gated before writes. A managed MV is editable only when its license is not revoked, at least one known license window is still valid, and unlock metadata allows play/edit. When editing is denied, the editor entry point should show the manifest reason instead of converting the MV into a plain local package. When editing is allowed, `mv.json`, `composition.rmvcomp`, and asset files stay logical managed assets backed by encrypted files; editor saves must not create plain logical files in the content cache.

But in managed content they may be stored as encrypted assets described by `managed-package.json`.

Local editing should stay readable and unencrypted. Community / Official downloaded MVs should stay managed and encrypted when edited: the editor loads the logical composition, saves back through managed storage, updates local hashes/fingerprints, and leaves remote hashes unchanged so the catalog can report the MV as modified.

Implementation notes:

- Managed MV paths are defined by [content_cache_paths.h](C:/Users/rento/CLionProjects/raythm/src/core/content_cache_paths.h) as `mv_cache_key`, `mv_dir`, and `mv_managed_package_manifest_path`.
- Managed MV manifests and logical encrypted composition IO live in [mv_managed_storage.h](C:/Users/rento/CLionProjects/raythm/src/mv/mv_managed_storage.h).
- The MV managed storage layer reuses [managed_content_storage.h](C:/Users/rento/CLionProjects/raythm/src/services/managed_content_storage.h) for encrypted asset bytes instead of inventing a second encryption primitive.
- [mv_storage.h](C:/Users/rento/CLionProjects/raythm/src/mv/mv_storage.h) exposes both local and managed MV packages through the same `mv_package`, `load_composition`, and `save_composition` API.
- Image assets imported into a managed MV are written as encrypted logical files and recorded in `managed-package.json` `assets.files`; local packages still keep plain `assets/images/*` files for easy editing.
- Rendering code reads MV assets through `read_asset_bytes`: local packages read the package file, while managed packages decrypt the asset bytes without exposing a plain logical file path.
- MV composition export always goes through `load_composition`; local packages read the plain file and managed packages decrypt the logical composition before writing a user-selected `.rmvcomp` export.
- `mvHash` / `mvFingerprint` are local editable-state values. `remoteMvHash` / `remoteMvFingerprint` are preserved across local edits so modified detection can compare local vs remote state.
- `mv_managed_storage_smoke` and `content_cache_paths_smoke` cover the current managed MV path and manifest foundation.

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
  - 選択中の楽曲ディレクトリ配下へ保存
  - `songs_root()/song_id/charts/chart_id.rchart` に保存
- `EXPORT CHART`
  - 既存譜面を `.rchart` として書き出す

## Files To Check When Something Looks Wrong

- imported song が出ない
  - `AppData/Local/raythm/songs/`
  - 対象の `song.json`
- imported chart が出ない
  - `AppData/Local/raythm/songs/<songId>/charts/`
  - ローカルカタログDBの `local_charts.song_id`
- MV が出ない / 読まれない
  - `AppData/Local/raythm/mvs/`
  - 対象の `mv.json`
  - 対象の `composition.rmvcomp`
  - 対象の `assets/`
- アップデート後に様子がおかしい
  - `AppData/Local/raythm/updater/update.log`
