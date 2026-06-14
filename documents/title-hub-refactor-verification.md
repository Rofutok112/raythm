# Title Hub Refactor Verification Notes

These notes capture the behavior surfaces preserved by the Title Hub refactor plan and the focused checks used while splitting responsibilities.

## Preserved Behavior Surfaces

- Friends panel tabs still expose friends, requests, and room invites.
- Friend requests still support accept and decline operations.
- Outgoing friend requests remain visible in the requests tab.
- Friends rows still support opening public profiles.
- Room invites still support join, mark-read, and decline operations.
- Accepted room invites still produce a room join request for the Title shell to apply.
- Social realtime invite events still update invite state and produce a notification effect.
- Public profile relationship actions still support send request, accept request, remove friend, block/unblock, and friends reload after success.
- Ranking source switching still supports local, all, and friends sources when available.
- Local-only charts still fall back to local ranking source.

## Current Verification

Built with the Codex build tree:

```powershell
cmake --build cmake-build-codex --target raythm title_friends_state_smoke friend_client_parser_smoke friend_client_refresh_smoke public_profile_state_smoke ranking_panel_view_smoke song_select_data_controller_smoke song_select_state_smoke multiplayer_state_smoke title_startup_controller_smoke -j 2
```

Focused smoke tests executed:

```powershell
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\title_friends_state_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\friend_client_parser_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\friend_client_refresh_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\public_profile_state_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\ranking_panel_view_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\song_select_data_controller_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\song_select_state_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\multiplayer_state_smoke.exe'
& 'C:\Users\rento\GitHub\raythm\cmake-build-codex\title_startup_controller_smoke.exe'
```

Diff hygiene:

```powershell
git diff --check
```

