# Raythm Architecture Boundaries

This note fixes the vocabulary used by the current Feature refactor. It mirrors
`.codex/skills/raythm-architecture/SKILL.md` and is intentionally practical:
new code should choose one of these roles before it grows inside a Scene.

## Layers

- `Scene`: lifecycle entry point. It gathers frame input, calls Feature
  controllers, calls views/renderers, and applies navigation or audio side
  effects returned by controllers.
- `Feature State`: user-visible state for one experience, grouped by meaning
  such as catalog, selection, filters, scroll, preview, ranking, dialogs, and
  auth UI.
- `Controller`: `context -> result` state transitions. Controllers may request
  navigation, audio, reloads, saves, or notices, but should not hide those side
  effects inside rendering code.
- `View` / `Renderer`: draw state and report hit regions or commands. Views do
  not perform persistence, networking, Scene navigation, or long-running work.
- `Service`: application work such as catalog loading, import/export, ranking,
  upload/download, auth, save, and chart preparation.
- `Gameplay Domain`: deterministic timing, judge, score, performance, and chart
  event logic. It should not know about raylib, windows input, HTTP, SQLite, or
  filesystem paths.
- `Infrastructure`: audio device, platform input, filesystem, SQLite, HTTP,
  auth storage, external process, and other outside-world boundaries.

## Current Feature Boundaries

- `Play`: `play_session_state`, `play_flow_controller`, `play_renderer`, and
  `play_session_loader`. `play_mv_controller` owns MV package loading and MV
  runtime/audio visualization input, so `play_scene` no longer builds low-level
  MV render context in its draw path. Timing names should distinguish audio
  clock, chart time, visual time, and input event time.
- `Editor`: editor state/controllers/services/views. `editor_note_edit_service`
  owns note edit operations and `editor_timing_edit_service` owns timing and
  scroll automation edit operations. `editor_timing_action_controller` applies
  timing/scroll edit results and related selection/transport sync.
  `editor_screen_view` owns DAW screen draw composition, while
  `editor_screen_controller` applies the UI command results that used to live
  inside the Scene draw body. `editor_timeline_screen_controller` owns timeline
  preview/cursor presentation details. `editor_state` remains the command-history
  mutation owner so undo/redo granularity stays explicit.
- `Song Select`: `song_select::state` now stores catalog, selection, filters,
  scroll, preview, dialogs, ranking, and auth UI as separate state sections.
  `song_select::data_controller` owns catalog/ranking async reload and stale
  result handling for both the standalone scene and the Title hub.
- `Title Hub`: Title remains the hub Scene. Play/Create/Online/Profile/Settings
  mode logic should continue moving into mode controllers and data/transfer
  controllers. `title_startup_controller` owns startup loading/progress flow.
  `title_play_data_controller` keeps Title-only upload and scoring warmup while
  delegating shared Song Select data reloads. `title_hub_view` owns hub draw
  composition and returns login/profile commands for the Scene to apply.
- `Online Catalog`: existing `online_download_*` files still carry the old name.
  `online_catalog::data_controller` owns catalog, song page, chart page, owned
  library, download, and detail ranking futures. `title_online_view::state`
  keeps user-visible catalog data, filters, selection, and loading flags.
  `online_download_preview_controller` owns preview scrub/playback input so the
  update file does not talk to the audio layer for that interaction directly.
- `Song Create`: `song_create_scene` is still the form composition surface, but
  MIDI import parsing, timing normalization/validation, and song persistence
  live in `song_create_midi_importer`, `song_create_timing_service`, and
  `song_create_service`. Metadata form drawing lives in
  `song_create_form_panel`, genre/keyword editing lives in
  `song_create_tag_editor`, the created-song decision screen lives in
  `song_create_saved_view`, and the timing modal/summary UI is isolated in
  `song_create_timing_panel`. Timing edits, MIDI import application, and timing
  validation flow through `song_create_timing_controller`. The Scene now owns
  lifecycle, state coordination, audio preview application, and navigation while
  delegating view, persistence, and timing mutation details.
- `Gameplay Input`: `lane_input_tracker` is the pure domain-side state machine
  for lane held/press/release events. `input_handler` is the outer adapter that
  reads Windows or raylib input and feeds the tracker with `input_event_time_ms`.
- `Ranking`: ranking load/submit/cache work is a service boundary in
  `src/services/ranking_service.*`, not part of the pure Gameplay Domain.

## Migration Rules

- Add or change behavior in a controller/service first, then let Scene call it.
- Keep view updates as command extraction only; state mutation belongs in a
  controller.
- Keep async work behind `request_xxx()` and `poll_xxx()` APIs. Poll results
  should make success, failure, stale results, and queued reloads explicit.
- Prefer compatibility facades during large refactors. For example,
  `song_select::state` still exposes legacy field names while its storage is
  split into meaning-based sections.
