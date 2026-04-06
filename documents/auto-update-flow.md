# Auto Update Flow

このドキュメントは、`Launcher.exe` と `Updater.exe` を使った現在の自動更新フローを整理したものです。

対象コード:

- [launcher_main.cpp](C:/Users/rento/CLionProjects/raythm/src/launcher_main.cpp)
- [updater_main.cpp](C:/Users/rento/CLionProjects/raythm/src/updater_main.cpp)
- [update_workflow.h](C:/Users/rento/CLionProjects/raythm/src/updater/update_workflow.h)
- [update_paths.h](C:/Users/rento/CLionProjects/raythm/src/updater/update_paths.h)

## Overview

更新の入口は常に `Launcher.exe` です。

通常フローは次のとおりです。

1. `Launcher.exe` が現在版を読む
2. GitHub Releases の最新情報を取る
3. 更新がなければ `raythm.exe` を直接起動する
4. 更新があれば `Updater.exe` を更新用引数付きで起動する
5. `Updater.exe` が package を取得して適用する
6. 成功したら新しい `raythm.exe` を起動する

## Update Window

Windows 版の `Launcher.exe` と `Updater.exe` はコンソールではなく GUI 実行です。

そのため、黒いターミナルは通常出さず、`Updater` は小さな進捗ウィンドウで現在の段階を表示します。

表示例:

- `Preparing update...`
- `Downloading update package...`
- `Verifying package...`
- `Extracting package...`
- `Applying update...`
- `Launching updated game...`

致命的な失敗時は、この更新ウィンドウからエラーダイアログを出します。

## Files And Directories

Updater が使う主な作業ディレクトリは `AppData/Local/raythm/updater/` 配下です。

- `version.json`
  - 現在インストール済みとして扱う版
- `downloads/`
  - `game-win64.zip`
  - `SHA256SUMS.txt`
- `staged/`
  - ZIP 展開先
- `backup/current/`
  - 適用前バックアップ
- `temp-updater/`
  - 一時コピーした `Updater.exe` と runtime DLL
- `update.log`
  - 更新ログ

パス定義は [update_paths.h](C:/Users/rento/CLionProjects/raythm/src/updater/update_paths.h) と [update_paths.cpp](C:/Users/rento/CLionProjects/raythm/src/updater/update_paths.cpp) にあります。

## Launcher Flow

[launcher_main.cpp](C:/Users/rento/CLionProjects/raythm/src/launcher_main.cpp) の役割は次です。

- package version を読む
- `version.json` が無ければ初期化する
- `fetch_latest_release_info()` で GitHub Releases の最新を取る
- `is_newer_version()` で更新有無を判定する

更新が必要なときは `update_launch_request` を組み立てて `Updater.exe` を起動します。

渡す引数は主に次です。

- `--current-version=...`
- `--target-version=...`
- `--target-tag=...`
- `--package-url=...`
- `--checksum-url=...`

## Updater Main Flow

[updater_main.cpp](C:/Users/rento/CLionProjects/raythm/src/updater_main.cpp) の流れは次です。

1. `version.json` を読む
2. 引数を parse する
3. package と checksum を `downloads/` へ保存する
4. SHA-256 を検証する
5. package を `staged/` へ展開する
6. 必要なら管理者権限で再起動する
7. 必要なら temp copy の updater へ切り替える
8. `raythm.exe` が止まるまで待つ
9. `staged/` を install root へ適用する
10. `version.json` を新しい版へ更新する
11. 新しい `raythm.exe` を起動する

## Why Elevation Happens

インストール先が `C:\Program Files\raythm` のような場所だと、そのままでは書き込めません。

その場合は:

- `can_write_to_directory()` で書き込み可否を確認
- 書けなければ `runas` で `Updater.exe` を昇格再起動

という流れになります。

ログ上は次のように見えます。

- `install directory requires elevation; relaunching updater`

## Why Temp Updater Exists

