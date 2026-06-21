# MV Production Editor Plan

Issue: https://github.com/Rofutok112/raythm/issues/312

## Goal

raythm の MV 機能を、既存の `script.rmv` と `draw(ctx)` による直接描画方式から、GUI-first の内蔵モーショングラフィックス制作環境へ作り直す。

これは単なる MV runtime refactor ではない。ユーザーが raythm 内で曲付随の MV を作り、プレビューし、曲時間や曲イベントに同期させ、保存できる機能にする。

目指す完成形は、軽量な After Effects 的ワークフローである。Premiere 的な動画切り貼りを主目的にはしない。ゲーム内でリアルタイム描画されるレイヤー、図形、文字、画像、生成ビジュアル、キーフレーム、標準エフェクト、イベントトリガーを組み合わせて、曲に同期した MV 演出を作る。

この計画では、既存 MV script 互換は維持しない。ゲームは未リリースで既存 MV 資産も少ないため、後方互換よりも GUI editor、song visual events 連携、runtime performance、素材管理、プリセット、保守性を優先する。

## Product Standard

Issue #312 の完了条件は「新しいデータモデルがある」ではなく、「基本的な MV を GUI で制作できる」である。

ユーザーは最終的に次の操作をできるべきである。

- レイヤーを追加、削除、選択、並べ替え、表示/非表示、ロック、リネームする。
- 背景、図形、線、文字、画像、ジャケット、波形、スペクトラム、ビートグリッドなどの Source を配置する。
- 画像素材を MV package の `assets/` に取り込み、レイヤーの Source として使う。
- タイムライン上でレイヤーの開始時刻、終了時刻、表示区間を編集する。
- Preview 上で現在時刻の MV を確認し、選択レイヤーの境界とアンカーを見ながら調整する。
- Inspector で位置、スケール、回転、アンカー、不透明度、色、テキスト、Source 固有プロパティを編集する。
- キーフレームで位置、スケール、回転、不透明度、色、Source/effect parameter を動かす。
- フェード、パルス、フラッシュ、シェイク、グロー、色変化、ビート同期、音量反応などの標準エフェクトを GUI から追加する。
- 曲イベント / MVイベントから `section.chorus`, `effect.flash`, `text.lyric`, `custom.drop` のようなイベントを受け、レイヤーやエフェクトを発火させる。
- 保存した MV を play scene で同じ runtime path により再生する。

最初から得意にしないもの:

- 複数の実写動画ファイルを切り貼りするノンリニア動画編集。
- 高度な 3D カメラワーク。
- 複雑な手描きアニメーション制作。
- パーティクル大量生成や流体表現。
- 旧 `.rmv` スクリプト互換。

## Decisions

- `script.rmv` / `draw(ctx)` / `DrawRect` 系 API は廃止する。
- 新しい MV 本体は `composition.rmvcomp` とする。
- `mv.json` は MV metadata と `compositionFile` を持つ package manifest に寄せる。
- 素材込みの持ち運び形式は `.rmvpack` とし、`mv.json`, `composition.rmvcomp`, `assets/` を zip-compatible archive としてまとめる。
- MV は song-scoped content とする。`songId` は必須で、MV の所有、保存、公開、解禁、選択は曲だけで決まり、譜面単位の状態は参照しない。
- MV package は `assets/` を第一級に持つ。画像や将来の素材は package 内で参照解決する。
- ユーザー向けの第一級概念は Object / Component ではなく Composition / Layer / Source / Transform / Effect / Keyframe / EventTrigger / Asset とする。
- Authoring Model と Runtime Model は分ける。
- Authoring Model は GUI と保存形式を優先し、Runtime Model は高速評価と安定再生を優先する。
- Runtime では layer / effect / event / property / asset 参照を ID 化し、毎フレームの文字列探索を避ける。
- ScriptBehaviour は最初に作らない。必要になった場合も直接描画 API ではなく、Composition を操作する optional hook として再検討する。
- Public sharing はこの Issue の必須範囲外。ただし package layout、asset references、compatibility metadata は将来共有できる形にする。

## Target Model

```text
MVComposition
  Metadata
  Settings
  AssetLibrary
  Layer[]
    Source
      Background
      Shape
      Text
      Image
      Generated
    Transform
    Effect[]
    KeyframeTrack[]
    EventTrigger[]
    ScriptBehaviour?  // future, optional
```

Core authoring types:

- `mv_composition`
- `mv_metadata`
- `mv_settings`
- `mv_asset`
- `mv_layer`
- `mv_source`
- `mv_transform`
- `mv_effect`
- `mv_keyframe_track`
- `mv_event_trigger`

Initial source types:

- background
- rect
- circle
- line
- text
- image
- jacket image
- spectrum
- waveform
- beat grid

Initial transform fields:

