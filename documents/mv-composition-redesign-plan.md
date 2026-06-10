# MV Composition Redesign Plan

Issue: https://github.com/Rofutok112/raythm/issues/312

## Goal

MV script / MV runtime を、既存の `script.rmv` と `draw(ctx)` 方式から、Composition / Layer / Keyframe 中心のオーサリングモデルへ置き換える。

この計画では、既存 MV script 互換は維持しない。ゲームは未リリースで既存 MV 資産も少ないため、後方互換よりも将来の GUI editor、event notes 連携、runtime performance、保守性を優先する。

公開・共有・テンプレート配布などの公開機能はこの計画の対象外とする。ただし UI は後回しにせず、内部 Composition Editor として早期に作り、モデルが GUI で自然に編集できるかを検証する。

## Decisions

- `script.rmv` / `draw(ctx)` / `DrawRect` 系 API は廃止する。
- 新しい MV 本体は `composition.rmvcomp` とする。
- `mv.json` は MV metadata と `compositionFile` を持つ package manifest に寄せる。
- 既存 `mv::scene` / renderer は、移行初期のみ内部 render command として流用してよい。
- ユーザー向けの第一級概念は Object / Component ではなく Composition / Layer / Source / Transform / Effect / Keyframe / EventTrigger とする。
- Authoring Model と Runtime Model は分ける。
- Runtime では layer / effect / event / property 参照を ID 化し、毎フレームの文字列探索を避ける。
- ScriptBehaviour は最初に作らない。必要になった場合も直接描画 API ではなく、Composition を操作する optional hook として再検討する。

## Target Model

```text
MVComposition
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

## Package Layout

```text
mvs/{mv_id}/
  mv.json
  composition.rmvcomp
  assets/
```

`mv.json` example:

```json
{
  "mvId": "uuid",
  "songId": "song-id",
  "name": "New MV",
  "author": "",
  "compositionFile": "composition.rmvcomp"
}
```

`scriptFile` is removed from the new format.

## Stage 0: Policy Lock

- Document that old MV script compatibility is intentionally dropped.
- Mark `.rmv` import/export, MV script samples, script editor, script completion, script runtime, and MV language smoke tests as removal targets.
- Keep the first implementation local-only and internal-editor-only.

Done when:

- The repo has a written plan for replacing old MV script with Composition.
- Future implementation work does not need to preserve `script.rmv` compatibility.

## Stage 1: Authoring Model

Add `src/mv/composition/` and define the GUI-editable model.

Core types:

- `mv_composition`
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

Initial layer fields:

- stable layer id
- name
- visible
- locked
- z order
- `start_ms`
- `duration_ms`
- `in_point_ms`
- `out_point_ms`
- source
- transform
- effects
- keyframe tracks
- event triggers

Initial transform fields:

- position
- scale
- rotation
- anchor
- opacity

Done when:

- A composition can represent a static layered MV without runtime or UI dependencies.
- All time fields make units explicit in names.

## Stage 2: Storage Format

Add serializer/parser for `composition.rmvcomp`.

Implementation notes:

- Prefer existing `network::json_helpers` instead of adding a JSON dependency.
- Keep parser errors structured enough for editor display.
- Add `composition_path`, `load_composition`, and `save_composition` helpers to MV storage or a dedicated composition storage service.
- Update new MV package creation to write `mv.json` and an empty/default `composition.rmvcomp`.
- Replace `scriptFile` with `compositionFile` for new packages.

Done when:

- `mv_storage_smoke` or a new storage smoke test round-trips composition packages.
- New MV creation no longer creates `script.rmv`.

## Stage 3: Baker And Runtime Model

Add a bake step:

```text
Authoring MVComposition
  -> MVBaker
Runtime MVComposition
```

Baker responsibilities:

- Resolve layer names and ids.
- Resolve effect references.
- Resolve event names to interned event ids.
- Resolve keyframe target properties.
- Sort layers and tracks into runtime order.
- Validate layer duration and keyframe order.
- Report dangling references and unsupported source/effect combinations.

Runtime model should favor arrays and ids:

```text
runtime_composition
  runtime_layer[]
  runtime_source[]
  runtime_transform_track[]
  runtime_effect_instance[]
  runtime_event_subscription[]
