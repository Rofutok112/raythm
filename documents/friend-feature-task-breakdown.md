# Friend Feature Task Breakdown

This checklist turns `friend-feature-requirements.md` into implementation-sized work. Code changes should keep server, network, feature state/controller, and view responsibilities separated.

Repositories:

- Server: `C:\Users\rento\CLionProjects\raythm-Server\server`
- Client: `C:\Users\rento\CLionProjects\raythm`

Phase rule: each phase should leave the app buildable. If a phase spans both repositories, finish and verify the server side first, then wire the client side behind tolerant empty/error states.

## Phase 0: Contracts

- [ ] Confirm `Friendship` pair normalization:
  - Store `requesterId` and `addresseeId` for request direction.
  - Also store `userAId` and `userBId`, sorted lexicographically, for pair uniqueness.
- [ ] Confirm declined relationship policy:
  - Keep `DECLINED` rows for cooldown/audit.
  - Reuse the same row for re-request after cooldown unless implementation proves a history table is needed.
- [ ] Confirm invite participation rule:
  - A valid `RoomInvite` lets the recipient join a password-protected room without typing the room password.
- [ ] Confirm presence lifetime:
  - Presence is memory-only.
  - Offline history is not stored.
  - Login/startup refresh gets durable requests and invites through HTTP.
- [ ] Confirm ranking source:
  - Friend rankings are online-only and inherit the existing Official/public chart restrictions.
- [ ] Confirm deploy target:
  - Local server repo is `C:\Users\rento\CLionProjects\raythm-Server\server`.
  - Remote dev deploy path is `/home/raythm/raythm-server`.
- [ ] Phase 0 done when:
  - The decisions above are reflected in `friend-feature-requirements.md`.
  - No implementation work depends on an unresolved product question.

## Phase 1: Server Data

- [ ] Add Prisma enums:
  - `FriendshipStatus`: `PENDING`, `ACCEPTED`, `DECLINED`, `BLOCKED`
  - `RoomInviteStatus`: `PENDING`, `ACCEPTED`, `DECLINED`, `EXPIRED`, `CANCELLED`
- [ ] Add `Friendship` model with relations to `User`.
- [ ] Add `RoomInvite` model with relations to `Room` and `User`.
- [ ] Add indexes for:
  - accepted friends by either user
  - pending requests by addressee
  - blocked pairs
  - unread pending invites by recipient
  - active invites by room and recipient
- [ ] Add migration and run Prisma validation/generation.
- [ ] Add server-side helper functions for:
  - normalized pair key
  - relationship lookup
  - accepted friend filtering
  - block filtering
- [ ] Phase 1 done when:
  - `npm run prisma:validate` passes.
  - `npm run prisma:generate` passes.
  - Migration SQL includes pair uniqueness and invite indexes.

## Phase 2: Server Friend API

- [ ] Create `server/src/friends/routes.ts`.
- [ ] Register routes under `/friends`.
- [ ] Implement `GET /friends`.
- [ ] Implement `GET /friends/requests`.
- [ ] Implement `POST /friends/requests`.
- [ ] Implement `POST /friends/requests/:requestId/accept`.
- [ ] Implement `POST /friends/requests/:requestId/decline`.
- [ ] Implement `DELETE /friends/:userId`.
- [ ] Implement `POST /friends/:userId/block`.
- [ ] Implement `DELETE /friends/:userId/block`.
- [ ] Apply rate limit policy to request and block operations.
- [ ] Return relationship state consistently for profile integration.
- [ ] Add focused route smoke tests or inject tests for:
  - create request
  - duplicate request
  - accept/decline authorization
  - remove friend
  - block/unblock
- [ ] Phase 2 done when:
  - Friend API tests pass.
  - Unauthenticated `/friends` returns `401`.
  - Relationship states match the profile response contract.

## Phase 3: Server Invite API

- [ ] Extend room join logic so a valid accepted invite can bypass password entry for the invite recipient.
- [ ] Implement `POST /rooms/:roomId/invites`.
- [ ] Implement `GET /room-invites`.
- [ ] Implement `POST /room-invites/:inviteId/accept`.
- [ ] Implement `POST /room-invites/:inviteId/decline`.
- [ ] Implement `POST /room-invites/:inviteId/read`.
- [ ] Expire stale invites at read/accept time.
- [ ] Reject invites when users are not accepted friends or either side has blocked the other.
- [ ] Add cooldown/rate-limit handling for repeated room invites to the same recipient.
- [ ] Add route smoke tests or inject tests for:
  - create invite
  - duplicate/cooldown invite
  - read invite
  - accept invite
  - decline invite
  - expired invite cannot join
  - password-protected room join succeeds with valid invite