- position
- scale
- rotation
- anchor
- opacity

Initial keyframe targets:

- transform position
- transform scale
- transform rotation
- transform anchor
- opacity
- fill color
- stroke color
- text content
- effect parameter

Initial effect types:

- fade
- pulse
- flash
- shake
- glow
- color shift
- beat pulse
- audio level reactive

Initial event trigger actions:

- set property
- animate property
- trigger effect
- show/hide layer
- set text
- seek local effect time

## Package Layout

```text
mvs/{mv_id}/
  mv.json
  composition.rmvcomp
  assets/
    images/
    generated/
```

`mv.json` example:

```json
{
  "mvId": "uuid",
  "songId": "song-id",
  "name": "New MV",
  "author": "",
  "compositionFile": "composition.rmvcomp",
  "formatVersion": 1
}
```

`scriptFile` is removed from the new format.

## Data Format

Use the same storage philosophy as raythm content, while keeping MV ownership song-scoped:

- Local editable content is plain workspace data.
- Community / Official downloaded content is managed content cache data.
- The MV Editor works with the same logical `mv_composition` model in both cases.
- Storage services decide whether saving writes plain files or encrypted managed assets.
- A song owns its MV packages. MV ownership, persistence, publishing, unlocks, and selection are decided by song-level identity only.

Local editable package:

```text
mvs/{mv_id}/
  mv.json
  composition.rmvcomp
  assets/
    images/
    generated/
```

Managed Community / Official package:

```text
content-cache/{community|official}/mvs/{managed_mv_id}/
  managed-package.json
  .encrypted/
    assets/
      *.renc
```

Managed MV files are logical files backed by encrypted assets. For example, `composition.rmvcomp` remains the logical authoring file, but it may be stored physically as `.encrypted/assets/{hash}.renc` and described by `managed-package.json`.

Portable package:

```text
*.rmvpack
  mv.json
  composition.rmvcomp
  assets/
    images/
    generated/
```

`.rmvpack` is the user-facing archive for MV import/export and the natural upload payload for Browse. It carries package assets, unlike a standalone `.rmvcomp` export. Standalone `.rmvcomp` remains useful for debugging, review, and future advanced workflows that intentionally reference already-installed assets.

Implementation foundation:

- `content_cache_paths::mv_cache_key` and `content_cache_paths::mv_dir` reserve `content-cache/{community|official}/mvs/{mv_id}/` for managed MV packages.
- `mv::managed_storage::package_manifest` stores song-scoped MV identity, local/remote hashes, unlock metadata, encryption metadata, and logical encrypted assets.
- `mv::managed_storage::write_composition` serializes `mv_composition`, writes it through `managed_content_storage::write_encrypted_asset`, updates local `mvHash` / `mvFingerprint`, and preserves remote hashes.
- `mv::managed_storage::read_composition` decrypts the logical `composition.rmvcomp` asset and parses the same authoring model used by local packages.
- `mv_storage::save_metadata` writes `mv.json` as plain text for local MV packages, but writes the logical `mv.json` through `mv::managed_storage::write_mv_json_asset` for managed packages.
- `mv::managed_storage::write_asset_file` and `read_asset_file` store package-local MV assets, such as imported images, as encrypted logical files in the managed manifest.
- `mv_storage` scans local `mvs/` packages and managed `content-cache/{source}/mvs/` packages, so the editor/play entry points can use the same package lookup API.
- `mv_storage::read_asset_bytes` lets preview/play load local assets from disk or managed assets from decrypted bytes without leaking plain managed asset paths.
- `mv_storage::export_composition` exports the logical composition through `load_composition`, so managed packages decrypt before writing a plain user export and do not depend on a physical `composition.rmvcomp` file in the cache.
- `mv_storage::export_package` writes a plain `.rmvpack` from either local or managed packages by serializing `mv.json`, normalizing `composition.rmvcomp`, and reading each referenced asset through `read_asset_bytes`.
- `mv_storage::import_package` extracts `.rmvpack`, validates `mv.json`, `composition.rmvcomp`, and referenced assets, then installs a normalized package under local `mvs/{mv_id}/` with the target song ID supplied by the caller. It rebuilds the destination package from metadata, composition, and referenced asset bytes instead of copying arbitrary archive contents.
- `mv_storage` rejects `compositionFile` and asset paths that are absolute, drive-relative, or traverse outside the package. Managed MV asset logical paths use the same package-relative rule before encryption.

The local editable package should stay human-inspectable and editor-friendly. `composition.rmvcomp` should be UTF-8 JSON, not a custom line format like `.rchart`, because MV authoring data is nested, sparse, versioned, and expected to evolve. However, the file should still be deterministic and strict: stable ordering, explicit units, required fields, version checks, and canonical serialization matter for hashes and managed-content modified detection.

