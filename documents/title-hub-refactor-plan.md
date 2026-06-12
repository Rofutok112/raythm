# Title Hub Refactor Plan

## Purpose

Title Hub is becoming the place where song select, account state, public profiles, friends, room invites, realtime events, downloads, and rankings meet. The goal of this refactor is to turn Title Hub from a large feature controller into a thin shell that coordinates smaller feature modules.

This is not a rewrite. Each phase must leave the app buildable and should preserve current behavior.

## Current Pain

- `title_friends_controller` owns too many responsibilities:
  - panel state
  - drawing helpers
  - hit testing
  - async request futures
  - realtime event polling
  - notifications
  - profile open requests
  - room join requests
- Related features communicate through controllers instead of explicit effect/result objects.
- View code and state mutation are close enough that adding UI actions tends to add more controller branching.
- Async work can drift into large controllers because there is no feature service boundary.
- Title Hub has no clear shell boundary, so new cross-feature flows naturally attach to whichever controller is already nearby.

## Target Architecture

Long term, Title should be organized conceptually like this:

```text
title/
  shell/
    title_shell_state
    title_shell_controller
    title_header_view
    title_layout

  friends/
    friends_state
    friends_reducer
    friends_commands
    friends_controller
    friends_service
    friends_view

  profile/
    public_profile_state
    public_profile_controller
    public_profile_service
    public_profile_view

  ranking/
    ranking_state
    ranking_controller
    ranking_service
    ranking_panel_view
    ranking_source_policy

  song_select/
    song_select_state
    song_select_controller
    song_select_data_controller
    song_select_view
```

The physical directory layout can migrate gradually. The first priority is responsibility boundaries, not moving every file immediately.

## Responsibility Rules

### Scene / Shell

Allowed:

- Own feature controllers.
- Call feature `tick`, `poll`, `handle_input`, and `draw`.
- Apply feature effects:
  - show notification
  - open profile
  - request room join
  - request reload
  - change scene

Not allowed:

- Parse API responses.
- Own feature-specific futures.
- Mutate friends/ranking/profile internals directly.
- Implement row-level UI behavior.

### View

Allowed:

- Draw from state.
- Compute layout and hit regions.
- Return view commands.

Not allowed:

- Call network clients.
- Mutate state.
- Call `ui::notify`.
- Start scene transitions.
- Own async or realtime clients.

### Controller

Allowed:

- Convert view commands and service events into actions.
- Call reducers.
- Return shell effects.
- Keep small feature-level coordination state.

Not allowed:

- Contain long drawing helpers.
- Contain raw HTTP details.
- Directly apply global side effects when a result object would work.

### Reducer / State Logic

Allowed:

- Mutate feature state from explicit actions.
- Be testable without raylib.
- Decide unread counts, selected tabs, empty states, operation states, and realtime state application.

Not allowed:

- Draw.
- Start async work.
- Call network APIs.
- Emit global notifications directly.

### Service

Allowed:

- Own async work, retries, refresh behavior, and realtime clients.
- Hide `std::future`, worker threads, and polling details behind feature events.
- Return typed service events.

Not allowed:

- Own UI selection state.
- Know layout or hit regions.
- Call scene transitions.

## Core Flow

The desired feature flow is:

```text
View
  -> view_command

Controller
  -> action
  -> reducer
  -> effect_request

Service
  -> service_event

Shell
  -> applies effect_request
```

For example:

```text
Friends panel JOIN click
  -> friends_view_command{ accept_invite, invite_id }
  -> friends_controller starts service operation
  -> friends_service_event{ invite_accept_succeeded, join_payload }
  -> friends_reducer updates invite state
  -> friends_controller_result{ room_join_request }
  -> title shell applies room join flow
```

## Shared Types To Introduce

These types should be added before large behavior moves.

```cpp
enum class friends_view_command_type {
    none,
    close,
    select_tab,
    open_profile,
    accept_request,
    decline_request,
    remove_friend,
    block_user,
    accept_invite,
    decline_invite,
    mark_invite_read,
};

struct friends_view_command {
    friends_view_command_type type = friends_view_command_type::none;
    std::string id;
};
```

```cpp
struct ui_notice_request {
    std::string message;
    ui::notice_tone tone = ui::notice_tone::info;
    float seconds = 2.0f;
};

struct feature_effects {
    std::vector<ui_notice_request> notices;
    std::optional<std::string> profile_user_id;
    std::optional<room_join_request> room_join;
    bool reload_friends = false;
};
```