- [ ] Phase 3 done when:
  - Invite API tests pass.
  - Unauthenticated `/room-invites` returns `401`.
  - Existing room join behavior still passes for non-invite joins.

## Phase 4: Server Presence And Realtime

- [ ] Add a social realtime service separate from room state.
- [ ] Track connected user IDs and their current activity:
  - `online`
  - `away`
  - `inRoom`
  - `inMatch`
- [ ] Add a user-level WebSocket route for social events.
- [ ] Broadcast presence changes only to accepted friends.
- [ ] Emit room invite events to online recipients.
- [ ] Keep durable request/invite fetch through HTTP as the source of truth after reconnect.
- [ ] Handle multiple connections for the same user without flickering offline until the final socket closes.
- [ ] Add reconnect behavior:
  - on connect, send current friend presence snapshot
  - on disconnect, broadcast offline/away only after connection count reaches zero
- [ ] Add tests or a manual websocket smoke for:
  - non-friend does not receive presence
  - accepted friend receives online/offline update
  - online invite recipient receives invite event
- [ ] Phase 4 done when:
  - Social realtime does not reuse room broadcast lists.
  - Presence filtering is covered by test or documented manual smoke evidence.

## Phase 5: Server Friend Ranking

- [ ] Implement `GET /charts/:chartId/rankings/friends`.
- [ ] Reuse existing ranking eligibility checks.
- [ ] Query chart rankings for current user plus accepted friends only.
- [ ] Recalculate placement inside the friend-scoped result.
- [ ] Exclude blocked users even if historical friendship rows exist.
- [ ] Return the same entry shape as existing online ranking where possible.
- [ ] Preserve existing global ranking endpoint behavior.
- [ ] Add tests for:
  - self only
  - accepted friend included
  - non-friend excluded
  - blocked user excluded
  - ineligible chart returns the same style of error as global online ranking
- [ ] Phase 5 done when:
  - Friend ranking tests pass.
  - Existing `npm run test:ranking-submit` still passes.

## Phase 6: Client Network Layer

- [ ] Add `src/network/friend_client.h`.
- [ ] Add `src/network/friend_client.cpp`.
- [ ] Mirror existing auth refresh behavior from `multiplayer_client`.
- [ ] Define result structs for:
  - friend list
  - friend requests
  - relationship operation
  - room invites
  - friend ranking listing
- [ ] Parse maintenance, unauthorized, HTTP failure, and malformed JSON into explicit result states.
- [ ] Add CMake targets/source entries.
- [ ] Add request methods for every v1 endpoint.
- [ ] Add DTO conversion that keeps API-only fields out of shared gameplay models.
- [ ] Phase 6 done when:
  - Client build includes `friend_client.*`.
  - Network tests or smoke cover 401 refresh and malformed JSON for at least friend list and invites.

## Phase 7: Client Friend Feature State

- [ ] Add a Title-scoped friends state type.
- [ ] Store friends, inbound requests, outbound requests, invites, unread counts, selected user, selected invite, and presence snapshots.
- [ ] Add loading, refreshing, failure, empty, and pending-operation fields.
- [ ] Add a friends controller with `request_xxx()` and `poll_xxx()` methods.
- [ ] Keep futures and stale-result handling inside the controller/service layer.
- [ ] Add command/result structs for:
  - load friends
  - respond to request
  - send request from profile
  - send invite from room
  - accept/decline/read invite
- [ ] Phase 7 done when:
  - Scene does not own friend futures directly.
  - Stale async results cannot overwrite newer friend state.

## Phase 8: Client UI

- [ ] Add a Title Hub friends entry point with unread badge.
- [ ] Add friends panel/modal view.
- [ ] Add `Friends`, `Requests`, and `Invites` tabs.
- [ ] Add empty states:
  - not logged in
  - no friends
  - no requests
  - no invites
  - load failed
- [ ] Add actions:
  - open profile
  - accept request
  - decline request
  - remove friend
  - block/unblock
  - invite current room
  - accept room invite
  - decline/read invite