New song-scoped MV compositions initialize root `durationMs` and default full-song layers from the parent song duration. If the song duration is unavailable, the editor uses a long authoring fallback instead of an 8-second preview-sized clip. Layer `startMs`, layer `durationMs`, keyframe `timeMs`, and `EventTrigger.timeMs` are all song-timeline milliseconds.

`composition.rmvcomp` example shape:

```json
{
  "format": "raythm.mv.composition",
  "formatVersion": 1,
  "compositionId": "uuid",
  "canvas": {
    "width": 1920,
    "height": 1080,
    "background": "#101216"
  },
  "durationMs": 132000,
  "layers": [
    {
      "id": "layer-bg",
      "name": "Bass Pulse BG",
      "visible": true,
      "locked": false,
      "z": 0,
      "startMs": 0,
      "durationMs": 132000,
      "source": {
        "type": "shape",
        "shape": "rect",
        "fill": "#171a21"
      },
      "transform": {
        "position": [960, 540],
        "scale": [1, 1],
        "rotationDeg": 0,
        "anchor": [0.5, 0.5],
        "opacity": 1
      },
      "effects": [
        {
          "id": "fx-pulse",
          "type": "audioLevelReactive",
          "target": "opacity",
          "amount": 0.18
        }
      ],
      "keyframes": [],
      "eventTriggers": [
        {
          "event": "section.chorus",
          "actions": [
            {
              "type": "triggerEffect",
              "effectId": "fx-pulse"
            }
          ]
        }
      ]
    }
  ],
  "assets": [
    {
      "id": "asset-jacket",
      "type": "image",
      "path": "assets/images/jacket.png",
      "sha256": "..."
    }
  ]
}
```

Rules:

- All time fields must name their unit, usually `Ms`.
- Authoring IDs are stable strings so editor undo/redo and references survive reorder/rename.
- Layer IDs and asset IDs must be unique inside a composition; duplicate IDs fail validation.
- Effect IDs must be non-empty and unique within their layer; `triggerEffect` actions must reference an existing effect ID on the same layer.
- Runtime IDs are generated by the baker and can be dense integers.
- Asset paths are package-relative only. Absolute paths are forbidden in saved/public packages.
- Imported image assets are copied into `assets/images/` and referenced by asset ID from image layers.
- Asset IDs and filenames should be content-hash-derived when practical so duplicate imports converge and managed manifests can track stable logical files.
- Image layers must reference an existing composition asset id; missing or dangling `assetId` values fail validation before playback/export.
- Keyframe tracks use `target` plus sorted `points`, where each point has `timeMs`, `value`, and `easing`.
- Initial keyframe targets are numeric transform properties: `transform.position.x`, `transform.position.y`, `transform.scale.x`, `transform.scale.y`, `transform.rotationDeg`, and `transform.opacity`.
- Unknown fields should be preserved when practical, or ignored with diagnostics when not supported.
- Unsupported `formatVersion` should fail clearly before playback.

## Browse And Publishing Format

Browse should treat MV as its own song-scoped content type. An MV belongs to a song, and its package identity is derived from MV/song metadata only.

`mv.json` stays the local/public content metadata:

```json
{
  "mvId": "uuid",
  "songId": "song-id",
  "name": "New MV",
  "author": "",
  "description": "",
  "compositionFile": "composition.rmvcomp",
  "formatVersion": 1,
  "visibility": "public",
  "tags": ["motion", "lyrics"],
  "links": {
    "sourceSongId": "song-id"
  }
}
```

Browse catalog fields should be lightweight and should not require installing the managed package:

- remote MV id
- title/name
- author/uploader
- target song id/title/artist
- duration
- thumbnail/preview image URL
- file size
- composition hash / fingerprint
- remote composition hash / fingerprint
- format version
- content source: `community` or `official`
- unlock metadata
- visibility
- like/download counts

Download/install should mirror the existing managed content approach:

- Put Community / Official MV packages under `content-cache/{community|official}/mvs/`.
- Accept `.rmvpack` as the upload/install package shape before server-side conversion into managed encrypted assets.
- Store public identity, remote version, hashes, license/unlock metadata, and encrypted asset metadata in `managed-package.json`.
- Store `mv.json`, `composition.rmvcomp`, and MV assets as logical managed assets.
- Use remote hashes/fingerprints for update checks and local hashes/fingerprints for modified detection.
- Keep Browse listing metadata server-side and cache only the installed package data locally.

## Managed Encryption And Editing

Treat encryption as a managed-content storage detail, not as a separate MV format.

The editor flow should match managed-content editing semantics for song-owned MV packages:

```text
load:
  plain composition.rmvcomp
  or managed encrypted composition asset
  -> mv_composition

edit:
  mv_editor_state mutates mv_composition

save:
  local package -> write plain composition.rmvcomp/assets
  managed package -> re-encrypt managed mv.json/composition/assets and update managed-package.json hashes
```

