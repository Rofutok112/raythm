# Friend Feature Requirements

## 1. Summary

raythm のフレンド機能 v1 は、既存のアカウント、公開プロフィール、ランキング、マルチプレイルームをつなぐ交流基盤として実装する。v1 の目的は、プレイヤー同士がフレンド関係を作り、フレンドのオンライン状態を確認し、ルームへ招待し、フレンド内ランキングで競えるようにすること。

DM や 1 対 1 の永続メッセージ履歴は v1 の対象外とする。メッセージ的な体験は、既存のルームチャットと部屋招待通知に限定する。

実装対象の主なリポジトリ境界:

- サーバー: `C:\Users\rento\CLionProjects\raythm-Server\server`
- クライアント: `C:\Users\rento\CLionProjects\raythm`
- サーバー実装は Fastify/Prisma/PostgreSQL の既存構成に合わせる。
- クライアント実装は既存の `auth_client`, `ranking_client`, `multiplayer_client` と同じネットワーク/refresh 方針に合わせる。

## 2. Scope

### v1 に含めるもの

- フレンド申請、承認、拒否
- フレンド一覧
- フレンド解除
- ブロック、ブロック解除
- プロフィール起点のフレンド追加導線
- フレンド向けオンライン状態表示
- フレンドへの部屋招待
- 未処理申請と未読招待の取得
- 既存オンラインランキングの「全体 / フレンド」切り替え

### v1 に含めないもの

- 1 対 1 DM
- 永続的な 1 対 1 通知履歴
- フレンドグループ、クラン、サークル
- タイムライン投稿
- 高度な検索、推薦、知り合い候補
- 観戦機能
- モバイルプッシュ通知
- ユーザー別の細かい presence 公開設定
- フレンドコード、ID 検索を主導線にした追加 UX

## 3. Product Rules

### フレンド追加

- フレンド追加の基本導線は公開プロフィールからの申請とする。
- 公開プロフィールは、ランキング行、部屋メンバー、投稿者表示など既存の導線から開ける。
- 名前検索やフレンドコードは v1 の主導線にしない。
- 自分自身には申請できない。
- 既にフレンド、申請中、相手から申請済み、ブロック中、相手にブロックされている場合は、プロフィール上の操作を状態に応じて切り替える。

### 公開範囲

- v1 のデフォルトは「フレンドには基本公開」とする。
- フレンドにはオンライン状態、プレイ中/ルーム参加中の概要、フレンドランキングを見せる。
- 細かい公開設定 UI は v1 では作らない。
- 拒否、ブロック、フレンド解除で関係を制御する。

### ブロック

- ブロックした相手からのフレンド申請、部屋招待、オンライン状態参照を遮断する。
- ブロックした相手との既存フレンド関係と未処理申請は無効化する。
- ブロック状態は相手に詳細表示しない。操作結果は汎用的な失敗メッセージにする。

### 部屋招待

- 招待できる対象は承認済みフレンドのみ。
- 招待元は、自分が現在参加している OPEN なルームに限る。
- パスワード付きルームでも招待は可能にする。招待から参加する場合はパスワード入力を不要にし、招待レコードを参加権として扱う。
- 招待は短期間で失効する。v1 の既定は 30 分。
- ルームが CLOSED になった場合、既存招待は参加不可として扱う。

### フレンド内ランキング

- 既存オンラインランキングの対象制約を引き継ぐ。現状の Official かつ public 譜面限定ルールは変えない。
- フレンドランキングは、自分と承認済みフレンドのランキング記録だけを対象にする。
- 未プレイのフレンドはランキング行には出さない。
- 自分の記録は、フレンドが 0 人でも表示対象に含める。
- 順位はフレンド内順位として再計算する。全体順位も表示したい場合は補助情報として別フィールドにする。

## 4. Server Requirements

### Data Model

サーバーは Prisma/PostgreSQL に以下の永続モデルを追加する。