- [ ] Add disabled/loading states for every mutating button.
- [ ] Add notification messages for success, cooldown/rate-limit, expired invite, and generic failure.
- [ ] Verify text fits at common desktop and narrow window sizes.
- [ ] Phase 8 done when:
  - The panel is usable while logged out, loading, empty, and populated.
  - UI commands are returned to the controller instead of doing network work from the view.

## Phase 9: Profile Integration

- [ ] Extend public profile response/client model with relationship status.
- [ ] Add relationship action to the public profile modal.
- [ ] Show state-specific action:
  - send request
  - request pending
  - accept incoming request
  - friend
  - blocked
  - unavailable
- [ ] Refresh friends/profile state after relationship operations.
- [ ] Preserve existing profile links/avatar behavior.
- [ ] Ensure ranking rows, room members, and existing profile entry points show the same relationship state.
- [ ] Phase 9 done when:
  - A profile opened from ranking can send a friend request.
  - A profile opened from room members can send a friend request.
  - Existing own-profile editing behavior is unchanged.

## Phase 10: Multiplayer Integration

- [x] Let multiplayer room UI open the friends invite flow.
- [x] Send invites only while the user is in an OPEN room.
- [x] Surface realtime invite notifications without disrupting gameplay.
- [x] Route accepted invites into existing room join flow.
- [x] Suppress password prompt when join is backed by a valid invite.
- [x] Keep existing room chat unchanged; do not add DM UI.
- [x] Ensure invites are unavailable outside an OPEN room.
- [x] Ensure invite notifications do not steal input during gameplay.
- [x] Phase 10 done when:
  - Friend invite to normal room works.
  - Friend invite to password-protected room works without password prompt.
  - Existing room chat and queue flows still work.

## Phase 11: Ranking Integration

- [x] Extend ranking source UI with `All` and `Friends` for online rankings.
- [x] Add friend ranking load request path.
- [x] Preserve existing local ranking behavior.
- [x] Show friend-specific empty/error states.
- [x] Keep profile hit testing working for friend ranking entries.
- [x] Keep global ranking as the default online source.
- [x] Do not show `Friends` for local-only rankings.
- [x] Phase 11 done when:
  - Switching `All`/`Friends` reloads the correct endpoint.
  - Friend ranking empty state distinguishes no friends from no friend records where the API can tell the difference.
  - Local ranking behavior is unchanged.

## Phase 12: Verification

- [x] Server smoke tests:
  - request, accept, decline, remove, block, unblock
  - duplicate request prevention
  - invite create, read, accept, decline, expire
  - blocked users cannot request/invite/view presence
  - friend ranking excludes non-friends
- [x] Client smoke tests:
  - `friend_client` parsing and 401 refresh
  - friends controller state transitions
  - profile relationship action state mapping
  - ranking source switching
- [x] Smoke-backed integration checks:
  - profile to request to accepted friend
  - room invite to join
  - password-protected room invite join
  - friend ranking with self only and with friends
  - offline invite appears after reconnect
- [x] Pre-deploy server verification:
  - `npm run prisma:validate`
  - `npm run typecheck`
  - `npm run build`
  - `npm run test:ranking-submit`
- [ ] Dev deploy verification when deploying:
  - SSH to `raythm@raythm-server` or `raythm@100.119.194.50`.
  - Run `cd /home/raythm/raythm-server && ./deploy.sh dev`.
  - Run `cd /home/raythm/raythm-server && docker compose ps`.
  - Run `curl -fsS http://localhost:3000/health` when the API port is published on the host.
  - If the API port is not published, run the same health and protected-route probes from inside the `server` container.
  - Probe unauthenticated protected friend endpoints and expect `401`.
- [x] Phase 12 done when:
  - Required local server gates pass.
  - Client build/smoke gates pass.
  - If deployed, remote health and protected-route smoke results are recorded in the implementation notes.

## Verification Notes

- 2026-06-12 server pre-deploy checks are expected to run from `C:\Users\rento\CLionProjects\raythm-Server\server`.
- Record local command results for `npm run prisma:validate`, `npm run typecheck`, `npm run build`, `npm run test:friend-feature`, `npm run test:social-realtime`, and `npm run test:ranking-submit`.
- Record remote dev checks only after an intentional deploy: `./deploy.sh dev`, `docker compose ps`, `curl -fsS http://localhost:3000/health`, and unauthenticated protected-route probes.