Managed MV manifest should mirror the existing managed-content manifest vocabulary where possible:

```json
{
  "schemaVersion": 1,
  "contentSource": "community",
  "serverUrl": "https://...",
  "remoteMvId": "remote-mv",
  "localMvId": "mv_cache_key",
  "mvVersion": 1,
  "revisionId": "revision",
  "packageId": "package",
  "keyId": "content-key",
  "contentKeyVersion": 1,
  "encryptionScheme": "raythm-dev-sha256-stream-v1",
  "license": {
    "expiresAt": "",
    "offlineExpiresAt": "",
    "revoked": false
  },
  "compositionHash": "...",
  "compositionFingerprint": "...",
  "remoteCompositionHash": "...",
  "remoteCompositionFingerprint": "...",
  "unlock": {},
  "assets": {
    "mvJson": {},
    "composition": {},
    "thumbnail": {},
    "files": []
  }
}
```

Rules:

- Local saves remain plain and editable.
- Managed saves remain managed and encrypted.
- Editing a Community / Official MV should not silently convert it into a plain local MV.
- Managed editor saves must not create plain `mv.json`, `composition.rmvcomp`, or `assets/...` logical files inside the content cache.
- If a managed MV is edited, update local `compositionHash` / `compositionFingerprint`; keep remote hashes unchanged so catalog status can become `modified`.
- If the content is locked, expired, revoked, or sealed, block editing before opening the editor or open read-only.
- `mv::managed_storage::can_edit` is the local edit gate for managed MV packages. It denies editing when the package license is revoked, all known license windows are expired, or unlock metadata says the content is locked/not playable.
- Managed composition and asset writes call the edit gate before re-encrypting data, so UI mistakes cannot silently mutate locked Community / Official packages.
- Do not store absolute source asset paths in either plain or managed packages.
- Do not put secret keys in `mv.json` or `composition.rmvcomp`.
- Client-side encryption is protection, not perfect DRM. Server entitlement and license checks still matter for paid/subscription content.

Current implementation status:

- `mv_managed_storage_smoke` proves managed MV composition encryption, decryption, manifest round-trip, local hash/fingerprint update, and remote hash preservation.
- `mv_storage_smoke` proves managed MV packages are discoverable by local song ID and that `load_composition` / `save_composition` decrypt and re-encrypt through the normal MV storage API.
- `content_cache_paths_smoke` proves MV managed packages live under the `mvs` bucket and do not use chart storage.
- `mv_storage` no longer exposes `.rmv` script load/save/import/export APIs; `mv.json` is composition-first and writes `compositionFile`.
- `mv_storage_smoke` proves `.rmvpack` export/import keeps image assets with the composition and can export a managed MV package by decrypting logical assets through the storage API.
- `mv_storage_smoke` also proves managed MV metadata saves update encrypted `mv.json` assets without creating a plain `mv.json` in the managed cache.
- `mv_managed_storage_smoke` proves revoked, expired, or locked managed MV packages reject composition and asset writes before encryption.
- Browse/download install is not implemented yet, but once it creates a managed MV package manifest, the existing MV storage API can load and save that package without converting it to plain local files.

This keeps the same clean separation used by managed raythm content:

- Logical authoring format: `mv_composition`.
- Local physical storage: plain `mv.json + composition.rmvcomp + assets`.
- Community / Official physical storage: `managed-package.json + encrypted assets`.
- Runtime input: baked runtime composition from the same logical model.
- Ownership and Browse identity: song-scoped MV content, independent from chart selection.

## Editor Experience

The MV Editor is a real production surface, not a script text box.

Layout:

```text
Top Bar
  Save / Play / Pause / Current Time / Zoom / Add Layer / Add Asset

Left
  Layer List

Center
  Preview

Bottom
  Timeline

Right
  Inspector
```

Feature boundaries:

- `mv_editor_scene`: lifecycle, input gathering, controller calls, view calls, navigation.
- `mv_editor_state`: selection, timeline zoom, timeline scroll, playhead, playback state, dirty state, diagnostics.
- `mv_editor_controller`: add/delete/select/reorder layers, scrub timeline, save request, preview toggle, edit commands.
- `mv_editor_view`: draw layout and return UI commands only.
- `mv_composition_edit_service`: mutate composition in command-sized operations.
- `mv_asset_service`: import, copy, validate, and resolve package assets.
- `mv_composition_preview_controller`: bake authoring data and provide runtime preview result.

The editor should exercise the same authoring data that play scene consumes. If the editor uses a separate fake preview model, the design has failed.

## Runtime Architecture

```text
Authoring MVComposition
  -> MVBaker
Runtime MVComposition
  -> Runtime Evaluator
Render Commands
  -> MV Renderer
```

