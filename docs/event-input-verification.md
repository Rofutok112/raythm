# Event Input Verification

イベント入力化後の確認項目をまとめる。対象は `#56` 系の入力経路変更全体で、特に `#60` の手動確認観点として使う。

## 自動テスト

- `input_handler_smoke`
  4K/6K の press/release、native Windows イベントの順序、audio time 変換、同時押し変換、入力ソース表示用メタデータを確認する。
- `judge_system_smoke`
  tap、同時押し、ホールド解除、auto miss、native Windows 入力を audio time に写像した判定経路を確認する。

## 手動確認

### 1. 入力経路の確認

- プレイ画面の HUD に `INPUT native_windows (N)` が表示されること
- キーを押していないフレームでは `INPUT polling (0)` に戻ること
- 4K / 6K のどちらでも、割り当て済みキーで同じ表示が出ること

### 2. 同時押し

- 2 レーン以上を同時に押したとき、取りこぼしなく判定が出ること
- HUD の入力件数が 2 以上になるフレームがあること
- 4K と 6K の両方で確認すること

### 3. ホールド解除

- ホールドを終点前に離したとき miss になること
- 終点を超えてから離したとき、不要な miss が出ないこと

### 4. Auto Miss

- 入力しないノートが従来どおり miss になること
- イベント入力化後も、曲進行だけで miss 判定が進むこと

### 5. Audio Clock 基準のズレ確認

- 入力時だけ `native_windows` が出ることを確認したうえでプレイすること
- 体感で判定遅延が大きく悪化していないこと
- 同じ譜面で、4K / 6K のどちらでも極端な fast/slow 偏りが出ないこと

## 実施メモ

- native event が取れない環境でもプレイ不能にはならない。`polling` に落ちる。
- ただし timing 確認時は `native_windows` 表示が出ている状態で評価すること。