2026-06-12 final local verification coverage:

- Profile-to-request-to-accepted-friend path is covered by `npm run test:friend-feature`: create request, profile `pending_incoming` with `relationshipRequestId`, accept request, accepted friend listing.
- Duplicate friend request prevention is covered by `npm run test:friend-feature`: repeated pending request returns `409`.
- Offline invite recovery is covered by `npm run test:friend-feature`: invite is persisted and returned by `GET /room-invites`; stale pending invites are expired and omitted on fetch.
- Room invite join is covered by `npm run test:friend-feature`: accepted invite returns join data and `hasValidAcceptedRoomInvite` authorizes passwordless join access.
- Password-protected room invite join is covered by the room join implementation path: `POST /rooms/:roomId/join` accepts either a valid accepted invite or password verification before showing `Invalid room password`.
- Friend ranking coverage is split across server and client smoke tests: server asserts self plus accepted friend only, non-friends excluded, `No friends yet.`, and `No friend ranking records yet.`; client asserts `All`/`Friends` source selection and local-only fallback.
- Existing room chat and queue flows are not changed by the friend invite patch surface; the multiplayer smoke covers the OPEN-only invite guard, and the app target build covers the unchanged room scene integration.

2026-06-12 pre-deploy verification:

- `npm run prisma:validate`: pass.
- `npm run prisma:generate`: pass; required before typecheck because the local generated Prisma Client did not yet include the new friend models/enums.
- `npm run typecheck`: pass after `prisma:generate`.
- `npm run build`: pass.
- `npm run test:friend-feature`: pass.
- `npm run test:social-realtime`: pass.
- `npm run test:ranking-submit`: pass.
- Remote SSH via `raythm@raythm-server`: timeout because the hostname resolved to `192.168.11.33`.
- Remote SSH via `raythm@100.119.194.50`: pass.
- Remote `docker compose ps`: `raythm-server` and `raythm-postgres` are healthy.
- Remote host-side `curl http://localhost:3000/health`: failed because the server port is not published on the host.
- Remote container-side smoke: `/health` returned `200`; unauthenticated `/friends` and `/room-invites` returned `401`.
- Deploy command was not run in this pass.

2026-06-12 client implementation continuation:

- Split friend response and social realtime JSON parsing from `src/network/friend_client.cpp` into `src/network/friend_client_parser.*`.
- Added `friend_client_parser_smoke` to cover friend listing, request listing, invite accept/join payloads, realtime presence snapshots, realtime invite events, and malformed request payload handling.
- Fixed social realtime snapshot parsing so nested `friends[].userId` values are not also treated as a top-level `payload.userId` presence change.
- `cmake --build cmake-build-codex --target raythm ranking_service_smoke network_error_smoke title_startup_controller_smoke friend_client_parser_smoke -j 2`: pass.
- `friend_client_parser_smoke`: pass.
- `network_error_smoke`: pass.
- `ranking_service_smoke`: pass.
- `title_startup_controller_smoke`: pass.

2026-06-12 client inactive invite state continuation:

- `title_friends_state` now treats only empty, `pending`, and `accepted` invite statuses as visible/active.
- Realtime invite updates with `declined`, `expired`, or `cancelled` no longer insert unknown invites and remove matching visible invites.
- Expanded `title_friends_state_smoke` for inactive unknown invite ignoring, inactive existing invite removal, and active/inactive status classification.
- `cmake --build cmake-build-codex --target raythm ranking_service_smoke network_error_smoke title_startup_controller_smoke friend_client_parser_smoke title_friends_state_smoke -j 2`: pass.
- `title_friends_state_smoke`: pass.
- `friend_client_parser_smoke`: pass.
- `ranking_service_smoke`: pass.
- `title_startup_controller_smoke`: pass.

2026-06-12 realtime invite cancellation continuation:

- `cancelActiveRoomInvitesBetween` now returns the cancelled invite IDs/recipients so routes can notify affected online recipients.
- `DELETE /friends/:userId` and `POST /friends/:userId/block` now emit `social.room_invite_updated` events with minimal `{ id, status: "cancelled" }` invite payloads for cancelled active invites.
- Expanded `friend_client_parser_smoke` to verify the client parses `social.room_invite_updated` cancelled-invite payloads.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 unread invite badge expiration continuation:

