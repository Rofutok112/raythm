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

`songId` is required in `song.json`. The loader no longer falls back to the
directory name.

## Chart Metadata

Authoritative fields:

- Local source: `.rchart` `[Metadata]`
- Remote source: `/charts`
- Runtime model: `chart_meta`

`chart_meta` contains chart identity, authored metadata, and a runtime-only
parent song ID:
`chartId`, `keyCount`, `difficulty`, `chartAuthor`, `formatVersion`,
`resolution`, and `offset` are serialized in `.rchart`; `songId` is filled from
the parent song folder/catalog row after loading.

`.rchart` files must include `chartId`, `keyCount`, `difficulty`,
`chartAuthor`, `formatVersion`, `resolution`, and `offset`. `songId` is not a
file-format relationship field. `level` remains excluded from the file format
because runtime level is derived from chart notes.

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
- `visibility`: currently `public`
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
- files: `audio`, `jacket`

Chart upload fields:

- `metadataSchemaVersion`: `2`
- `contentSource`: `community`
- `songId`: remote song ID assigned by the server
- `clientSongId`: local song ID from the parent song, not from the `.rchart`
- `clientChartId`: local chart ID
- `visibility`: currently `public`
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

Upload success responses:

- Song upload: `{ "song": { "id": "<remoteSongId>" } }`
- Chart upload: `{ "chart": { "id": "<remoteChartId>" } }`

Paged catalog responses use `{ "items": [...], "total": number }`. Song detail
uses `{ "song": { ... } }`.

Song catalog responses should return `id`, `title`, `artist`, `baseBpm`,
`durationSec`, `previewStartMs`, `songVersion`, `contentSource`, `audioUrl`,
and `jacketUrl`.

Chart catalog responses should return `id`, `songId`, `keyCount`,
`difficultyName`, `chartAuthor`, `formatVersion`, `resolution`, `offset`,
`noteCount`, `minBpm`, `maxBpm`, `calculatedLevel`,
`difficultyRulesetId`, `difficultyRulesetVersion`, `chartSha256`,
`chartFingerprint`, and `contentSource`.

Server responses should use camelCase only. The client does not accept
snake_case aliases for catalog, detail, upload listing, or manifest metadata.

The authenticated "my uploads" responses should use the same song and chart
field names. Client-local IDs should stay in `clientSongId` and
`clientChartId`; server IDs should stay in `id` and `songId`.

Manifest responses are verification-only and should use camelCase field names:
`songJsonSha256`, `songJsonFingerprint`, `audioSha256`, `jacketSha256`,
`chartSha256`, and `chartFingerprint`.