- `Friendship`
  - `id`
  - `userAId`, `userBId`: ユーザーペアを辞書順で正規化した unique key 用
  - `requesterId`
  - `addresseeId`
  - `status`: `PENDING`, `ACCEPTED`, `DECLINED`, `BLOCKED`
  - `blockedById`: `BLOCKED` の場合のみ設定
  - `createdAt`, `updatedAt`, `respondedAt`
- `RoomInvite`
  - `id`
  - `roomId`
  - `senderId`
  - `recipientId`
  - `status`: `PENDING`, `ACCEPTED`, `DECLINED`, `EXPIRED`, `CANCELLED`
  - `readAt`
  - `expiresAt`
  - `createdAt`, `updatedAt`

制約:

- `Friendship` はユーザーペアを正規化した unique key を持ち、同じ 2 ユーザー間に複数の有効関係を作らない。
- `requesterId` と `addresseeId` は申請方向を保持する。`userAId` と `userBId` は pair uniqueness のためだけに使う。
- `requesterId` と `addresseeId` は同一ユーザーにできない。
- `userAId` と `userBId` も同一ユーザーにできない。
- `DECLINED` は監査と cooldown 判定のために保持する。再申請可能になった場合は同一 row を `PENDING` に戻すか、履歴テーブルを分けるかを migration 実装時に決める。
- 削除済みユーザーは一覧に出してよいが、表示名は既存の削除済みユーザー表現に従う。
- `RoomInvite` は部屋、送信者、受信者を参照する。
- ブロック中のユーザーペアには招待を作れない。
- `RoomInvite` は承認済み invite が join 権として使われたかを判定できるよう、`status` と `expiresAt` を必ず見る。

### HTTP API

すべて認証必須とする。

- `GET /friends`
  - 承認済みフレンド一覧、オンライン状態概要、未処理申請数、未読招待数を返す。
- `GET /friends/requests`
  - 自分宛と自分発信の未処理申請を返す。
- `POST /friends/requests`
  - `targetUserId` にフレンド申請を送る。
- `POST /friends/requests/:requestId/accept`
  - 自分宛申請を承認する。
- `POST /friends/requests/:requestId/decline`
  - 自分宛申請を拒否する。
- `DELETE /friends/:userId`
  - 承認済みフレンドを解除する。
- `POST /friends/:userId/block`
  - 対象ユーザーをブロックする。
- `DELETE /friends/:userId/block`
  - 対象ユーザーのブロックを解除する。
- `GET /users/:userId/profile`
  - 公開プロフィールと `relationshipStatus` を返す。自分宛の未処理申請がある場合は、プロフィール画面から承認できるよう `relationshipRequestId` も返す。
- `POST /rooms/:roomId/invites`
  - `recipientUserId` に部屋招待を送る。
- `GET /room-invites`
  - 自分宛の未読/未処理招待を返す。
- `POST /room-invites/:inviteId/accept`
  - 有効な招待を承認し、部屋参加に使える状態を返す。
- `POST /room-invites/:inviteId/decline`
  - 招待を拒否する。
- `POST /room-invites/:inviteId/read`
  - 招待を既読にする。
- `GET /charts/:chartId/rankings/friends`
  - 自分と承認済みフレンドだけのランキングを返す。

エラー方針:

- 未認証は既存認証 middleware と同じ `401` にする。
- 権限なし、ブロック、相手にブロックされている、関係がないなど、関係性の詳細を相手に漏らすべきでないケースは汎用的な `403` または `404` とする。
- cooldown/rate limit は既存 rate-limit plugin の応答形式に合わせる。
- invitation expired は `410` または既存ルーム API の失効表現に合わせ、クライアントが「招待の期限が切れました」を出せる message を返す。

### API Response Shape

フレンド系のユーザー表示は既存の公開ユーザー表現に合わせる。

- `id`
- `displayName`
- `avatarUrl`
- `relationshipStatus`
- `relationshipRequestId`, `pending_incoming` の場合のみ操作用に使用し、それ以外では `null` または空文字を許容
- `onlineStatus`
- `currentRoom`, ある場合のみ