- `GET /friends` now counts unread invites only when they are pending, unread, and not expired.
- Expanded `npm run test:friend-feature` to assert `unreadInviteCount` excludes stale pending invites while still counting active unread invites.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 room invite listing expiration continuation:

- `GET /room-invites` now persists stale pending invites as `EXPIRED` during fetch but omits them from the returned active invite list.
- Expanded `npm run test:friend-feature` to include a stale pending invite and assert it is expired in storage but not returned by the listing.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.
- Client `cmake --build cmake-build-codex --target raythm friend_client_parser_smoke title_friends_state_smoke -j 2`: pass.
- Client `friend_client_parser_smoke`: pass.
- Client `title_friends_state_smoke`: pass.

2026-06-12 realtime cancellation smoke coverage continuation:

- Expanded `npm run test:friend-feature` to connect a fake social realtime socket for the invite recipient.
- The smoke now asserts unfriend cancellation sends `social.room_invite_updated` with `{ invite: { id, status: "cancelled" } }`.
- The smoke also reactivates the invite in-memory before block and asserts block cancellation sends the same realtime event.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 server block/invite continuation:

- Added block-aware invite cleanup: blocking a user cancels active pending/accepted room invites between the two users.
- Added invite accept/join guards so blocked pairs or no-longer-friends cannot keep using a previously accepted invite as passwordless room access.
- Expanded `npm run test:friend-feature` coverage for block cancelling accepted invite access and blocked users being unable to send a new friend request.
- `npm run typecheck`: pass.
- `npm run test:friend-feature`: pass.
- `npm run test:social-realtime`: pass.
- `npm run test:ranking-submit`: pass.
- `npm run build`: pass.

2026-06-12 server unfriend invite cancellation continuation:

- `DELETE /friends/:userId` now proactively cancels active pending/accepted room invites between the two users before deleting the accepted friendship.
- Expanded `npm run test:friend-feature` coverage to assert the invite row becomes `CANCELLED` immediately after unfriend, in addition to losing passwordless join access.
- `npm run typecheck`: pass.
- `npm run test:friend-feature`: pass.
- `npm run test:social-realtime`: pass.
- `npm run test:ranking-submit`: pass.
- `npm run build`: pass.

2026-06-12 client friends state continuation:

- Split realtime social state updates from `title_friends_controller` into `src/scenes/title/title_friends_state.*`.
- Added `title_friends_state_smoke` for unread badge count, friend presence updates, non-friend presence ignoring, new invite insertion, duplicate invite refresh, and no duplicate unread-count increment.
- Controller now delegates social realtime state mutation to the state helper and only shows notifications for newly inserted invites/errors.
- `cmake --build cmake-build-codex --target raythm ranking_service_smoke network_error_smoke title_startup_controller_smoke friend_client_parser_smoke title_friends_state_smoke -j 2`: pass.
- `title_friends_state_smoke`: pass.
- `friend_client_parser_smoke`: pass.
- `network_error_smoke`: pass.
- `ranking_service_smoke`: pass.
- `title_startup_controller_smoke`: pass.

2026-06-12 server invite join guard continuation:

- Strengthened `hasValidAcceptedRoomInvite` so accepted room invites stop authorizing passwordless room join after the sender/recipient are no longer accepted friends.
- Expanded `npm run test:friend-feature` coverage for removing a friend after accepting an invite and verifying the invite can no longer authorize join access.
- `npm run typecheck`: pass.
- `npm run test:friend-feature`: pass.
- `npm run test:social-realtime`: pass.
- `npm run test:ranking-submit`: pass.
- `npm run build`: pass.

2026-06-12 public profile incoming request continuation:

- `GET /users/:userId/profile` now returns `relationshipRequestId` when the viewer has an actionable incoming pending friend request from that profile user.
- Public profile relationship action now shows `ACCEPT` for actionable `pending_incoming` relationships and calls `accept_friend_request` with the returned request id.
- Expanded `npm run test:friend-feature` to register the users profile routes and assert incoming profile responses expose both `pending_incoming` and the request id.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.
- Client `cmake --build cmake-build-codex --target raythm friend_client_parser_smoke title_friends_state_smoke -j 2`: pass.
- Client `friend_client_parser_smoke`: pass.
- Client `title_friends_state_smoke`: pass.

