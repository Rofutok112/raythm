# Multiplayer Flow

This document pins down the multiplayer direction before the implementation spreads across UI, networking, gameplay, and server state.

## Goal

Multiplayer should be a first-class route from HOME, not a hidden extension of solo PLAY.

The target flow is:

1. Open `MULTIPLAY`.
2. Browse listed rooms.
3. Create a room with visibility and optional password.
4. In the room, the host selects song/chart and room settings.
5. Members confirm local ownership or download missing content.
6. Members ready up.
7. Host starts; server decides the start time.
8. Clients enter the same chart together.
9. Clients stream progress and submit result.
10. Room shows shared result and supports rematch, song change, leave, or close.

The interaction model is inspired by modern rhythm-game multiplayer lobbies: a room browser, a focused lobby, host-owned settings, visible readiness, and a single clear start action. Do not copy another product's visual assets, text, branding, or exact layout; adapt the pattern to raythm's existing Song Select spacing and panels.

## Screen Structure

Use the same outer spacing as the seamless Song Select screen, but do not place the browser, setup, and lobby on one crowded surface.

Shared bounds:

- Back button: `39, 983, 330, 58`
- Main multiplayer surface: `390, 109, 1488, 932`

The route is split into three screens:

1. Room Browser
2. Room Setup
3. Room Lobby

The back button returns to Room Browser from Setup/Lobby, then exits MULTIPLAY from Browser.

### Room Browser Screen

Purpose: server/room list and list-based join.

Required controls:

- Public room list
- Refresh
- Create room route
- Room rows should show:
  - room code or room name
  - public/private/locked state
  - room status
  - player count
- selected chart summary

Join behavior:

- Clicking an unlocked row joins immediately.
- Clicking a locked row opens a dedicated password screen.
- Room-code entry is not part of the normal client UI. Keep any code-based HTTP endpoint as compatibility/debug surface only.

### Password Prompt Screen

Purpose: focused join step for locked rooms.

Required controls:

- Selected room summary
- Password input
- Join room
- Back to room list

Future filters:

- All/Public/Friends
- Waiting/Playing/Result
- Has slot
- Official only
- Key count

### Room Setup Screen

Purpose: host settings and selected content.

Required controls:

- Create room
- Public/private toggle
- Password input
- Online song/chart picker
- Host-only room settings as the screen grows

Room settings target:

- visibility: `public | private`
- password: optional; never returned in plaintext
- selectedSongId
- selectedChartId
- keyCount
- autoStartDelayMs
- maxMembers
- allowLateJoin
- allowSpectators
- content policy: official only / any installed / community allowed

Current interim behavior:

- `CREATE ROOM` uses the song/chart selected inside `MULTIPLAY`.
- The song/chart picker lists public server charts in a compact two-column picker.
- Setup is a separate screen from the room list.
- Password validation is implemented in the HTTP room contract.
- A richer Song Select-equivalent browser with search, filters, download state, and jacket preview is still pending.

### Room Lobby Screen

Purpose: member readiness and match launch.

Required controls:

- Member list
- Selected song/chart area
- Ready/unready
- Start
- Leave
- Member rows should show:
  - display name
  - connected/disconnected
  - ready/waiting
  - current score/progress once playing
  - result once finished

Host-only actions:

- Start
- Change song/chart
- Change visibility/password/settings
- Close room

Non-host actions:

- Ready/unready
- Leave
- Download missing content

## Server Contract Direction

HTTP remains the initial/fallback contract:

- `POST /rooms`
- `GET /rooms`
- `GET /rooms/:roomId`
- `POST /rooms/join`
- `POST /rooms/:roomId/join`
- `POST /rooms/:roomId/leave`
- `PATCH /rooms/:roomId/settings`
- `POST /rooms/:roomId/ready`
- `POST /rooms/:roomId/start`
- `POST /rooms/:roomId/progress`
- `POST /rooms/:roomId/result`

WebSocket becomes the primary real-time contract:

- `room.state`
- `room.member.joined`
- `room.member.left`
- `room.member.disconnected`
- `room.ready.changed`
- `room.settings.changed`
- `room.countdown`
- `room.start`
- `room.progress`
- `room.result`
- `room.closed`

## Password Handling

Password support must be explicit server state, not client-only UI.

Server target:

- Store only a password hash for private locked rooms.
- `GET /rooms` returns `requiresPassword: true`, never the password.
- `POST /rooms/:roomId/join` accepts password when required.
- Compatibility room-code joins still require password if locked, but the normal UI should prefer list row selection.
- Host can set, replace, or clear password in settings.

## Song Selection In Room

The final design must not require users to visit solo PLAY before creating a room.
Multiplayer room setup must only offer songs/charts that exist on the server and are visible online.

Target:

- Reuse the Song Select browser shape inside the main multiplayer panel, backed by server content.
- Host selection updates room settings.
- Members see selected song/chart immediately.
- Members without content see a clear missing-content state.
- Download route should open Browse with selected song/chart, then return to the room.

Interim:

- Room creation and `APPLY SONG` use the same remote catalog entry point as Browse:
  `title_online_view::fetch_remote_catalog()`.
- MULTIPLAY does not read cached official/community status from `local_content.db`; TTL-prone status cache must not be revived for this route.
- `local_content_index` is used only as an ID binding table from local song/chart IDs to remote song/chart IDs.
- The client verifies those remote IDs against the server catalog, then keeps only local charts whose current runtime status is playable online (`official`, `community`, or `update`).
- Songs without an uploaded, locally playable chart do not appear in the setup picker.
- `CREATE ROOM` and `APPLY SONG` require the selected online song/chart to be downloaded locally for now.
- Room settings still store remote song/chart IDs; play launch maps them back to local IDs before opening the chart.
- The picker is intentionally simple right now; it should grow toward the full online browser feature set.

## Start And Play

Start rules:

- Only host can start.
- Server validates selected chart exists in room settings.
- Server validates all non-host members are ready.
- Server emits `startsAt`.
- Clients enter play scene for the selected local chart.
- If content is missing, client stays in lobby and shows missing content.

Current interim:

- HTTP refresh can discover `countdown`/`playing`.
- WebSocket start notification is not implemented yet.

## Implementation Order

1. Stabilize multiplayer screen layout to match Song Select bounds.
2. Add server password fields and join validation.
3. Add room-internal song/chart selection.
4. Add missing-content detection and Browse handoff.
5. Add WebSocket room state events.
6. Add progress streaming from play scene.
7. Add shared result view and rematch/song-change flow.