招待は以下を返す。

- `id`
- `roomId`
- `roomName`
- `sender`
- `status`
- `read`
- `expiresAt`
- `createdAt`

### Realtime And Presence

- 既存の WebSocket 基盤を拡張し、ログイン中ユーザー単位の social realtime channel を追加する。
- 接続中ユーザーの presence はメモリに保持する。
- presence は最低限 `online`, `inRoom`, `inMatch`, `away` を扱う。
- フレンドの presence 更新は承認済みフレンドにだけ配信する。
- 部屋招待作成時、受信者がオンラインなら realtime event を送る。
- オフライン中の申請と招待は DB に残し、ログイン直後またはフレンド画面初回表示時に HTTP API で取得する。
- サーバー再起動で presence は失われてよい。復元対象は DB に残る申請、招待、フレンド関係だけとする。
- 複数接続があるユーザーは 1 つ以上の接続が残っている限り online とする。
- 部屋 WebSocket の接続状態から `inRoom`/`inMatch` を導出する場合も、配信は social channel の relationship filter を必ず通す。

### Rate Limit And Abuse Controls

- フレンド申請送信、招待送信、ブロック操作は rate limit 対象にする。
- 同じ相手への申請再送は、拒否後すぐにはできない。v1 の既定 cooldown は 24 時間。
- 同じルームから同じ相手への招待連打を制限する。v1 の既定 cooldown は 60 秒。
- ブロック関係がある場合、申請、招待、presence 参照、フレンドランキング上の関係表示を遮断する。

## 5. Client Requirements

### Network Layer

クライアントは `src/network/friend_client.*` を追加し、既存の `auth_client` と `multiplayer_client` と同じ方針で実装する。

- 保存済み session を使う。
- 401 の場合は `auth::restore_saved_session()` で refresh して再試行する。
- メンテナンス応答、未ログイン、通信失敗の結果型を明示する。
- JSON パース失敗時は汎用エラーにする。

### Service And State

Title Hub 配下に social/friends の状態と controller を置く。

- フレンド一覧
- 自分宛申請
- 自分発信申請
- 部屋招待
- 未読数
- presence snapshot
- 読み込み中、更新中、失敗、空状態

非同期処理は `request_xxx()` と `poll_xxx()` の形にし、Scene に future 管理を直接増やさない。

### UI

- Title Hub にフレンド入口を追加する。
- フレンド画面は v1 ではモーダルまたは右側パネルでよい。
- 表示タブは `Friends`, `Requests`, `Invites` を基本とする。
- 未ログイン時はログイン誘導を表示する。
- フレンド一覧から、プロフィール表示と現在ルームへの招待を実行できる。
- 公開プロフィールに relationship action を追加する。
- 申請中、承認済み、ブロック中などはボタン文言と disabled 状態で明示する。
- 未読申請/招待は Title Hub の入口に badge 表示する。
- 申請/招待の成功、失敗、期限切れ、ブロック相当の失敗は既存通知 UI に流し、画面全体を閉じない。
- フレンド UI は Title Hub の状態、controller、view の責務を分け、Scene に future や HTTP 詳細を増やさない。

### Multiplayer Integration

- ルーム画面からフレンド招待を開ける。
- フレンド招待 UI と送信処理は `OPEN` room でのみ有効にする。`IN_MATCH`, `CLOSED`, unknown status では送信しない。
- 招待通知を受けたら、通知または Invites タブから対象ルームへ参加できる。
- 招待通知は非インタラクティブな global notice として表示し、play/pause/menu の hit-test やキー入力を奪わない。
- 招待経由の参加では、パスワード付きルームの password dialog を出さない。
- 部屋メンバーのプロフィール導線は既存の `public_profile_controller` を使い、そこから申請できるようにする。

### Ranking Integration

