# Metadata Flow Normalization

Issue #315 tracks the release-time cleanup of song, chart, display, verification,
and binding metadata. This note records the current source-of-truth boundaries
used by the client after the normalization pass.

## Song Metadata

Authoritative fields:

- Local source: `song.json`
- Remote source: `/songs` and `/songs/:id`
- Runtime model: `song_meta`

The canonical runtime names are snake_case. Serialized JSON and remote payloads
use camelCase. `preview_start_ms` is the canonical preview unit; seconds are a
runtime convenience derived from it.

Newly written `song.json` files include `songId`. Loading still accepts older
files that omit it and falls back to the directory name, but new content should
not rely on that fallback.

## Chart Metadata

Authoritative fields:

- Local source: `.rchart` `[Metadata]`
- Remote source: `/charts`
- Runtime model: `chart_meta`

`chart_meta` contains chart identity and authored metadata only:
`chartId`, `songId`, `keyCount`, `difficulty`, `chartAuthor`, `formatVersion`,
`resolution`, and `offset`.

Newly written `.rchart` files include `chartId` and `songId` when available.
`level` remains excluded from the file format because runtime level is derived
from chart notes.

## Display Metadata

Authoritative fields:

- Local source: parsed `.rchart` content
- Remote source: `/charts`
- Runtime model: `song_select::chart_option`

Display metadata is kept separate from authored chart metadata:

- `note_count`: local `chart_data.notes.size()`, remote `noteCount`
- `min_bpm` / `max_bpm`: local BPM timing-event range, remote `minBpm` / `maxBpm`
- `level`: local calculated level cache, remote `calculatedLevel` with `level`
  accepted as a transition field

Remote chart display metadata is normalized into the same `chart_option` fields
used by local song select. If remote BPM range is not available yet, browse uses
the remote song `baseBpm` as a display fallback.

## Verification Metadata

Authoritative fields:

- Local source: computed hashes and fingerprints
- Remote source: manifest endpoints
- Runtime model: `ranking_client::*_manifest`

Manifest data is for verification only: content source, hashes, fingerprints,
and availability. It should not become the source of display metadata such as
BPM, note count, or calculated level.

## Local/Remote Binding

Authoritative fields:

- Local source: local content binding database
- Runtime model: `local_content_index`

Bindings map local IDs to remote IDs per server URL and origin. They are not a
metadata cache and should not duplicate song/chart display fields.

## Server Contract

Uploads use multipart form-data. The server can still parse uploaded files, but
the client now sends normalized metadata fields as the preferred contract.

Song upload fields:

- `metadataSchemaVersion`: `2`
- `contentSource`: `community`
- `clientSongId`: local song ID from `song.json`
- `title`
- `artist`
- `baseBpm`
- `durationSec`
- `previewStartMs`
- `songVersion`
- `songJsonSha256`
- `songJsonFingerprint`
- `audioSha256`
- `jacketSha256`
- `externalLinks`
- files: `audio`, `jacket`

Chart upload fields:

- `metadataSchemaVersion`: `2`
- `contentSource`: `community`
- `songId`: remote song ID assigned by the server
- `clientSongId`: local song ID
- `clientChartId`: local chart ID
- `keyCount`
- `difficultyName`
- `chartAuthor`
- `formatVersion`
- `resolution`
- `offset`
- `noteCount`
- `minBpm`
- `maxBpm`
- `calculatedLevel`
- `difficultyRulesetId`
- `difficultyRulesetVersion`
- `chartSha256`
- `chartFingerprint`
- file: `chart`

Chart catalog responses should return the same normalized display fields:
`noteCount`, `minBpm`, `maxBpm`, and `calculatedLevel`. The client accepts
`level` as a transition alias for `calculatedLevel`, and snake_case aliases for
server implementations that have not moved to camelCase yet.
