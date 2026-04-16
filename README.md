raylibを使ったリズムゲーム

- [Content Storage And Song Catalog](C:/Users/rento/CLionProjects/raythm/documents/content-storage-and-song-catalog.md)
- [Auto Update Flow](C:/Users/rento/CLionProjects/raythm/documents/auto-update-flow.md)

## Discord Notifications

GitHub Issue / Pull Request の通知は [discord-notify.yml](/C:/Users/rento/GitHub/raythm/.github/workflows/discord-notify.yml) で Discord Webhook に送ります。

- `DISCORD_WEBHOOK_URL` を GitHub Actions secrets に登録すると通知が有効になります。
- Issue と PR はタイトル中心の短い Embed で通知します。
- Release と GitHub Pages build は本文やエラー内容を整形したうえで 280 文字に短縮して Embed に載せます。
- 通知対象は `Issue opened/reopened`、`PR opened/reopened/ready_for_review/closed`、`Release published`、`page_build` です。
