# chart_ml

`raythm` repo 内で機械学習用のデータ抽出と前処理を行うための Python ワークスペースです。

現時点では以下を提供します。

- `song.json` / `.rchart` の読み込み
- 外部 4K 譜面データから `raythm` 用 song package への変換
- 4キー譜面の抽出
- 譜面単位の JSONL データセット生成
- `4 event x 4 lane` のウィンドウデータ生成
- データセット統計の確認
- 再現可能な corpus manifest / fixed split / 品質レポート生成
- 簡易な遷移学習モデルの学習と譜面生成
- PyTorch ベースのニューラル譜面モデルの学習と譜面生成
- 音源からの timing 推定（固定 / 可変 BPM）
- `legacy / full` feature profile の比較

## セットアップ

```bash
cd tools/chart_ml
python -m venv .venv
.venv\Scripts\activate
pip install -e .
```

学習ライブラリも入れる場合:

```bash
pip install -e .[train]
```

## CLI

ヘルプ:

```bash
python -m chart_ml --help
```

### 1. 譜面単位の JSONL を作る

```bash
python -m chart_ml extract-charts ^
  --songs-dir C:\path\to\songs ^
  --output C:\path\to\charts.jsonl ^
  --key-count 4
```

1 行につき 1 譜面のレコードを出力します。

固定 split を使いたい場合は `--manifest` と `--split` を追加します。

```bash
python -m chart_ml extract-charts ^
  --songs-dir C:\path\to\songs ^
  --manifest C:\path\to\corpus_manifest.json ^
  --split train ^
  --output C:\path\to\train_charts.jsonl
```

### 2. 4 event x 4 lane の学習窓を作る

```bash
python -m chart_ml extract-windows ^
  --songs-dir C:\path\to\songs ^
  --output C:\path\to\windows.jsonl ^
  --key-count 4 ^
  --span 4
```

各レコードには以下が入ります。

- 譜面メタデータ
- 現在イベント位置
- 直近 `span` 個のイベント tick
- 各イベント時点の 4 レーン状態

レーン状態は以下の文字列です。

- `off`
- `tap`
- `hold_start`
- `holding`
- `hold_end`

### 3. 統計を出す

```bash
python -m chart_ml stats ^
  --songs-dir C:\path\to\songs ^
  --key-count 4
```

### 3.5. corpus manifest と fixed split を作る

```bash
python -m chart_ml build-corpus ^
  --songs-dir C:\path\to\raythm_songs ^
  --output-manifest C:\path\to\corpus_manifest.json ^
  --output-split-dir C:\path\to\splits ^
  --output-report C:\path\to\corpus_report.md
```

このコマンドでは:

- 4K / LN 比率 / ノート密度 / 長さなどの品質メトリクスを集計
- 同一レーン overlap や壊れた hold を検出
- exact duplicate chart を検出
- song family / chart family を作って近縁譜面の偏りを追跡
- `song_id` ベースの安定ハッシュで `train / validation / test` を固定分割
- manifest JSON と `train.jsonl`, `validation.jsonl`, `test.jsonl`、人間向け report を出力

必要なら品質ゲートも入れられます。

```bash
python -m chart_ml build-corpus ^
  --songs-dir C:\path\to\raythm_songs ^
  --output-manifest C:\path\to\corpus_manifest.json ^
  --min-note-count 100 ^
  --min-ln-ratio 0.05 ^
  --max-notes-per-second 12.0 ^
  --max-charts-per-song-family 3
```

`--max-charts-per-song-family` や `--max-charts-per-chart-family` を使うと、同一曲の差分譜面や近縁譜面が学習を支配しすぎるのを抑えられます。

### 4. 外部 4K データを raythm 形式へ変換する

取り込み用コマンドでは、外部の 4レーン鍵盤譜面データを `raythm` 用パッケージへ変換できます。

- 展開済みの譜面データ
- アーカイブ形式の譜面データ

のどちらにも対応しています。

各セットから 4レーン譜面だけを拾い、

- `song.json`
- `audio.*`
- `jacket.*`
- `charts/*.rchart`

を持つ `raythm` 用パッケージへ変換します。

```bash
python -m chart_ml import-external-4k ^
  --source-root C:\path\to\external_chart_data ^
  --output-songs-dir C:\path\to\raythm_songs
```

### 5. 簡易モデルを学習する