Baker responsibilities:

- Resolve layer names and stable IDs.
- Resolve asset references to package-local asset handles.
- Resolve effect references.
- Resolve event names to interned event IDs.
- Resolve keyframe target properties.
- Sort layers into runtime draw order.
- Validate layer durations, keyframe order, unsupported source/effect combinations, and missing assets.
- Convert authoring-friendly data into arrays and ID-based runtime structures.

Runtime systems:

- Timeline system
- Keyframe animation system
- Effect system
- Audio reactive system
- Event trigger system
- Generated source system
- Render command builder

Avoid in runtime:

- per-frame layer-name string lookup
- per-frame package path resolution
- per-frame parsing
- always-running script VM as the standard path
- dynamic component lookup as the common path

## Song Visual Event Integration

EventTrigger is the bridge between song/MV timeline events and MV runtime behavior.

Expected event shape:

```text
timeMs
eventName
payload
duration?
target?
```

Composition side:

```text
EventTrigger
  eventName
  timeMs?          // optional song timeline dispatch point
  optional target
  actionSequence
```

`timeMs` is an MV/song timeline value. A trigger may be fired either by an external song visual event name or automatically when playback crosses its `timeMs`. This keeps simple MV cues self-contained in the song-owned MV package while leaving room for richer song visual events later.

Example event names:

- `section.intro`
- `section.verse`
- `section.chorus`
- `section.drop`
- `effect.flash`
- `effect.pulse`
- `text.lyric`
- `custom.drop`

Rules:

- Unknown events are ignored safely.
- Event names are interned during bake.
- Runtime actions target layer/effect/property IDs.
- Event payload parsing must be bounded and failure-tolerant.
- Events should be usable by both play scene and editor preview.

## Stage 0: Policy Lock

- Document that old MV script compatibility is intentionally dropped.
- Mark `.rmv` import/export, MV script samples, script editor, script completion, script runtime, and MV language smoke tests as removal targets.
- Treat #312 as the parent issue for the internal MV production editor, not only the data model refactor.

Done when:

- The repo has a written plan for replacing old MV script with a production-oriented Composition Editor.
- Future implementation work does not need to preserve `script.rmv` compatibility.

## Stage 1: Authoring Model

Add `src/mv/composition/` and define the GUI-editable model.

Done when:

- A composition can represent a static layered MV without runtime or UI dependencies.
- All time fields make units explicit in names.
- Source, Transform, Effect, Keyframe, EventTrigger, and Asset references are represented in authoring data.

## Stage 2: Storage And Asset Packages

Add serializer/parser for `composition.rmvcomp`.

Implementation notes:

- Use structured JSON parsing for `composition.rmvcomp`; the file is nested enough that ad hoc string parsing is not acceptable.
- Keep `network::json_helpers` for small metadata files where it already fits the surrounding storage code.
- Keep parser errors structured enough for editor display.
- Add `composition_path`, `load_composition`, and `save_composition` helpers to MV storage or a dedicated composition storage service.
- Add package asset import/copy/resolve helpers.
- Image import should copy PNG/JPEG files into `assets/images/`, store only package-relative paths in `mv_asset`, and reject absolute or parent-traversing paths.
- Update new MV package creation to write `mv.json`, an empty/default `composition.rmvcomp`, and `assets/`.
- Replace `scriptFile` with `compositionFile` for new packages.

Done when:

- `mv_composition_storage_smoke` round-trips composition packages and assets.
- New MV creation no longer creates `script.rmv`.
- Missing/invalid assets produce editor-visible diagnostics instead of crashes.

## Stage 3: Baker And Runtime Model

Add a bake step from Authoring MVComposition to Runtime MVComposition.

Runtime model should favor arrays and ids:

```text
runtime_composition
  runtime_layer[]
  runtime_source[]
  runtime_asset[]
  runtime_transform_track[]
  runtime_effect_instance[]
  runtime_event_subscription[]
```

Done when:

- Invalid authoring data fails before playback.
- Runtime evaluation does not need layer-name string lookup per frame.
- Asset references are resolved before runtime evaluation.

## Stage 4: Minimal Runtime Preview

Implement runtime evaluation from:

```text
runtime_composition + context_input + current_ms
```

to render commands.

Initial behavior:

- Layer visibility by time range.
- Static transform evaluation.
- Static source rendering.
- Opacity composition.
- Static background, rect, circle, line, text, and image rendering.
- Basic generated spectrum/waveform placeholders backed by runtime input.

The first implementation may convert to existing `mv::scene` and use the existing renderer. Treat this as an internal bridge, not as a public MV API.

Done when:

- A smoke test can run `composition -> bake -> tick -> render commands`.
- Static background, rect, circle, line, text, and image layers render through the internal command path.

