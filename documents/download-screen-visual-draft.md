# Download Screen Visual Draft

## Intent

This is a visual direction draft for the HOME-driven browse/download screen.

The screen should not feel like the current song select with a different label.
It should instead feel like:

- a continuation of `HOME -> BROWSE`
- a lighter browsing surface
- a catalog that is song-first and chart-second
- a screen where `OFFICIAL` and `COMMUNITY` are clearly separated

## Confirmed Product Rules

- songs do not duplicate
- every chart belongs to exactly one song
- therefore the browsing structure is always `song -> charts`
- search is required
- official and unofficial content should not be mixed in one list
- only server-published entries appear in `BROWSE`
- local-only songs should never appear as list items in this screen

## Core Layout Decision

Use a `top mode rail + left song lane with integrated search + right detail stage` composition.

This is intentionally different from the current song select.

Reasons:

- the left lane header keeps search attached to the song list it controls
- the left lane keeps fast vertical scanning for songs
- the right stage gives enough room for parent song details and child charts
- the whole screen can still inherit the HOME atmosphere

## Screen Structure

### 1. Top Rail

Persistent across the whole screen.

- back to `HOME`
- `OFFICIAL / COMMUNITY` mode switch
- account / network state

The top rail should feel like it grew out of the HOME menu row, not like a hard app toolbar.

### 2. Left Song Lane

This is the primary browsing zone.

- lane title and state copy
- full-width search field embedded into the lane header
- vertical song results
- jacket thumbnail kept small
- title and artist remain primary
- installed / update badges are visible but quiet
- search matches title and artist only
- local state is only an annotation on server entries, never a separate source list

The selected song should feel like a soft-lit card, not a dense list row from the old song select.

### 3. Right Detail Stage

This is the focus area for the selected song.

Upper part:

- medium jacket
- title
- artist
- bpm / short metadata
- official or community marker

Lower part:

- chart list belonging to the selected song
- each chart row shows difficulty, key count, level, and status
- bottom-right action area for download / update / open local

## Recommended Visual Mood

Keep the current HOME feeling:

- wide composition
- airy spacing
- soft borders
- semi-transparent cards
- low-contrast panels

Avoid:

- strong dashboard framing
- giant hard boxes
- old song select balance
- a layout where jacket dominates the center alone

## Rough Wireframe

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│ HOME          OFFICIAL     COMMUNITY                                            ○      │
│                                                                                              │
│  SONGS                                                                       SELECTED SONG  │
│  Catalog unavailable                                                         OFFICIAL       │
│  [ Search songs / artists...                                               ]               │
│                                                                                              │
│  > [jk] Song A            INSTALLED              [ medium jacket ]                          │
│    [jk] Song B            UPDATE                 Title                                       │
│    [jk] Song C                                   Artist                                      │
│    [jk] Song D                                   BPM 174 / 4 Charts / Server Version        │
│    [jk] Song E                                                                                │
│                                                Charts                                         │
│  scroll lane                                    4K NORMAL    Lv.  7.8     INSTALLED        │
│                                                4K HYPER     Lv. 10.4     UPDATE           │
│                                                4K ANOTHER   Lv. 13.1     GET              │
│                                                6K HYPER     Lv. 11.9     GET              │
│                                                                                              │
│                                                [ Download Song ] [ Open Local ]             │
│                                                installed copy / update note                 │
│                                                                                              │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Why This Fits The Data Model

The screen always makes the song the main object.

- search returns songs
- left lane lists songs
- right stage shows the selected song
- charts never float independently from their parent song

This keeps the UI aligned with the rule that songs are unique and charts always belong to one song.

## OFFICIAL / COMMUNITY Separation

These should feel like distinct online spaces, not a small filter chip.

Recommended first pass:

- same scene shell
- same layout skeleton
- different active mode and catalog data
- slightly different helper copy and badges

Suggested mood difference:

- `OFFICIAL`: cleaner, curated, stable
- `COMMUNITY`: more exploratory, more update-aware, more user-content feeling

## Search Behavior

Search should be visible before the user starts browsing.

Rules:

- search is scoped to the active mode
- search result unit is song
- empty search shows the catalog normally
- no tag UI for now

The search field should not be hidden behind a modal or submenu.
It should live inside the left song lane rather than floating as a separate top-bar control.

Match targets:

- title
- artist

## Action Hierarchy

Primary action:

- `Download Song`

Secondary actions:

- `Open Local`
- per-chart install or update

`Open Local` is only shown as a follow-up action for a selected server entry that is already installed.

This prevents the UI from over-emphasizing chart-only operations before the parent song is understood.

## Transition From HOME

`BROWSE` on HOME should visually expand into this screen.

Desired feeling:

- the selected HOME card becomes the origin
- the top rail appears first
- the song lane slides in
- the selected-song stage fades up after that

This keeps continuity with the HOME-driven flow instead of hard-cutting into a utility screen.

## First Implementation Target

The first usable version only needs:

- `OFFICIAL / COMMUNITY` split
- integrated lane search bar
- song list
- selected song detail
- child chart list
- install / update state labels

The target is:

> HOME の雰囲気を保ちながら、現行曲選択とは別構成で、楽曲親子構造と検索を中心にしたダウンロード画面を成立させること