- 既存ランキング表示に `All` と `Friends` の source 切り替えを追加する。
- `Friends` はオンラインランキングと同じくログイン必須にする。
- フレンドランキング取得中、未ログイン、対象外譜面、フレンド記録なしの空状態文言を用意する。
- ローカル専用またはオンラインランキング route を使えない譜面では、`Friends`/online source 切り替えを表示せず、ローカルランキングだけを表示する。

## 6. Acceptance Criteria

- 公開プロフィールからフレンド申請を送れる。
- 受信者は申請一覧で承認または拒否できる。
- 承認後、双方のフレンド一覧に表示される。
- フレンド解除後、双方のフレンド一覧から消える。
- ブロック後、相手は申請、招待、presence 参照ができない。
- フレンド一覧にオンライン/ルーム参加中/対戦中の状態が表示される。
- ルーム参加中にフレンドへ招待を送れる。
- オンラインの受信者には招待がリアルタイムに届く。
- オフラインの受信者は次回取得時に未読招待を見られる。
- 招待からパスワード付きルームへ参加できる。
- 既存ランキングで `Friends` を選ぶと、自分と承認済みフレンドだけの順位が表示される。
- DM/1 対 1 のメッセージ履歴 UI は表示されない。

## 7. Test Plan

### Server

- フレンド申請を作成できる。
- 自分自身への申請は失敗する。
- 重複申請は増えない。
- 受信者だけが承認/拒否できる。
- 承認済み関係を解除できる。
- ブロックすると既存申請とフレンド関係が無効化される。
- ブロック中は申請と招待が失敗する。
- 有効な部屋招待を作成、既読、承認、拒否できる。
- 期限切れ招待は参加に使えない。
- フレンドランキングは非フレンドの記録を含めない。
- rate limit/cooldown が効く。
- `npm run prisma:validate` が通る。
- `npm run typecheck` が通る。
- `npm run build` が通る。

### Client

- `friend_client` が成功応答、401 refresh、未ログイン、メンテナンス、JSON 不正を扱える。
- フレンド state がロード中、空、失敗、更新成功を表現できる。
- プロフィールの relationship action が状態別に切り替わる。
- 招待通知/一覧から multiplayer join flow に接続できる。
- multiplayer room invite UI/command は `OPEN` room でのみ有効である。
- ランキング panel が `All` と `Friends` を切り替えられる。

### Integration

- ランキング行のプロフィールから申請し、相手が承認し、フレンドランキングに反映される。
- 部屋メンバーのプロフィールから申請できる。
- フレンド一覧から部屋招待し、受信者が招待から参加できる。
- ブロック後、同じ相手から申請や招待が届かない。

### Deploy-Oriented Verification

- サーバーのローカル検証は `C:\Users\rento\CLionProjects\raythm-Server\server` で実行する。
- デプロイ前に最低限 `npm run prisma:validate`, `npm run typecheck`, `npm run build` を通す。
- 既存 smoke が関係する範囲で `npm run test:ranking-submit` を通し、ランキングの既存挙動を壊していないことを確認する。
- dev 環境へデプロイする場合は、リモート `/home/raythm/raythm-server` で `./deploy.sh dev` を実行する。
- デプロイ後は `docker compose ps` と health check で稼働確認する。ホストに API ポートを公開している構成では `curl -fsS http://localhost:3000/health` を使い、公開していない構成では `docker compose exec -T server` からコンテナ内 `http://127.0.0.1:3000/health` を確認する。
- 認証必須 API は未認証 `401` を期待値にした smoke を用意する。フレンド API 実装後は `/friends` と `/room-invites` の未認証 `401` を確認対象に加える。

## 8. Implementation Order

1. サーバー DB モデルと migration
2. サーバー friends API
3. サーバー room invite API
4. サーバー social realtime/presence
5. サーバー friends ranking API
6. クライアント `friend_client`
7. クライアント friends state/controller/service
8. Title Hub のフレンド入口と一覧 UI
9. 公開プロフィールの申請/関係アクション
10. マルチプレイ招待 UI と参加フロー
11. ランキング `All/Friends` 切り替え
12. テストと smoke verification