## Stage 5: Remove Old Runtime Path

Replace play-side MV loading.

Tasks:

- Update `play_mv_controller` to load `composition.rmvcomp`.
- Remove calls to old `mv_runtime`.
- Remove `script.rmv` assumptions from MV storage.
- Remove `.rmv` import/export commands from song select.
- Remove old MV script editor panel and style.
- Remove old MV language/runtime smoke tests or replace them with composition runtime smoke tests.
- Delete `documents/mv-script-samples.rmv`.

Removal targets:

- `src/mv/lang/*`
- `src/mv/api/mv_builtins.*`
- `src/mv/mv_runtime.*`
- `src/mv/mv_script_panel.*`
- `src/mv/mv_script_editor_style.*`
- `mv_lang_smoke`
- `mv_api_smoke`
- `mv_runtime_benchmark_smoke`

Done when:

- Gameplay no longer reads or compiles `script.rmv`.
- Building the repo does not compile the old MV language.

Current implementation status:

- `play_mv_controller` loads song-scoped `composition.rmvcomp` packages through `mv_storage`.
- Song Select user-facing import/export uses asset-inclusive `.rmvpack`, not `.rmv`; standalone `.rmvcomp` remains an internal/debug composition export.
- `mv_storage` no longer has script file helpers or `scriptFile` metadata.
- The old MV script editor, VM/runtime, render command API, renderer, validator, and their smoke targets have been removed from the build.

## Stage 6: Internal Production Editor Shell

Replace `mv_editor_scene` with an internal Composition Editor shell early.

The editor is not public-facing yet, but it must be usable enough to validate that the authoring model feels natural in GUI.

Current implementation status:

- `mv_editor_scene` opens and saves `mv_composition` directly instead of editing `.rmv` source text.
- The first editor shell shows Layer List, Preview, Timeline, and Inspector in one production-style surface.
- The shell supports save, play/pause preview, current time, playhead scrub, add text layer, add rect layer, add image layer, select layer, delete layer, and metadata editing.
- Inspector editing currently supports visibility, lock state, position, uniform scale, opacity, layer start, and layer duration for the selected layer.
- The editor can set transform keyframes at the current playhead; keyframes are serialized, parsed, and linearly evaluated in preview/play.
- Preview and play scene both consume the same `composition.rmvcomp` authoring data for static background/text/rect/image rendering.
- Image assets are imported into the MV package, referenced through composition assets, and loaded from package-local paths.
- Managed image assets are encrypted in `managed-package.json` `assets.files` and decoded from decrypted bytes for preview/play.
- The old `mv_script_panel` and MV language/runtime files are removed; the editor is composition-only.

Done when:

- The old script editor is gone.
- The editor opens a composition package.
- Save, play/pause preview, current time, zoom, add layer, and add asset controls exist.
- Layer List, Preview, Timeline, and Inspector are visible and routed through state/controller/view boundaries.

## Stage 7: Basic Editing

Implement the minimum useful editor behavior.

Layer List:

- add layer
- delete layer
- select layer
- reorder layers
- toggle visibility
- toggle lock
- rename layer

Inspector:

- source type
- source properties
- asset selection for image layers
- position
- scale
- rotation
- anchor
- opacity
- start/duration

Timeline:

- playhead scrub
- layer duration bars
- drag layer start/end
- drag layer span to move start time while preserving duration
- zoom and horizontal scroll

Preview:

- evaluate the current composition at playhead time
- show selected layer bounds and anchor
- show bake/storage/asset errors without crashing

Done when:

- A basic MV can be authored locally without script editing.
- The editor and play scene consume the same composition/runtime path.

Current implementation status:

- Timeline supports playhead scrub, visible layer duration bars, layer span move, and start/end trimming for unlocked layers.
- Locked layers are visually muted and do not accept timeline drag edits.
- Inspector can edit selected layer name, transform sliders, visibility, lock state, source fill color, and text source content.
- The layer list can move the selected layer forward/back in the stack; the editor normalizes `z` from list order so saved draw order stays deterministic.
- Timeline zoom, horizontal scroll, per-layer track selection polish, drag-to-reorder, and richer source-specific property editors are still pending.

## Stage 8: Keyframe Tracks

Add keyframe support for common properties.

Current implementation status:

- Numeric keyframe tracks exist in the authoring model.
- Serializer/parser round-trips `target` and sorted `points`.
- Preview/play evaluate transform position, scale, rotation, and opacity with linear interpolation.
- Editor can write transform keyframes for the selected layer at the current playhead.
- Timeline shows transform keyframe markers per layer.
- Editor can clear transform keyframes at the current playhead without deleting non-transform tracks.
- Timeline does not yet expose per-track lanes, point dragging, or interpolation selection.

Initial interpolation:

- hold
- linear
- easeIn
- easeOut
- easeInOut

