# Friend Feature Task Breakdown

This checklist turns `friend-feature-requirements.md` into implementation-sized work. Code changes should keep server, network, feature state/controller, and view responsibilities separated.

Repositories:

- Server: `C:\Users\rento\GitHub\raythm-server\server`
- Client: `C:\Users\rento\GitHub\raythm`

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
  - Local server repo is `C:\Users\rento\GitHub\raythm-server\server`.
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

- [ ] Let multiplayer room UI open the friends invite flow.
- [ ] Send invites only while the user is in an OPEN room.
- [ ] Surface realtime invite notifications without disrupting gameplay.
- [ ] Route accepted invites into existing room join flow.
- [ ] Suppress password prompt when join is backed by a valid invite.
- [ ] Keep existing room chat unchanged; do not add DM UI.
- [ ] Ensure invites are unavailable outside an OPEN room.
- [ ] Ensure invite notifications do not steal input during gameplay.
- [ ] Phase 10 done when:
  - Friend invite to normal room works.
  - Friend invite to password-protected room works without password prompt.
  - Existing room chat and queue flows still work.

## Phase 11: Ranking Integration

- [ ] Extend ranking source UI with `All` and `Friends` for online rankings.
- [ ] Add friend ranking load request path.
- [ ] Preserve existing local ranking behavior.
- [ ] Show friend-specific empty/error states.
- [ ] Keep profile hit testing working for friend ranking entries.
- [ ] Keep global ranking as the default online source.
- [ ] Do not show `Friends` for local-only rankings.
- [ ] Phase 11 done when:
  - Switching `All`/`Friends` reloads the correct endpoint.
  - Friend ranking empty state distinguishes no friends from no friend records where the API can tell the difference.
  - Local ranking behavior is unchanged.

## Phase 12: Verification

- [ ] Server smoke tests:
  - request, accept, decline, remove, block, unblock
  - duplicate request prevention
  - invite create, read, accept, decline, expire
  - blocked users cannot request/invite/view presence
  - friend ranking excludes non-friends
- [ ] Client smoke tests:
  - `friend_client` parsing and 401 refresh
  - friends controller state transitions
  - profile relationship action state mapping
  - ranking source switching
- [ ] Manual integration checks:
  - profile to request to accepted friend
  - room invite to join
  - password-protected room invite join
  - friend ranking with self only and with friends
  - offline invite appears after reconnect
- [ ] Pre-deploy server verification:
  - `npm run prisma:validate`
  - `npm run typecheck`
  - `npm run build`
  - `npm run test:ranking-submit`
- [ ] Dev deploy verification when deploying:
  - SSH to `raythm@raythm-server` or `raythm@100.119.194.50`.
  - Run `cd /home/raythm/raythm-server && ./deploy.sh dev`.
  - Run `cd /home/raythm/raythm-server && docker compose ps`.
  - Run `curl -fsS http://localhost:3000/health`.
  - Probe unauthenticated protected friend endpoints and expect `401`.
- [ ] Phase 12 done when:
  - Required local server gates pass.
  - Client build/smoke gates pass.
  - If deployed, remote health and protected-route smoke results are recorded in the implementation notes.