Exact names can follow existing local style. The important part is that feature controllers return effects instead of applying global side effects directly.

## Phases

### Phase 0: Baseline And Guardrails

- [ ] Record current behavior surfaces:
  - Friends panel tabs.
  - Request accept/decline.
  - Outgoing request display.
  - Invite join/read/decline.
  - Public profile open from friends rows.
  - Room join request from invite accept.
  - Realtime invite notification.
- [ ] Keep existing smoke tests passing.
- [ ] Add or preserve focused state tests for friend state and profile action mapping.
- [ ] Confirm app target builds before structural moves.

Phase 0 done when:

- [ ] Current behavior is documented in this file or nearby verification notes.
- [ ] `raythm` builds.
- [ ] Existing friend/profile/ranking smoke tests pass.

### Phase 1: Extract Friends View Commands

- [ ] Add `title_friends_view_command` types.
- [ ] Move friends panel drawing helpers out of `title_friends_controller.cpp`.
- [ ] Create `title_friends_view.h/cpp`.
- [ ] Make the view return commands instead of mutating controller state directly.
- [ ] Keep layout and hit regions aligned inside the view.
- [ ] Keep controller responsible for interpreting commands.

Phase 1 done when:

- [ ] `title_friends_controller.cpp` no longer contains row-level draw helpers.
- [ ] Friends view has no network calls, futures, notifications, or scene transition code.
- [ ] Friends panel behavior is unchanged.
- [ ] `raythm` builds.
- [ ] `title_friends_state_smoke` and friend parser/refresh smoke tests pass.

### Phase 2: Introduce Friends Reducer

- [ ] Add `title_friends_reducer.h/cpp`.
- [ ] Move pure state transitions out of controller:
  - apply friend listing result
  - apply request listing result
  - apply invite listing result
  - apply operation success/failure state
  - apply social realtime event
  - update unread badge counts
- [ ] Keep reducer free of raylib dependencies.
- [ ] Expand smoke coverage for reducer behavior where current tests only cover partial state helpers.

Phase 2 done when:

- [ ] Controller delegates state mutation to reducer functions.
- [ ] Reducer can be tested without raylib.
- [ ] Existing `title_friends_state_smoke` either covers reducer behavior or is replaced by equivalent reducer tests.
- [ ] `raythm` builds.

### Phase 3: Return Effects Instead Of Applying Side Effects

- [ ] Add a feature effect/result type for friends controller.
- [ ] Replace direct `ui::notify` calls in friends controller with notice effects.
- [ ] Replace direct profile open storage with profile effects where practical.
- [ ] Replace direct room join request storage with room join effects where practical.
- [ ] Let Title shell apply these effects.

Phase 3 done when:

- [ ] Friends controller no longer directly calls `ui::notify`.
- [ ] Profile open and room join requests are represented as typed effects.
- [ ] Title shell is the only layer applying those cross-feature effects.
- [ ] Behavior remains unchanged.
- [ ] `raythm` builds and friend/profile smoke tests pass.

### Phase 4: Extract Friends Service

- [ ] Add `title_friends_service.h/cpp`.
- [ ] Move async operations out of controller:
  - reload friends
  - reload requests
  - reload invites
  - accept/decline request
  - remove/block user
  - accept/read/decline invite
  - social realtime connect/poll
- [ ] Service should emit typed events.
- [ ] Controller should poll service events and pass them to reducer.
- [ ] Keep auth refresh behavior inside `friend_client` or service, not view/controller branching.

Phase 4 done when:

- [ ] Friends controller has no raw `std::future` members.
- [ ] Friends controller does not start worker threads directly.
- [ ] Social realtime client is owned by service or a narrower realtime adapter.
- [ ] Service has no UI layout or selected-tab knowledge.
- [ ] `friend_client_refresh_smoke` still passes.
- [ ] `raythm` builds.

### Phase 5: Split Title Shell From Feature Logic

- [ ] Identify the current Title Hub orchestration file(s).
- [ ] Add shell-level result application helpers:
  - apply notices
  - open profile
  - start room join
  - request feature reloads
  - route cross-feature commands
- [ ] Ensure shell owns feature controllers but not their internal state transitions.
- [ ] Make friends/profile/ranking communication go through explicit effects, not direct controller calls.