2026-06-12 outgoing requests UI continuation:

- Requests tab now displays both incoming and outgoing friend requests with section labels.
- Incoming request rows now include a profile button alongside accept/decline.
- Outgoing request rows show `PENDING` and provide a profile button for the addressee, so sent requests are not hidden from the social panel.
- Client `cmake --build cmake-build-codex --target raythm title_friends_state_smoke friend_client_parser_smoke -j 2`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.

2026-06-12 invite read UI continuation:

- Invites tab now exposes a third action for unread invites: `READ`, alongside `JOIN` and `DECLINE`.
- Read invites show as `SEEN` and no longer accept read clicks from the panel.
- Expanded `npm run test:friend-feature` to call `POST /room-invites/:inviteId/read` and assert `GET /friends` no longer counts that invite as unread.
- Client `cmake --build cmake-build-codex --target raythm title_friends_state_smoke friend_client_parser_smoke -j 2`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 unblock/profile relationship smoke continuation:

- Expanded `npm run test:friend-feature` to cover `DELETE /friends/:userId/block`.
- The smoke now verifies profile relationship states after block: blocker sees `blocked`, blocked user sees `unavailable`.
- The smoke then unblocks, verifies the blocker profile returns to `none`, and verifies the previously blocked user can send a new friend request.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.
- Client `cmake --build cmake-build-codex --target raythm title_friends_state_smoke friend_client_parser_smoke -j 2`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.

2026-06-12 public profile action state continuation:

- Split public profile relationship action mapping into `src/scenes/shared/public_profile_state.*` so labels, enabled states, and operation types are testable without raylib UI.
- Public profile controller now uses the shared action mapping for draw, click enablement, and relationship operation dispatch.
- Added `public_profile_state_smoke` for `none`, `pending_incoming` with and without request id, `pending_outgoing`, `accepted`, `blocked`, `unavailable`, `self`, and active-operation states.
- Client `cmake -S . -B cmake-build-codex -G "MinGW Makefiles" -DCMAKE_C_COMPILER=C:/Users/rento/Documents/w64devkit/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/Users/rento/Documents/w64devkit/bin/g++.exe`: pass.
- Client `cmake --build cmake-build-codex --target raythm public_profile_state_smoke title_friends_state_smoke friend_client_parser_smoke -j 2`: pass.
- Client `public_profile_state_smoke`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 public profile unavailable label continuation:

- Public profile relationship action mapping now labels `unavailable` explicitly as `UNAVAILABLE` instead of generic `N/A`.
- Updated `public_profile_state_smoke` to lock the `unavailable` label and disabled state.
- Client `cmake --build cmake-build-codex --target raythm public_profile_state_smoke title_friends_state_smoke friend_client_parser_smoke -j 2`: pass.
- Client `public_profile_state_smoke`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 friends ranking source switching continuation:

- Expanded `song_select_data_controller_smoke` so `selection_key_for_state` preserves `ranking_service::source::friends` for online-route-capable charts.
- The same smoke now verifies `friends` source falls back to `local` for local-only or legacy remote-link-only charts, matching online source safety behavior.
- Client `cmake --build cmake-build-codex --target raythm song_select_data_controller_smoke public_profile_state_smoke title_friends_state_smoke friend_client_parser_smoke -j 2`: pass.
- Client `song_select_data_controller_smoke`: pass.
- Client `public_profile_state_smoke`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.

2026-06-12 multiplayer invite OPEN-room guard continuation:

- Added `multiplayer::can_invite_friends` and `invite_friends_unavailable_message` so room invite availability is testable outside the raylib UI.
- Multiplayer room UI now shows the invite button as disabled outside `OPEN` rooms.
- Multiplayer command handling now rejects both opening the invite modal and sending a room invite outside `OPEN` rooms.
- Added `multiplayer_state_smoke` for `OPEN`, `IN_MATCH`, `CLOSED`, and unknown room statuses.
- Client `cmake --build cmake-build-codex --target raythm multiplayer_state_smoke friend_client_parser_smoke title_friends_state_smoke public_profile_state_smoke song_select_data_controller_smoke -j 2`: pass.
- Client `multiplayer_state_smoke`: pass.
- Client `friend_client_parser_smoke`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `public_profile_state_smoke`: pass.
- Client `song_select_data_controller_smoke`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.
- Remote dev `docker compose ps`: `raythm-server` and `raythm-postgres` healthy.
- Remote container-side smoke: `/health` returned `200`; unauthenticated `/friends` and `/room-invites` returned `401`.