Editor additions:

- keyframe markers on timeline
- selected keyframe editing in inspector
- add/remove keyframe command
- keyframe value editing for transform, opacity, color, and effect parameters

Done when:

- Runtime and preview evaluate keyframes deterministically.
- Keyframe edits are command-sized and ready for undo/redo.

## Stage 9: Standard Effects And Presets

Add data-driven effects as the main path for common MV expressions.

Initial effects:

- fade
- pulse
- flash
- shake
- glow
- color shift
- beat pulse
- audio level reactive

Preset examples:

- chorus flash
- beat ring
- lyric pop
- bass pulse background
- jacket zoom
- spectrum floor
- drop shake

Rules:

- Effects are authoring data.
- Runtime effects are C++ systems.
- Presets expand into normal layers/effects/keyframes where possible.
- Do not reintroduce free-form direct drawing scripts for standard expressions.

Current implementation status:

- `effect` entries round-trip in `composition.rmvcomp`.
- Runtime/editor preview evaluate `fade`, `pulse`, `flash`, and `shake` effects through `mv::composition::evaluate_transform`.
- The editor Inspector can add `Fade`, `Pulse`, `Flash`, and `Shake`, clear effects, and adjust fade duration / pulse / flash / shake amount for the selected layer.
- `mv::composition::apply_preset` expands built-in presets into normal layers/effects/keyframes, so presets do not introduce a second MV runtime format.
- The editor layer panel can add `Flash`, `Lyric`, and `Bass` presets; these are saved as ordinary `composition.rmvcomp` data and remain compatible with managed encryption.
- Effect ordering UI, per-effect selection, user-authored preset packages, keyframed effect parameters, and audio-reactive effects are still pending.

Done when:

- Common motion graphics effects can be authored without script hooks.
- Effect parameter changes can be keyframed.
- At least a small preset library exists to make a first MV quickly.

## Stage 10: Generated Sources And Audio Reactivity

Add generated visual sources that make raythm MV feel native.

Initial generated sources:

- spectrum bars
- waveform
- beat grid
- radial pulse
- lane/input pulse if gameplay context is available

Runtime inputs:

- audio level
- spectrum bands
- beat phase
- song time
- optional gameplay state such as combo or accuracy

Current implementation status:

- `beatGrid`, `waveform`, and `spectrum` generated sources round-trip in `composition.rmvcomp`.
- The editor can add Beat Grid, Waveform, and Spectrum layers from the layer panel.
- Editor preview renders Beat Grid, Waveform, and Spectrum with deterministic song-time motion.
- Play scene renders Spectrum from `audio_manager::get_bgm_fft256` when BGM FFT data is available, with deterministic fallback motion otherwise.
- Real audio waveform data, audio level, beat phase from timing data, and gameplay-reactive generated sources are still pending.

Done when:

- Generated sources render in editor preview and play scene.
- Audio reactive behavior is deterministic enough for preview and does not require script execution.

## Stage 11: EventTrigger

Integrate with song visual events and MV timeline events as song-level synchronization inputs.

Runtime behavior:

- Playback can notify MV runtime of song visual events.
- Playback fires MV-local `EventTrigger.timeMs` actions when song visual time crosses the trigger point.
- Editor preview can simulate or scrub through event-triggered actions.
- Unknown events are ignored safely.

Current implementation status:

- `eventTriggers` already round-trip in `composition.rmvcomp` as normal authoring data.
- `mv::composition::apply_event` evaluates matching event names without script execution.
- `EventTrigger.timeMs` round-trips in `composition.rmvcomp`.
- `mv::composition::apply_timeline_events` applies MV-local triggers when playback crosses their song timeline time.
- `mv::composition::add_flash_cue`, `add_show_cue`, and `add_text_cue` keep editor cue authoring deterministic and de-duplicate cues near the playhead.
- `play_mv_controller` dispatches MV-local timeline triggers during composition playback and exposes `notify_song_visual_event` for future song-level event sources.
- The MV editor Inspector can add `Cue Flash`, `Cue Show`, and text-layer `Cue Text` at the current playhead, clear cues near the playhead, and shows cue markers on the timeline.
- Initial actions support `showLayer`, `hideLayer`, `setText`, `setProperty`, and `triggerEffect`.
- `setProperty` covers visibility, source text/fill, transform opacity, position, scale, and rotation.
- `triggerEffect` can restart an existing layer effect at the event time, which lets effects like `flash` respond to `section.chorus` or `effect.flash`.
- Serializer validation rejects missing/duplicate effect IDs and `triggerEffect` actions that point at missing effects.
- External song visual event sources and arbitrary event simulation UI are still pending.

Done when:

- Composition triggers can react without script execution.
- Event-triggered text, flash, layer visibility, and animation actions work.