Phase 5 done when:

- [ ] Title shell is visibly a coordinator.
- [ ] Feature controllers do not call each other directly.
- [ ] Cross-feature transitions are represented by typed effects or commands.
- [ ] Existing title startup and song select smoke tests pass.

### Phase 6: Apply The Pattern To Profile

- [ ] Extract `public_profile_view` if drawing and operation handling are still mixed.
- [ ] Move profile network calls into `public_profile_service` or an equivalent operation adapter.
- [ ] Keep `public_profile_state` as the source for action labels/enabled state.
- [ ] Return effects for:
  - friend request sent
  - request accepted
  - friend removed
  - block/unblock
  - reload friends
  - notices

Phase 6 done when:

- [ ] Profile action mapping remains pure and tested.
- [ ] Profile controller does not directly own avoidable async details.
- [ ] Relationship operations produce typed effects.
- [ ] `public_profile_state_smoke` passes.

### Phase 7: Apply The Pattern To Ranking

- [ ] Extract a `ranking_source_policy` for source availability and fallback.
- [ ] Keep ranking panel view command-based.
- [ ] Ensure `All`/`Friends` source changes are represented as state actions.
- [ ] Keep online/friend/local ranking load requests behind data/service controllers.

Phase 7 done when:

- [ ] Source availability is decided in one policy location.
- [ ] Ranking view does not know network route details.
- [ ] Local-only fallback remains tested.
- [ ] `ranking_panel_view_smoke` and `song_select_data_controller_smoke` pass.

### Phase 8: Directory And Naming Cleanup

- [ ] Move files into feature subdirectories if the codebase is ready for the churn.
- [ ] Keep include paths stable or update CMake in one pass.
- [ ] Avoid mixing mechanical moves with behavior changes.
- [ ] Add a short architecture note in the title directory if helpful.

Phase 8 done when:

- [ ] File layout reflects shell/friends/profile/ranking/song-select boundaries.
- [ ] Mechanical moves are isolated from behavior changes.
- [ ] Full target build passes.

## Global Completion Criteria

The refactor is complete when all of the following are true:

- [ ] Title Hub behaves as a shell/coordinator, not as the owner of feature internals.
- [ ] Friends panel rendering lives outside `title_friends_controller.cpp`.
- [ ] Friends view returns commands and does not mutate state directly.
- [ ] Friends state transitions are handled by reducer/state functions testable without raylib.
- [ ] Friends async work and realtime polling are owned by service/adapters, not by view or shell.
- [ ] Friends controller returns typed effects for notifications, profile open, room join, and reload requests.
- [ ] Profile relationship action mapping remains pure and tested.
- [ ] Ranking source availability/fallback is centralized.
- [ ] Scene/shell does not own feature-specific futures.
- [ ] No feature controller directly calls another feature controller for cross-feature flow.
- [ ] Existing behavior is preserved:
  - friends list load
  - request accept/decline
  - outgoing request display
  - room invite join/read/decline
  - realtime invite notification
  - public profile relationship actions
  - ranking `All`/`Friends` switching
  - local-only ranking fallback
- [ ] App builds with the normal Codex build target.
- [ ] Focused smoke tests pass:
  - `title_friends_state_smoke` or successor reducer smoke
  - `friend_client_parser_smoke`
  - `friend_client_refresh_smoke`
  - `public_profile_state_smoke`
  - `ranking_panel_view_smoke`
  - `song_select_data_controller_smoke`
  - `song_select_state_smoke`
  - `multiplayer_state_smoke`

## Non-Goals

- Do not rewrite Title Hub in one PR.
- Do not introduce a global ECS or event bus just for this.
- Do not move every title file before responsibility boundaries are clear.
- Do not change visual design as part of structural refactors unless needed to preserve behavior.
- Do not replace existing network clients while extracting services.
- Do not make service objects own UI state such as selected tab, hovered row, or scroll offset.

## Review Checklist

Use this checklist for each refactor PR:

- [ ] Does this reduce responsibilities in a controller?
- [ ] Does this keep behavior unchanged?
- [ ] Is raylib kept out of pure reducer/state logic?
- [ ] Are side effects returned as typed requests where practical?
- [ ] Are async details hidden from views and shell?
- [ ] Are new abstractions used by real code immediately?
- [ ] Are smoke tests updated for the new boundary?
- [ ] Is the diff small enough to review without mixing unrelated moves?