```

Done when:

- Invalid authoring data fails before playback.
- Runtime evaluation does not need layer-name string lookup per frame.

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
- Basic keyframe evaluation can be stubbed until Stage 8.

The first implementation may convert to existing `mv::scene` and use the existing renderer. Treat this as an internal bridge, not as a public MV API.

Done when:

- A smoke test can run `composition -> bake -> tick -> scene nodes`.
- Static background, rect, circle, line, and text layers render through the internal command path.

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

Keep or rename:

- `src/mv/api/mv_scene.h` can temporarily become an internal render command model.
- `src/mv/render/mv_renderer.*` can stay while Composition runtime outputs compatible commands.

Done when:

- Gameplay no longer reads or compiles `script.rmv`.
- Building the repo does not compile the old MV language.

## Stage 6: Internal Composition Editor UI

Replace `mv_editor_scene` with an internal Composition Editor.

The editor is not a public feature yet, but it should be usable enough to validate authoring model decisions.

Layout:

```text
Top Bar
  Save / Play / Pause / Current Time / Zoom / Add Layer

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
- `mv_editor_state`: selection, timeline zoom, playhead, playback state, dirty state, bake diagnostics.
- `mv_editor_controller`: add/delete/select/reorder layers, scrub timeline, save request, preview toggle.
- `mv_editor_view`: draw layout and return UI commands only.
- `mv_composition_edit_service`: mutate composition in command-sized operations.
- `mv_composition_preview_controller`: bake authoring data and provide runtime preview result.

Done when:

- The old script editor is gone.
- The editor can open a composition package, save it, preview it, and edit basic layer/source/transform fields.

## Stage 7: Basic Editing

Implement the minimum useful editor behavior.

Layer List:

- add layer
- delete layer
- select layer
- reorder layers
- toggle visibility
- rename layer

Inspector:

- source type
- source properties
- position
- scale
- rotation
- anchor
- opacity
- start/duration

Timeline:

- playhead scrub
- layer duration bars
- zoom and horizontal scroll

Preview:

- evaluate the current composition at playhead time
- show selected layer bounds and anchor
- show bake errors without crashing

Done when:

- A basic MV can be authored locally without script editing.
- The editor exercises the same authoring data that runtime playback consumes.

## Stage 8: Keyframe Tracks

Add keyframe support for common properties.

Initial targets:

- transform position
- transform scale
- transform rotation
- opacity
- color

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

Done when:

- Runtime and preview evaluate keyframes deterministically.
- Keyframe edits are command-sized and ready for undo/redo.

## Stage 9: Standard Effects

Add data-driven effects as the main path for common MV expressions.

Candidate initial effects:

- fade
- pulse
- flash
- shake
- color shift
- audio level reactive
- beat pulse

Rules:

- Effects are authoring data.
- Runtime effects are C++ systems.
- Do not reintroduce free-form direct drawing scripts for standard expressions.

Done when:

- Common motion graphics effects can be authored without script hooks.
- Effect parameter changes can be keyframed.

## Stage 10: EventTrigger

Integrate with event notes from Issue 311 after chart events exist.

Expected chart event shape:

- tick
- event name
- payload
- optional duration
- optional target

Composition side:

```text
EventTrigger
  eventName
  actionSequence
```

Example event names:

- `section.chorus`
- `effect.flash`
- `text.lyric`
- `custom.drop`

Runtime behavior:

- Unknown events are ignored.
- Event names are interned during bake.
- Actions target layer/effect/property ids.

Done when:

- Playback can notify MV runtime of chart events.
- Composition triggers can react without script execution.

## Stage 11: ScriptBehaviour Reconsideration

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

- The project has enough standard effects and triggers to decide whether scripting is still necessary.

## Stage 12: Public Feature Preparation

This is outside the initial implementation scope, but should be split after the internal system works.

Future public work:

- user-facing templates
- import/export for composition packages
- docs and examples
- online sharing support
- compatibility checks for shared MVs
- asset packaging
- optional generated presets

Done when:

- Internal composition editor and runtime are stable enough to expose intentionally.

## Suggested Implementation Order

1. Stage 1 to Stage 4: build the new internal model/runtime path.
2. Stage 5: remove the old script runtime from gameplay and build.
3. Stage 6 to Stage 7: replace the editor with internal Composition UI.
4. Stage 8 to Stage 10: add time variation, effects, and event notes.
5. Stage 11 to Stage 12: revisit advanced scripting and public features.

This order avoids keeping two MV systems alive for long, while still giving the UI enough priority to validate the authoring model early.

## Test Plan

Add focused smoke tests:

- `mv_composition_model_smoke`
- `mv_composition_storage_smoke`
- `mv_composition_baker_smoke`
- `mv_composition_runtime_smoke`
- `mv_composition_editor_smoke` if editor state/controller can be tested without raylib rendering

Keep tests mostly inside pure model, storage, baker, runtime, and controller code. Renderer and Scene checks can remain smoke-level.

## Follow-Up Issue Split

Suggested follow-up issues:

- Define MV Composition authoring model.
- Add `composition.rmvcomp` storage and package manifest migration.
- Implement MV baker and runtime composition evaluator.
- Remove old `.rmv` script runtime and script editor.
- Build internal Composition Editor shell.
- Implement basic layer/source/transform editing.
- Implement keyframe tracks.
- Implement standard effects.
- Implement event notes and EventTrigger integration.
- Reconsider optional ScriptBehaviour.
- Prepare public-facing MV authoring features.
