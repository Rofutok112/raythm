# Title Hub Feature Boundaries

Title Hub is the coordinator for song selection, friends, public profiles, room invite flow, and ranking panels. The scene owns feature controllers and applies their effects, but feature internals stay behind controller, service, reducer, and view boundaries.

## Shell

- `title_scene` owns the feature controllers and drives their frame lifecycle.
- `title_shell_effects` applies cross-feature effects such as notices, opening profiles, room joins, and friends reloads.
- Shell code should not own feature futures, parse API payloads, or implement row-level UI behavior.

## Friends

- `title_friends_view` draws the panel, computes hit regions, and returns view commands.
- `title_friends_controller` interprets view commands and service events, delegates state changes to `title_friends_reducer`, and emits typed effects.
- `title_friends_service` owns async friend/request/invite operations and social realtime polling.

## Public Profile

- `public_profile_view` draws profile UI and returns view commands.
- `public_profile_controller` keeps profile UI state, forwards network work to `public_profile_service`, and emits notice/reload effects.
- Relationship labels and enabled states stay in `public_profile_state`.

## Ranking

- `ranking_source_policy` owns source availability and fallback rules.
- Ranking views read availability and emit state actions; online route decisions stay out of view code.

When adding Title Hub behavior, prefer adding a typed command, reducer action, service event, or shell effect before reaching across feature controllers directly.
