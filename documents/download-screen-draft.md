# Download Screen Draft

## Goal
- Add an in-game screen for browsing and downloading songs/charts from `raythm-server`.
- Keep this as a draft-only design note for now.
- Do not implement network/UI behavior in this branch.

## Entry Point
- `HOME -> ONLINE`
- The new screen should feel like the existing seamless hub flow, not like a hard scene break.

## Scope
- Public songs/charts can be browsed without login.
- Download only selected content instead of downloading everything.
- Show jacket, title, artist, and lightweight chart metadata.
- Separate this screen from upload/create tooling.

## Layout Direction
- Left column: vertical song list
- Center: selected song hero area
  - jacket
  - title / artist
  - short metadata
- Right column: available charts for the selected song
  - difficulty
  - key count
  - level
  - note count / BPM if available
- Bottom or lower-right action area:
  - `DOWNLOAD SONG`
  - `DOWNLOAD CHART`
  - `OPEN LOCAL`

## Interaction Notes
- Song list and chart list should both support scrolling.
- Download buttons should reflect state:
  - not downloaded
  - downloading
  - already downloaded
- If a local copy exists, surface that clearly instead of silently replacing it.

## State / Data Notes
- Use server song list as the remote source of truth.
- Preserve current local storage layout under `%LOCALAPPDATA%/raythm`.
- Keep room for future filters:
  - official only
  - downloaded only
  - search

## Open Questions
- Should `ONLINE` land directly on download browsing, or should it later split into `Download / Multiplayer / Rankings`?
- What should happen when a downloaded song already exists but server metadata changed?
- Do we want song-level download, chart-level download, or both from day one?