```bash
python -m chart_ml train-simple ^
  --songs-dir C:\path\to\raythm_songs ^
  --output-model C:\path\to\simple_model.json
```

これは固定グリッド上で

- 初期パターン
- パターン遷移
- 拍位置ごとの出現パターン

を数え上げるシンプルなモデルです。

manifest/split を使う場合:

```bash
python -m chart_ml train-simple ^
  --songs-dir C:\path\to\raythm_songs ^
  --manifest C:\path\to\corpus_manifest.json ^
  --split train ^
  --output-model C:\path\to\simple_model.json
```

### 6. 学習済みモデルから譜面を1本出す

```bash
python -m chart_ml generate-simple ^
  --model C:\path\to\simple_model.json ^
  --template-chart C:\path\to\template.rchart ^
  --output C:\path\to\generated.rchart ^
  --song-id demo_song ^
  --chart-id demo_ml ^
  --difficulty ML_SIMPLE ^
  --seed 42
```

この段階では timing は生成しません。

- `template.rchart` の timing event をそのまま使う
- `template.rchart` の event tick 列をそのまま使う
- その tick 上で lane state を学習モデルからサンプリングする

つまり今の段階では「音楽理解」まではしておらず、

- テンプレート譜面の timing / event tick
- 学習したパターン遷移
- hold を含む状態列

だけで譜面を構成します。

### 6.5. ニューラルモデルを学習する

学習前に前処理キャッシュを作っておくと、2回目以降の学習がかなり速くなります。

```bash
python -m chart_ml prepare-neural-cache ^
  --songs-dir C:\path\to\raythm_songs ^
  --cache-dir C:\path\to\neural_cache ^
  --feature-profile full ^
  --subdivision 8
```

このキャッシュには、

- 音源から抽出した特徴量
  - `legacy`: mono mel + overall onset
  - `full`: `left/right/mid/side` mel + `chroma` + `low/mid/high` onset
- beat subdivision ごとに整列した特徴列
- 4 レーンの譜面状態ラベル列

が `chart` ごとに保存されます。

固定 split を使う場合:

```bash
python -m chart_ml prepare-neural-cache ^
  --songs-dir C:\path\to\raythm_songs ^
  --cache-dir C:\path\to\neural_cache ^
  --feature-profile full ^
  --manifest C:\path\to\corpus_manifest.json ^
  --split train ^
  --subdivision 8
```

```bash
python -m chart_ml train-neural ^
  --songs-dir C:\path\to\raythm_songs ^
  --output-model C:\path\to\neural_model.pt ^
  --cache-dir C:\path\to\neural_cache ^
  --feature-profile full ^
  --subdivision 8 ^
  --epochs 6 ^
  --batch-size 4 ^
  --device cpu
```

この学習では

- 音源特徴を抽出
  - `legacy`: mono mel + onset
  - `full`: stereo mel + chroma + band onset
- 譜面を beat subdivision ごとの `4 lane x action` ラベルに変換
- BiGRU ベースのモデルで時系列分類

を行います。

軽い ablation をしたい場合は、同じデータセットで `legacy` と `full` を比べられます。

```bash
python -m chart_ml prepare-neural-cache ^
  --songs-dir C:\path\to\raythm_songs ^
  --cache-dir C:\path\to\neural_cache_legacy ^
  --feature-profile legacy

python -m chart_ml train-neural ^
  --songs-dir C:\path\to\raythm_songs ^
  --cache-dir C:\path\to\neural_cache_legacy ^
  --feature-profile legacy ^
  --output-model C:\path\to\legacy_model.pt
```

manifest/split を使う場合:

```bash
python -m chart_ml train-neural ^
  --songs-dir C:\path\to\raythm_songs ^
  --manifest C:\path\to\corpus_manifest.json ^
  --split train ^
  --output-model C:\path\to\neural_model.pt ^
  --cache-dir C:\path\to\neural_cache
```

1 譜面だけで overfit テストしたいときは、例えばこうします。

```bash
python -m chart_ml prepare-neural-cache ^
  --songs-dir C:\path\to\official_songs ^
  --cache-dir C:\path\to\neural_cache ^
  --chart-id ColorArRay_Hard

python -m chart_ml train-neural ^
  --songs-dir C:\path\to\official_songs ^
  --output-model C:\path\to\overfit_model.pt ^
  --cache-dir C:\path\to\neural_cache ^
  --chart-id ColorArRay_Hard ^
  --validation-ratio 0 ^
  --epochs 20 ^
  --batch-size 1
```

