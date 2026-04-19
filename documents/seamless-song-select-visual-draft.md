# Seamless Song Select Visual Draft

## Intent

This is a visual direction draft for the new HOME-driven song select.

The goal is not to preserve the current boxed admin-like layout.
Instead, the new scene should feel like a natural extension of the title/home world:

- less panel framing
- more open composition
- stronger motion continuity from `PLAY`
- more visible ranking information
- smaller jacket, larger information field

## Core Decisions

### 1. Jacket is smaller

The jacket should no longer dominate the screen.

Reasons:

- we want more room for ranking information
- the scene should feel lighter and less like a static detail page
- the song select should prioritize browsing rhythm-game data, not just cover art

### 2. Song list remains vertical

The song list should use vertical scrolling.

Reasons:

- easier to scan quickly
- matches the rhythm-game selection feel more naturally
- simpler to expand later with filters, download state, or online markers

### 3. Chart list remains close to the current model

The per-song chart display can stay structurally similar to the current version.

Reasons:

- it already works well
- chart difficulty comparison is easy in the current stacked style
- this avoids unnecessary rework while the main scene composition changes

### 4. Remove strong rectangles / boxed sections

The current hard-framed sections should be largely removed.

Instead:

- use spacing
- use alignment
- use subtle contrast shifts
- use section labels
- use motion and emphasis instead of thick panel borders

### 5. Rankings get more space

Because the jacket becomes smaller, rankings should become much more visible.

This screen should feel more like:

- song browsing
- chart comparison
- competitive context

and less like:

- metadata form

## Screen Structure

The new screen should feel like a wide composition with 3 reading zones:

1. Left: vertical song list
2. Center: current song + charts
3. Right: ranking-rich competitive view

## Rough Wireframe

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│ Song Select                                                                     Account ○   │
│ from PLAY                                                                        Online ✓   │
│                                                                                              │
│  SONGS                         SELECTED SONG                         RANKING                  │
│                                                                                              │
│  > Song A                       [ small jacket ]                     #01  Player        Score │
│    Song B                       Title                               #02  Player        Score │
│    Song C                       Artist                              #03  Player        Score │
│    Song D                       BPM / tags                          #04  Player        Score │
│    Song E                                                              ...                 │
│    Song F                       CHARTS                               Your Best              │
│    Song G                       4K HARD     Lv.12.4                  Rank / Score           │
│    Song H                       4K ANOTHER  Lv.14.8                  Accuracy / Combo       │
│    Song I                       6K HARD     Lv.13.1                  Updated time           │
│                                                                                              │
│                                 local rank / offset / status         local vs online toggle  │
│                                                                                              │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Visual Behavior

### Entry from HOME

The new song select should not simply replace the title scene.

Desired feeling:

- press `PLAY`
- the `PLAY` card becomes the origin of the next screen
- the HOME row stretches and dissolves into the new layout
- the song list and ranking column appear from that motion

### Ranking emphasis

Ranking should be readable at a glance.

This means:

- more visible row count than the current layout
- player name and score remain primary
- accuracy / combo / relative time remain secondary
- local best and online best should live in the same visual zone

## Layout Mood

Target mood:

- lighter
- wider
- more modern
- still game-like
- less utility-panel feeling

Avoid:

- too many framed boxes
- heavy dashboard look
- giant empty center dominated only by jacket art

## Suggested First Implementation

### Phase 1

- new scene with vertical song list
- smaller jacket
- chart area
- expanded ranking area
- reduced panel framing
- transition from `PLAY`

### Phase 2

- richer ranking interactions
- online / local switches
- song actions such as download markers
- ambient transition polish

## Non-Goals

For the first implementation, we do not need:

- full multiplayer integration
- full online catalog integration
- replacement of every old song-select feature

The first target is simply:

> HOME から自然につながる、新しい曲選択画面の見た目と導線を成立させること
