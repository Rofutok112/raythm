# Seamless Song Select Draft

## Goal

Build a new song select scene that feels like a continuation of `Title -> HOME -> PLAY`,
instead of a hard scene cut into the existing song select UI.

This draft is intentionally separate from the current song select implementation.
The current scene stays intact until the new flow is ready.

## Direction

- Keep the current song select scene as a fallback for now.
- Create a new scene for the HOME-driven flow.
- Switch `PLAY` to the new scene only after the basic transition works.
- Optimize first for continuity and motion, then migrate features.

## Desired Feel

- Press `PLAY` on HOME.
- The selected HOME card becomes the visual origin of the transition.
- The title layout stretches or slides into the new scene instead of disappearing.
- The new song select appears as the next state of the same UI world.

## Transition Ideas

### Option A: Card expand

- `PLAY` card expands forward and becomes the left or center anchor of the next screen.
- Background spectrum and title layer fade but do not hard cut.
- Song thumbnails or cards fade in after the expansion settles.

### Option B: Horizontal slide

- HOME row shifts left.
- `PLAY` remains visually pinned while the song select columns slide in from the right.
- Good if the new song select is also horizontally structured.

### Option C: Depth dissolve

- HOME slightly zooms.
- Foreground cards dissolve into song jackets and metadata panels.
- Strongest continuity feel, but more expensive to tune.

## Recommended First Pass

Use `Option A: Card expand`.

Reasons:

- Easy to understand visually
- Keeps `PLAY` as the origin point
- Works well even before all song select features are migrated
- Can later be extended with more layered animation

## New Scene Scope

The new scene should start smaller than the current song select.

### Phase 1

- Song list or song cards
- Selected song title / artist / jacket
- Basic chart list for the selected song
- Back navigation to HOME-compatible title state

### Phase 2

- Ranking panel
- Local / online state
- Account-aware actions
- Preview / richer metadata

### Phase 3

- Download integration
- Online catalog hooks
- Multiplayer entry integration if needed

## Layout Principles

- Preserve the clean density established by the new title screen
- Avoid dropping immediately into the older dense admin-like layout
- Keep more whitespace and stronger focal hierarchy than the legacy song select
- Use larger anchors first, then secondary details

## Technical Notes

- Prefer a brand-new scene and layout module
- Avoid mutating the current song select scene into a hybrid
- Share reusable services only where it reduces duplication cleanly
- Transition data should include the source rect of the HOME `PLAY` card

## Acceptance Criteria

- Transition begins from the `PLAY` element on HOME
- The move from HOME to song select feels continuous
- The implementation is based on a new scene, not a direct patch-over of the current song select
- Existing song select remains usable during migration