### 7. 音源の timing 候補を確認する

```bash
python -m chart_ml analyze-audio ^
  --audio C:\path\to\audio.mp3 ^
  --level 6.5 ^
  --timing-mode adaptive ^
  --feature-profile full
```

このコマンドでは

- 推定 BPM
- 曲の有効区間
- tempo segment 数
- onset 数
- beat 数
- 小節開始数
- 生成される event tick 数
- 使用している feature profile
- mel / chroma / band onset の特徴構成

を確認できます。

既存譜面にどれだけ近い timing かを測りたい場合は、評価コマンドも使えます。

```bash
python -m chart_ml evaluate-timing ^
  --audio C:\path\to\audio.mp3 ^
  --chart C:\path\to\reference.rchart ^
  --level 6.5 ^
  --timing-mode adaptive
```

ここでは

- note 開始位置が生成 event 候補へどれだけ近いか
- chart の beat grid と生成 beat grid のズレ
- 30ms / 60ms / 90ms 以内の一致率
- 曲の有効区間

を JSON で確認できます。

### 8. 音源から `.rchart` を1本出す

```bash
python -m chart_ml generate-audio ^
  --model C:\path\to\simple_model.json ^
  --audio C:\path\to\audio.mp3 ^
  --output C:\path\to\generated.rchart ^
  --song-id demo_song ^
  --chart-id demo_audio ^
  --difficulty ML_AUDIO ^
  --level 6.5 ^
  --timing-mode adaptive ^
  --seed 42
```

このコマンドでは

- 対象音源を `librosa` で解析
- timing map を生成
- 各 tick の音の強さで配置密度を補正
- 最終的に `.rchart` を出力

まで行います。

ゲームへそのまま入れたい場合は、追加で `--output-song-dir` を付けると

- `song.json`
- 元音源のコピー
- 任意の jacket
- `charts/{chart_id}.rchart`

を含む song package も作れます。

```bash
python -m chart_ml generate-audio ^
  --model C:\path\to\simple_model.json ^
  --audio C:\path\to\audio.mp3 ^
  --output C:\path\to\generated.rchart ^
  --output-song-dir C:\Users\you\AppData\Local\raythm\songs\demo_song ^
  --song-id demo_song ^
  --chart-id demo_audio ^
  --title Demo Song ^
  --artist Demo Artist ^
  --jacket-source C:\path\to\jacket.jpg
```

`raythm` は通常 `AppData\Local\raythm\songs\{song_id}\` を見るので、ここへ出すとそのまま読ませやすいです。

### 9. ニューラルモデルで音源から `.rchart` を出す

```bash
python -m chart_ml generate-neural ^
  --model C:\path\to\neural_model.pt ^
  --audio C:\path\to\audio.mp3 ^
  --output C:\path\to\generated_neural.rchart ^
  --song-id demo_song ^
  --chart-id demo_neural ^
  --feature-profile full ^
  --timing-mode adaptive ^
  --level 8.5
```

必要なら `--output-song-dir` を併用して、そのまま game import / local songs 用 package も出せます。

overfit テストで元譜面と同じ timing を使いたい場合は、`--template-chart` を使います。

```bash
python -m chart_ml generate-neural ^
  --model C:\path\to\overfit_model.pt ^
  --audio C:\path\to\audio.mp3 ^
  --template-chart C:\path\to\template.rchart ^
  --output C:\path\to\generated_neural.rchart ^
  --song-id demo_song ^
  --chart-id demo_neural
```

## ディレクトリ構成

```text
tools/chart_ml/
  README.md
  pyproject.toml
  src/chart_ml/
    __init__.py
    __main__.py
    cli.py
    corpus.py
    dataset.py
    deep_dataset.py
    deep_model.py
    deep_training.py
    audio_features.py
    external_4k.py
    rchart.py
    timing.py
    timing_utils.py
    simple_model.py
    models.py
```

## 今後の想定

- 音声特徴抽出
- PyTorch の学習ループ
- 実曲での品質評価とアーキテクチャ改善
- BPM 変化対応の timing 生成
- 学習モデルを使った音源条件付き生成の改善