## Stage 12: Undo/Redo And Editing Reliability

Before exposing the editor as a serious feature, editing operations should become command-sized and reversible.

Initial undoable commands:

- add/delete layer
- reorder layer
- edit transform
- edit source property
- edit timeline range
- add/remove/edit keyframe
- add/remove/edit effect
- import/remove asset

Current implementation status:

- `mv::composition::edit_history` stores bounded composition snapshots with selected-layer state and clean fingerprints.
- The MV editor exposes Undo / Redo buttons plus `Ctrl+Z`, `Ctrl+Y`, and `Ctrl+Shift+Z`.
- Add Text / Rect / Image / Beat Grid / Waveform, delete/reorder layer, layer name edits, source text/fill edits, visibility/lock toggles, timeline range edits, transform sliders, keyframe set/clear, and Fade/Pulse/Flash/Shake effect edits are undoable through the shared history.
- Save marks the current composition fingerprint clean, so undo/redo can restore the `Save *` indicator correctly.
- The current implementation is snapshot-based to make the editor reliable quickly; the intended next step is command objects that can also handle asset import/removal, drag-to-reorder, and more granular source/effect operations.

Done when:

- The editor can recover from common mistakes without manual file repair.
- Dirty state and save prompts reflect undo/redo correctly.

## Stage 13: ScriptBehaviour Reconsideration

Do not port old MV scripts.

If a script-like extension is still needed, define it as an optional advanced hook that manipulates Composition data or runtime parameters.

Allowed direction:

- `onStart`
- `onEvent`
- `onBeat`
- `onUpdate`
- layer/effect/property control APIs

Disallowed direction:

- direct draw API
- arbitrary replacement for Composition authoring
- always-running VM as the standard effect path

Done when:

- The project has enough standard effects, generated sources, and triggers to decide whether scripting is still necessary.

## Stage 14: Public Feature Preparation

This is outside the first internal implementation, but the internal design should not block it.

Future public work:

- user-facing templates
- composition package import/export
- docs and examples
- online sharing support
- compatibility checks for shared MVs
- asset packaging validation
- generated preset packs

Done when:

- Internal composition editor and runtime are stable enough to expose intentionally.

## Suggested Implementation Order

1. Stage 1 to Stage 4: build the new internal model/runtime path.
2. Stage 5: remove the old script runtime from gameplay and build.
3. Stage 6 to Stage 7: replace the editor with a usable internal production editor.
4. Stage 8 to Stage 10: add keyframes, standard effects, presets, generated sources, and audio reactivity.
5. Stage 11: connect song visual events and MV EventTrigger.
6. Stage 12: make editing reliable with undo/redo.
7. Stage 13 to Stage 14: revisit advanced scripting and public feature exposure.

This order avoids keeping two MV systems alive for long, while still giving the UI enough priority to validate the authoring model early. The editor should not wait until every runtime feature is complete; it is the feedback loop that proves the model can actually make MVs.

## Test Plan

Add focused smoke tests:

- `mv_composition_model_smoke` for model defaults, JSON round-trip, fingerprint stability, assets, event triggers, and unsupported version rejection.
- `mv_storage_smoke` for local package creation, `mv.json`, `composition.rmvcomp`, package asset directories, image asset import/copy/resolve, import/export, and composition id normalization.
- `mv_composition_storage_smoke` if MV storage splits out from the legacy `mv_storage_smoke` target.
- `mv_asset_storage_smoke`
- `mv_composition_baker_smoke`
- `mv_composition_runtime_smoke`
- `mv_keyframe_smoke`
- `mv_effect_smoke`
- `mv_composition_event_evaluator_smoke` for event name matching, unknown event ignore behavior, set/show actions, and triggerEffect evaluation.
- `mv_composition_event_authoring_smoke` for editor cue authoring helpers, cue de-duplication near the playhead, and timeline-triggered flash/show/text behavior.
- `mv_composition_editor_smoke` if editor state/controller can be tested without raylib rendering

Keep tests mostly inside pure model, storage, baker, runtime, and controller code. Renderer and Scene checks can remain smoke-level.

## Follow-Up Issue Split

Suggested follow-up issues:

- Define MV Composition authoring model.
- Add `composition.rmvcomp` storage and package asset management.
- Implement MV baker and runtime composition evaluator.
- Remove old `.rmv` script runtime and script editor.
- Build internal MV Production Editor shell.
- Implement basic layer/source/transform/timeline editing.
- Implement image asset import and package-local asset references.
- Implement keyframe tracks.
- Implement standard effects and presets.
- Implement generated sources and audio reactivity.
- Implement song visual events and EventTrigger integration.
- Add undo/redo for MV editing operations.
- Reconsider optional ScriptBehaviour.
- Prepare public-facing MV authoring, package import/export, and sharing features.
