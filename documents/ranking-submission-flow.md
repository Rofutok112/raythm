# Ranking Submission Flow

## 概要

現状のランキング送信は、`result_scene` でリザルト画面に入ったときに開始される。

流れは大きく次の 2 段階。

1. ローカルランキングへ保存
2. 条件を満たす場合のみオンラインランキングへ送信

オンラインランキングは現状 Official 譜面限定で、Community 譜面は対象外。

## クライアント側フロー

### 1. リザルト画面突入

- 起点: `src/scenes/result_scene.cpp`
- `result_scene::on_enter()` でランキング保存処理を開始する

### 2. ローカルランキング保存

- `ranking_service::submit_local_result_detailed()` を呼ぶ
- 保存先は `chart_id` ごとのローカルランキングファイル
- `result_data.note_results` がある場合は、その判定列を保存する
- ローカル順位は保存済み `note_results` から再計算される

### 3. オンライン送信判定

- `ranking_service::should_attempt_online_submit()` が true のときだけ続行する
- 実際のオンライン送信は `std::thread(...).detach()` で別スレッド実行される

## オンライン送信の前提条件

`ranking_service::submit_online_result()` に入る前後で、次の条件を確認している。

- `chart_id` がある
- 譜面が public
- ログイン済み
- メール認証済み
- Official 譜面である
- サーバーの scoring ruleset が active
- サーバーの `accepted_input` が `note_results_v1`

さらに Official 譜面では、送信前に manifest を取得してローカルファイルの hash を照合する。

照合対象:

- `song.json`
- audio
- jacket
- chart

ここで不一致ならオンライン送信は中断される。

## クライアントがサーバーへ送る payload

送信先:

- `POST /charts/:chartId/rankings`

送信 JSON:

```json
{
  "recorded_at": "2026-04-23T02:00:00Z",
  "ruleset_version": "2026-04-v1",
  "note_results": [
    {
      "event_index": 0,
      "result": "perfect",
      "offset_ms": 3.2
    }
  ]
}
```

送っているのは最終スコアではなく、`note_results` 本体と `recorded_at`、`ruleset_version`。

## サーバー側フロー

### 1. 受け口

- 起点: `server/src/rankings/routes.ts`
- `POST /charts/:chartId/rankings`

### 2. サーバー側前提チェック

- 認証必須
- 譜面が存在すること
- Official かつ public 譜面であること
- ユーザーのメール認証済みであること
- `rulesetVersion` が現在の active ruleset version と一致すること
- `noteResults` の形式が正しいこと

### 3. authoritative score 再計算

- サーバーは譜面ファイルを読み、総判定点数を数える
- 受け取った `noteResults` から `score / accuracy / clearRank / maxCombo / isFullCombo` を再計算する

つまり、クライアントが送ったスコア値は信用していない。
信頼しているのは `noteResults` と、サーバー側にある譜面ファイル。

### 4. DB 保存ルール

- 1 ユーザー 1 譜面につき 1 レコード
- 既存自己ベストより良い場合のみ更新

比較順:

1. `score`
2. `accuracy`
3. `recordedAt` が早い方
4. `submittedAt`
5. `id`

## 現状の trust boundary

現状の重要ポイントは次の通り。

- サーバーはスコアを authoritative に再計算している
- ただし Official 内容の hash 照合はクライアント側だけで完結している
- サーバーは manifest hash を使った再検証までは行っていない

そのため、今の実装は「クライアントが正しく実装されている前提」の部分がまだ残っている。
この点は `#254` と `#264` で改善予定。

## 既知の制約

- オンラインランキングは Official 譜面限定
- Community 譜面は送信されない
- オンライン送信は結果画面からバックグラウンド実行される
- 現状は `detach()` ベースなので、将来的には future 管理へ寄せたい

## 関連ファイル

クライアント:

- `src/scenes/result_scene.cpp`
- `src/gameplay/ranking_service.cpp`
- `src/network/ranking_client.cpp`

サーバー:

- `server/src/rankings/routes.ts`
- `server/src/rankings/shared.ts`
- `server/src/scoring/ruleset.ts`
- `server/src/scoring/routes.ts`