`Updater.exe` は MinGW runtime DLL を使っています。

- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

この updater を install root のまま実行すると、適用中にこれらの DLL を自分で掴んだままになります。すると DLL 上書きで `Permission denied` が起きます。

そのため現在は:

- install root の `Updater.exe` から直接 apply しない
- `AppData/Local/raythm/updater/temp-updater/` に
  - `Updater.exe`
  - runtime DLL
  をコピーする
- temp copy から再起動して、そのプロセスが apply する

という構成です。

ログ上は次のように見えます。

- `relaunching updater from temp copy`
- `relaunching temp updater copy`

## Why Temp Updater Is Still Created By Updater

`temp-updater` を作る責務は、設計上は `Launcher` に寄せることもできます。

ただし現在は `Updater` 側に置いています。主な理由は次です。

- install root が最終的にどこになるかを updater が知っている
- 管理者権限が必要かどうかを updater が判定している
- 昇格後の実体から見て「いま install root 上で動いているか」を updater 自身が判断できる

つまり今の構成では:

- `Launcher` は更新有無の判断と `Updater` 起動だけ
- `Updater` は「安全に apply できる実行形へ自分を切り替える」責務も持つ

という分担です。

将来的には、

- `Launcher` が先に temp 作成まで行う
- その後は final updater だけが download / verify / apply を担当する

という形へ整理する余地はあります。現状は、install root と権限判定を updater 側に閉じるほうを優先しています。

## Important Detail About Relaunch

昇格再起動と temp copy 再起動では、親 updater は子を待たずに終了します。

これは、親が install root 側の DLL を掴んだまま残ると、temp copy 側でも DLL を上書きできないためです。

## Install Root

Updater が更新対象にする install root は次で決まります。

- ふつうは `app_paths::executable_dir()`
- temp copy 起動時は `--install-root=...` で明示的に引き継ぐ

つまり、`cmake-build-codex/Updater.exe` を直接実行すると、その build ディレクトリを更新対象にしてしまいます。

実機確認では:

- release ZIP を別フォルダに展開したもの
- その中の `Launcher.exe`

から試すのが前提です。

## Release Assets Expected By Updater

Updater は GitHub Release に次の asset がある前提で動きます。

- `game-win64.zip`
- `SHA256SUMS.txt`

`Launcher` は `releases/latest` を見て、そこから asset URL を取ります。

## Logs To Read

更新がうまくいかないときは、まず `AppData/Local/raythm/updater/update.log` を見ます。

よくある見方:

- `package extraction succeeded`
  - ZIP 展開までは成功
- `install directory requires elevation; relaunching updater`
  - 管理者権限が必要
- `relaunching updater from temp copy`
  - DLL ロック回避のため temp copy へ切替
- `copy failed from ... to ...`
  - どのファイルで apply に失敗したか
- `failed to apply staged update`
  - 適用失敗
- `updated game launched`
  - 成功

## Common Failure Patterns

### 1. Updater started without a launch request

原因:

- `Updater.exe` を単体で直接起動した

意味:

- 更新用引数が無いので何もせず終了した

### 2. Failed to extract package into staging

原因候補:

- ZIP が壊れている
- `Expand-Archive` に失敗した

### 3. Permission denied During Apply

原因候補:

- install root への書き込み権限不足
- 実行中プロセスが DLL / exe を掴んでいる

### 4. Launcher Detects Update But Nothing Changes

原因候補:

- `Updater.exe` が別の install root に対して動いている
- `version.json` の更新に失敗した
- apply が途中で失敗した

## Practical Test Procedure

更新を試すときの現実的な手順:

1. release の `game-win64.zip` を任意フォルダに展開する
2. そのフォルダの `Launcher.exe` を起動する
3. 更新があれば `update.log` を見る
4. 成功後に `version.json` と起動済み `raythm.exe` を確認する

`cmake-build-codex` の `Updater.exe` を直接使うのは、更新実機確認には向いていません。