2026-06-12 friend ranking source UI continuation:

- Ranking source UI now receives whether the selected chart can use online chart routes.
- `FRIENDS` and `GLOBAL` sources are hidden from the ranking panel and ignored by hit-testing for local-only / legacy remote-link-only charts.
- Added `ranking_panel_view_smoke` for local-only vs online-source availability.
- Client `cmake --build cmake-build-codex --target raythm ranking_panel_view_smoke song_select_data_controller_smoke multiplayer_state_smoke -j 2`: pass.
- Client `ranking_panel_view_smoke`: pass.
- Client `song_select_data_controller_smoke`: pass.
- Client `multiplayer_state_smoke`: pass.

2026-06-12 non-blocking notice smoke continuation:

- Extended `song_select_state_smoke` to assert queued global notices do not register UI hit regions.
- This locks the shared notice behavior used by realtime room-invite notifications: notices can surface while gameplay input remains owned by the active scene.
- Client `cmake --build cmake-build-codex --target song_select_state_smoke raythm -j 2`: pass.
- Client `song_select_state_smoke`: pass.

2026-06-12 friend client refresh and verification continuation:

- Added `friend_client_refresh_smoke`, which starts a local loopback HTTP server and verifies `friend_client::fetch_friends()` handles `401` by calling `/me`, refreshing through `/auth/refresh`, and retrying `/friends` with the refreshed access token.
- Verified no DM/direct/private message implementation exists outside the friend feature requirement documents.
- Client `cmake --build cmake-build-codex --target raythm friend_client_parser_smoke friend_client_refresh_smoke title_friends_state_smoke public_profile_state_smoke ranking_panel_view_smoke song_select_data_controller_smoke song_select_state_smoke multiplayer_state_smoke -j 2`: pass.
- Client `friend_client_parser_smoke`: pass.
- Client `friend_client_refresh_smoke`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `public_profile_state_smoke`: pass.
- Client `ranking_panel_view_smoke`: pass.
- Client `song_select_data_controller_smoke`: pass.
- Client `song_select_state_smoke`: pass.
- Client `multiplayer_state_smoke`: pass.
- Server `npm run prisma:validate`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.
- Remote dev `docker compose ps`: `raythm-server` and `raythm-postgres` healthy.
- Remote container-side smoke: `/health` returned `200`; unauthenticated `/friends` and `/room-invites` returned `401`.
- Deploy command was not run in this pass because the server repository still has uncommitted local changes and ops notes require commit/push before deployment.

2026-06-12 final Phase 10-12 closeout verification:

- Expanded `npm run test:friend-feature` to reject duplicate pending friend requests with `409`.
- Expanded `npm run test:friend-feature` to distinguish friend ranking empty messages: `No friend ranking records yet.` when the user has accepted friends but no chart records, and `No friends yet.` when the user has no accepted friends.
- Server `npm run prisma:validate`: pass.
- Server `npm run typecheck`: pass.
- Server `npm run test:friend-feature`: pass.
- Server `npm run test:social-realtime`: pass.
- Server `npm run test:ranking-submit`: pass.
- Server `npm run build`: pass.
- Client `cmake --build cmake-build-codex --target raythm friend_client_parser_smoke friend_client_refresh_smoke title_friends_state_smoke public_profile_state_smoke ranking_panel_view_smoke song_select_data_controller_smoke song_select_state_smoke multiplayer_state_smoke -j 2`: pass.
- Client `friend_client_parser_smoke`: pass.
- Client `friend_client_refresh_smoke`: pass.
- Client `title_friends_state_smoke`: pass.
- Client `public_profile_state_smoke`: pass.
- Client `ranking_panel_view_smoke`: pass.
- Client `song_select_data_controller_smoke`: pass.
- Client `song_select_state_smoke`: pass.
- Client `multiplayer_state_smoke`: pass.
- Remote dev `docker compose ps`: `raythm-server` and `raythm-postgres` healthy.
- Remote container-side Node fetch: `/health` returned `200`; unauthenticated `/friends` and `/room-invites` returned `401`.
- Deploy command was not run because server changes remain uncommitted locally and the ops workflow requires commit/push before deployment.
