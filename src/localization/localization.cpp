#include "localization/localization.h"

#include <array>
#include <cstring>

namespace localization {
namespace {

struct translation_entry {
    const char* english;
    const char* japanese;
};

struct literal_translation {
    const char* english;
    const char* japanese;
};

locale g_current_locale = locale::english;

constexpr std::array<translation_entry, static_cast<int>(text_key::editor_settings) + 1> kTranslations = {{
    {"Settings", "設定"},
    {"Saved on exit", "終了時に保存"},
    {"Saved on back", "戻ると保存"},
    {"Click tabs to switch pages", "タブでページを切り替え"},
    {"ESC or right click goes back", "ESC / 右クリックで戻る"},
    {"Back", "戻る"},
    {"Gameplay", "ゲームプレイ"},
    {"Play feel and lane settings", "Play feel and lane settings"},
    {"Audio", "オーディオ"},
    {"BGM and sound effect volume", "BGM and sound effect volume"},
    {"Video", "ビデオ"},
    {"Frame rate settings", "Frame rate settings"},
    {"System", "システム"},
    {"Language, display, and theme", "Language, display, and theme"},
    {"Key Config", "キー設定"},
    {"Per-lane keyboard bindings", "Per-lane keyboard bindings"},
    {"Language", "言語"},
    {"English", "English"},
    {"Japanese", "日本語"},
    {"Note Speed", "ノーツ速度"},
    {"Camera Angle", "カメラ角度"},
    {"Lane Width", "レーン幅"},
    {"Lane Cover", "レーンカバー"},
    {"Note Height", "ノーツ高さ"},
    {"Global Offset", "全体オフセット"},
    {"BGM Volume", "BGM 音量"},
    {"SE Volume", "効果音 音量"},
    {"Loudness Normalization", "ラウドネス正規化"},
    {"Enabled", "有効"},
    {"Disabled", "無効"},
    {"Frame Rate", "フレームレート"},
    {"Unlimited", "無制限"},
    {"Display", "表示"},
    {"Fullscreen", "フルスクリーン"},
    {"Windowed", "ウィンドウ"},
    {"Theme", "テーマ"},
    {"Dark", "ダーク"},
    {"Light", "ライト"},
    {"Mode", "モード"},
    {"Lane", "レーン"},
    {"Key is already assigned", "このキーはすでに割り当て済みです"},
    {"This key cannot be assigned", "このキーは割り当てできません"},
    {"Press a key...", "キーを押してください..."},
    {"No songs found yet.", "曲がまだありません。"},
    {"JACKET", "ジャケット"},
    {"Notes", "ノーツ"},
    {"BPM", "BPM"},
    {"Title", "タイトル"},
    {"Artist", "アーティスト"},
    {"Genre", "ジャンル"},
    {"Audio", "音声"},
    {"Jacket", "ジャケット"},
    {"Browse", "参照"},
    {"Create", "作成"},
    {"Creating...", "作成中..."},
    {"Preview (ms)", "プレビュー (ms)"},
    {"Select audio file...", "音声ファイルを選択..."},
    {"Select image file... (optional)", "画像ファイルを選択... (任意)"},
    {"Crop Image", "画像を切り抜き"},
    {"Zoom", "ズーム"},
    {"Cancel", "キャンセル"},
    {"Apply", "適用"},
    {"MV Metadata", "MV メタデータ"},
    {"Update the MV title and author.", "Update the MV title and author."},
    {"MV Name", "MV 名"},
    {"Author", "作者"},
    {"Author name", "作者名"},
    {"Untitled MV", "無題の MV"},
    {"Metadata", "メタデータ"},
    {"Score", "スコア"},
    {"Accuracy", "精度"},
    {"ALL PERFECT", "ALL PERFECT"},
    {"FULL COMBO", "FULL COMBO"},
    {"FAILED", "FAILED"},
    {"Max Combo", "最大コンボ"},
    {"Avg Offset", "平均オフセット"},
    {"Fast", "Fast"},
    {"Slow", "Slow"},
    {"ENTER: Song Select    R: Retry    Use AUTO APPLY there",
     "ENTER: 曲選択    R: リトライ    AUTO APPLY は曲選択で使用"},
    {"Resume", "再開"},
    {"Retry", "リトライ"},
    {"Song Select", "曲選択"},
    {"Settings", "設定"},
    {"Back", "戻る"},
    {"Settings", "設定"},
}};

constexpr literal_translation kLiteralTranslations[] = {
    {"PLAY", "プレイ"},
    {"MULTIPLAY", "マルチプレイ"},
    {"BROWSE", "ブラウズ"},
    {"CREATE", "作成"},
    {"SELECT", "選択"},
    {"Rooms", "ルーム"},
    {"Create Room", "ルームを作成"},
    {"Sign in from the account menu before joining multiplayer.", "マルチプレイに参加するにはアカウントメニューからサインインしてください。"},
    {"No rooms yet.", "ルームはまだありません。"},
    {"No chart selected", "譜面未選択"},
    {"host", "ホスト"},
    {"lobby", "ロビー"},
    {"playing", "プレイ中"},
    {"online", "オンライン"},
    {"away", "離席"},
    {"Players", "プレイヤー"},
    {"READY", "準備完了"},
    {"WAIT", "待機"},
    {"Beatmap queue", "譜面キュー"},
    {"Add song", "曲を追加"},
    {"No queued songs yet.", "キューに曲はまだありません。"},
    {"Installed", "所持済み"},
    {"Not installed", "未所持"},
    {"Up", "上へ"},
    {"Down", "下へ"},
    {"Remove", "削除"},
    {"Chat", "チャット"},
    {"Message...", "メッセージ..."},
    {"Send", "送信"},
    {"Playing:", "プレイ中:"},
    {"Queue: host only", "キュー: ホストのみ"},
    {"Queue: all players", "キュー: 全員"},
    {"Leave", "退出"},
    {"Cancel Ready", "準備を取り消す"},
    {"Ready (queue empty)", "準備 (キュー空)"},
    {"Ready", "準備完了"},
    {"Ready (download needed)", "準備 (DL 必要)"},
    {"Download", "ダウンロード"},
    {"Start", "開始"},
    {"Room name", "ルーム名"},
    {"Optional", "任意"},
    {"Players:", "人数:"},
    {"Max", "最大"},
    {"Room password", "ルームパスワード"},
    {"Joining...", "参加中..."},
    {"Join", "参加"},
    {"Search", "検索"},
    {"Solo song select.", "ひとりで曲を選択"},
    {"Room battles soon.", "ルーム対戦は準備中"},
    {"Browse and download.", "曲を探してダウンロード"},
    {"Create, import, export.", "作成・インポート・エクスポート"},
    {"This route is still warming up.", "このルートはまだ準備中です。"},
    {"trace the line before the beat disappears", "trace the line before the beat disappears"},
    {"NEW SONG", "新規曲"},
    {"EDIT SONG", "曲を編集"},
    {"IMPORT SONG", "曲をインポート"},
    {"UPLOAD CHART", "譜面をアップロード"},
    {"SELECT SONG", "曲を選択"},
    {"OFFICIAL SONG", "公式曲"},
    {"LINKED SONG", "連携済み曲"},
    {"UPLOADED", "アップロード済み"},
    {"SELECT CHART", "譜面を選択"},
    {"UPDATE CHART", "譜面を更新"},
    {"EDIT CHART", "譜面を編集"},
    {"EXPORT CHART", "譜面をエクスポート"},
    {"OFFICIAL CHART", "公式譜面"},
    {"COMMUNITY CHART", "コミュニティ譜面"},
    {"MV EDITOR", "MV エディタ"},
    {"Song", "曲"},
    {"Chart", "譜面"},
    {"More", "その他"},
    {"HOME", "ホーム"},
    {"CREATE TOOLS", "作成ツール"},
    {"DELETE SONG", "曲を削除"},
    {"DELETE CHART", "譜面を削除"},
    {"RANKINGS", "ランキング"},
    {"RANKING", "ランキング"},
    {"LOCAL", "ローカル"},
    {"ONLINE", "オンライン"},
    {"Unknown Player", "不明なプレイヤー"},
    {"CLOSE", "閉じる"},
    {"OVERVIEW", "概要"},
    {"ACTIVITY", "履歴"},
    {"SONGS", "曲"},
    {"CHARTS", "譜面"},
    {"Recent Activity", "最近のプレイ"},
    {"No recent play activity yet.", "最近のプレイはまだありません。"},
    {"No #1 online records yet.", "オンライン 1 位記録はまだありません。"},
    {"Verified profile", "認証済みプロフィール"},
    {"Email verification pending", "メール認証待ち"},
    {"Manage account", "アカウント管理"},
    {"Loading profile...", "プロフィールを読み込み中..."},
    {"Uploaded Songs", "アップロードした曲"},
    {"Uploaded Charts", "アップロードした譜面"},
    {"Recent Plays", "最近のプレイ"},
    {"Uploaded content could not be loaded.", "アップロードしたコンテンツを読み込めませんでした。"},
    {"No uploaded songs.", "アップロードした曲はありません。"},
    {"No uploaded charts.", "アップロードした譜面はありません。"},
    {"Profile Links", "プロフィールリンク"},
    {"Label", "ラベル"},
    {"X / YouTube / Site", "X / YouTube / サイト"},
    {"SAVE LINKS", "リンクを保存"},
    {"Public on profile and uploaded content cards.", "プロフィールとアップロードしたコンテンツに公開されます。"},
    {"Profile Image", "プロフィール画像"},
    {"CHANGE IMAGE", "画像を変更"},
    {"REMOVE IMAGE", "画像を削除"},
    {"Delete this account from raythm-Server.", "raythm-Server のアカウントを削除します。"},
    {"This does not delete local songs or charts.", "ローカルの曲や譜面は削除されません。"},
    {"DELETE ACCOUNT", "アカウント削除"},
    {"Saving links...", "リンクを保存中..."},
    {"Saving profile image...", "プロフィール画像を保存中..."},
    {"Failed to crop profile image.", "プロフィール画像の切り抜きに失敗しました。"},
    {"Failed to open image.", "画像を開けませんでした。"},
    {"Avatar image could not be read.", "プロフィール画像を読み込めませんでした。"},
    {"Failed to upload profile image.", "プロフィール画像のアップロードに失敗しました。"},
    {"Profile image uploaded, but the local session could not be updated.",
     "プロフィール画像はアップロードされましたが、ローカルセッションを更新できませんでした。"},
    {"Profile image updated.", "プロフィール画像を更新しました。"},
    {"Failed to remove profile image.", "プロフィール画像の削除に失敗しました。"},
    {"Profile image removed, but the local session could not be updated.",
     "プロフィール画像は削除されましたが、ローカルセッションを更新できませんでした。"},
    {"Profile image removed.", "プロフィール画像を削除しました。"},
    {"Profile links saved.", "プロフィールリンクを保存しました。"},
    {"Failed to save profile links.", "プロフィールリンクの保存に失敗しました。"},
    {"Server returned an unexpected profile response.", "サーバーから予期しないプロフィール応答が返りました。"},
    {"Profile links saved, but the local session could not be updated.",
     "プロフィールリンクは保存されましたが、ローカルセッションを更新できませんでした。"},
    {"Saving profile links failed.", "プロフィールリンクの保存に失敗しました。"},
    {"Password is required to delete the account.", "アカウント削除にはパスワードが必要です。"},
    {"Deleting...", "削除中..."},
    {"Delete Account", "アカウント削除"},
    {"Enter your password to permanently delete this server account.",
     "このサーバーアカウントを完全に削除するにはパスワードを入力してください。"},
    {"Delete account password", "アカウント削除用パスワード"},
    {"CANCEL", "キャンセル"},
    {"Refreshing...", "更新中..."},
    {"Loading...", "読み込み中..."},
    {"Starting...", "開始中..."},
    {"Loaded. Waiting for players...", "読み込み完了。プレイヤーを待機中..."},
    {"Waiting for other players...", "他のプレイヤーを待機中..."},
    {"Starting in", "開始まで"},
    {"No playable song package found", "プレイ可能な曲パッケージが見つかりません。"},
    {"Failed to load selected chart", "選択した譜面を読み込めませんでした。"},
    {"No chart found for selected key mode", "選択したキー数の譜面が見つかりません。"},
    {"ESC: Back to Song Select", "ESC: 曲選択へ戻る"},
    {"Syncing owned songs...", "所有曲を同期中..."},
    {"Official catalog", "公式カタログ"},
    {"Community catalog", "コミュニティカタログ"},
    {"Owned library", "所有ライブラリ"},
    {"OFFICIAL", "公式"},
    {"COMMUNITY", "コミュニティ"},
    {"OWNED", "所有"},
    {"SEARCH", "検索"},
    {"songs / artists", "曲 / アーティスト"},
    {"Press Esc to return to the grid", "Esc で一覧に戻る"},
    {"Could not reach raythm-Server.", "raythm-Server に接続できません。"},
    {"Check the server URL and confirm raythm-Server is running.", "サーバー URL と起動状態を確認してください。"},
    {"No songs found.", "曲が見つかりません。"},
    {"Could not load charts.", "譜面を読み込めませんでした。"},
    {"DOWNLOADING...", "ダウンロード中..."},
    {"UPDATE SONG", "曲を更新"},
    {"DOWNLOAD SONG", "曲をダウンロード"},
    {"OPEN LOCAL", "ローカルを開く"},
    {"GET", "取得"},
    {"UPDATE", "更新"},
    {"GENRES", "ジャンル"},
    {"KEYWORDS", "キーワード"},
    {"GLOBAL RANKING", "グローバルランキング"},
    {"FILTER", "フィルター"},
    {"SOURCE", "ソース"},
    {"STATUS", "状態"},
    {"LEVEL", "レベル"},
    {"KEYS", "キー"},
    {"CLEAR FILTERS", "フィルター解除"},
    {"Downloading song...", "曲をダウンロード中..."},
    {"Downloading chart...", "譜面をダウンロード中..."},
    {"Download the song first.", "先に曲をダウンロードしてください。"},
    {"Song downloaded.", "曲をダウンロードしました。"},
    {"Chart downloaded.", "譜面をダウンロードしました。"},
    {"Download failed.", "ダウンロードに失敗しました。"},
    {"Invalid URL.", "URL が不正です。"},
    {"Failed to connect to server.", "サーバー接続に失敗しました。"},
    {"Could not connect to raythm-Server.", "raythm-Server に接続できません。"},
    {"Account", "アカウント"},
    {"Connect to raythm-Server", "raythm-Server に接続"},
    {"Signed in", "サインイン済み"},
    {"Email verified", "メール認証済み"},
    {"Verify on the Web to submit online scores.", "オンラインスコア投稿には Web 認証が必要です。"},
    {"LOGIN", "ログイン"},
    {"SIGN UP", "登録"},
    {"Name", "名前"},
    {"Display name", "表示名"},
    {"Email", "メール"},
    {"Pass", "パス"},
    {"Password", "パスワード"},
    {"Confirm", "確認"},
    {"Repeat password", "パスワード再入力"},
    {"Code", "コード"},
    {"6 digit code", "6 桁コード"},
    {"RESEND", "再送信"},
    {"VERIFY", "認証"},
    {"PROFILE", "プロフィール"},
    {"REFRESH", "更新"},
    {"LOGOUT", "ログアウト"},
    {"SONG SELECT", "曲選択"},
    {"ACCOUNT", "アカウント"},
    {"SETTINGS", "設定"},
    {"No songs found", "曲が見つかりません"},
    {"Local Offset", "ローカルオフセット"},
    {"Songs", "曲"},
    {"SONG >", "曲 >"},
    {"CHART >", "譜面 >"},
    {"MV >", "MV >"},
    {"< BACK", "< 戻る"},
    {"EDIT META", "メタ編集"},
    {"EXPORT SONG", "曲をエクスポート"},
    {"NEW CHART", "新規譜面"},
    {"IMPORT CHART", "譜面をインポート"},
    {"EDIT MV", "MV 編集"},
    {"DELETE MV", "MV 削除"},
    {"Delete Song", "曲を削除"},
    {"Delete Chart", "譜面を削除"},
    {"Confirm Action", "操作の確認"},
    {"This action cannot be undone.", "この操作は元に戻せません。"},
    {"DELETE", "削除"},
    {"CONFIRM", "確認"},
    {"Edit Song", "曲を編集"},
    {"New Song", "新規曲"},
    {"Song Created", "曲を作成しました"},
    {"Update song metadata", "曲メタデータを更新"},
    {"Enter song metadata", "曲メタデータを入力"},
    {"Choose the next action", "次の操作を選択"},
    {"SAVE", "保存"},
    {"ADD CHART", "譜面を追加"},
    {"ADD LATER", "あとで追加"},
    {"YouTube", "YouTube"},
    {"Niconico", "ニコニコ"},
    {"X", "X"},
    {"URL (optional)", "URL (任意)"},
    {"Reading song package(s)...", "曲パッケージを読み込み中..."},
    {"Exporting song package...", "曲パッケージを書き出し中..."},
    {"Importing song package(s)...", "曲パッケージをインポート中..."},
    {"BACK", "戻る"},
    {"Audio", "音声"},
    {"Offset", "オフセット"},
    {"WAVE ON", "波形 ON"},
    {"WAVE OFF", "波形 OFF"},
    {"Snap", "スナップ"},
    {"No audio", "音声なし"},
    {"Modified", "変更あり"},
    {"Saved", "保存済み"},
    {"Unsaved", "未保存"},
    {"Chart", "譜面"},
    {"Existing chart", "既存譜面"},
    {"New chart", "新規譜面"},
    {"Diff", "難易度"},
    {"Difficulty", "難易度"},
    {"New", "新規"},
    {"Unknown", "不明"},
    {"Status", "状態"},
    {"Unsaved Changes", "未保存の変更"},
    {"There are unsaved changes.", "未保存の変更があります。"},
    {"Save before leaving the editor?", "エディタを離れる前に保存しますか？"},
    {"SAVE", "保存"},
    {"DISCARD", "破棄"},
    {"Save Chart", "譜面を保存"},
    {"Save into this song's charts directory.", "この曲の charts フォルダに保存します。"},
    {"File", "ファイル"},
    {"Change Key Mode", "キーモード変更"},
    {"All placed notes will be cleared.", "配置済みノーツはすべて消去されます。"},
    {"Meter", "拍子"},
    {"Click TL", "TL をクリック"},
    {"Timing Events", "タイミングイベント"},
    {"Delete", "削除"},
    {"Event Editor", "イベントエディタ"},
    {"Type", "種類"},
    {"Bar", "小節"},
    {"Num", "分子"},
    {"Den", "分母"},
    {"Select a timing event from the list.", "リストからタイミングイベントを選択してください。"},
    {"Song Timing", "曲タイミング"},
    {"Song BPM", "曲 BPM"},
    {"Song Offset", "曲オフセット"},
    {"EDIT", "編集"},
    {"IMPORT MIDI", "MIDI 読み込み"},
    {"Reads MIDI tempo and time signature events.", "MIDI のテンポと拍子イベントを読み込みます。"},
    {"DONE", "完了"},
    {"Add BPM", "BPM 追加"},
    {"Add Time Sig", "拍子追加"},
    {"Time Sig", "拍子"},
    {"Metronome", "メトロノーム"},
    {"Metronome On", "メトロノーム ON"},
    {"Imported MIDI timing events:", "MIDI タイミング読込数:"},
    {"Normalized PPQ:", "PPQ 正規化:"},
    {"Audio file is required.", "音声ファイルが必要です。"},
    {"Failed to load audio preview.", "音声プレビューを読み込めませんでした。"},
    {"PERFECT", "PERFECT"},
    {"GREAT", "GREAT"},
    {"GOOD", "GOOD"},
    {"BAD", "BAD"},
    {"MISS", "MISS"},
    {"Perfect", "Perfect"},
    {"Great", "Great"},
    {"Good", "Good"},
    {"Bad", "Bad"},
    {"Miss", "Miss"},
    {"NO JACKET", "ジャケットなし"},
    {"COMBO", "COMBO"},
    {"PAUSED", "一時停止"},
    {"RESUME", "再開"},
    {"RESTART", "リスタート"},
    {"ESC: Resume", "ESC: 再開"},
    {"FAILED...", "FAILED..."},
    {"Play", "プレイ"},
    {"ESC: Back to Song Select", "ESC: 曲選択へ戻る"},
    {"Unknown Title", "不明なタイトル"},
    {"Local best updated.", "ローカルベストを更新しました。"},
    {"Submitting online ranking...", "オンラインランキング送信中..."},
    {"Online ranking updated.", "オンラインランキングを更新しました。"},
    {"Submitted score did not beat your online best.", "自己ベスト更新ではありませんでした。"},
    {"Submitted result did not beat your current best.", "自己ベスト更新ではありませんでした。"},
    {"Online ranking disabled for this play.", "このプレイではオンラインランキングは無効です。"},
    {"Failed play is not ranked.", "FAILED のためランキング対象外です。"},
    {"This result is not ranking eligible.", "このリザルトはランキング対象外です。"},
    {"Ranking Updated", "オンラインランキングを更新しました。"},
    {"Submitted - Not Updated", "送信済み - 自己ベスト更新なし"},
    {"Local Best Updated", "ローカルベストを更新しました。"},
    {"Result Saved Locally", "リザルトをローカルに保存しました。"},
    {"LOCAL BEST", "ローカルベスト"},
    {"ONLINE BEST", "オンラインベスト"},
    {"RANK", "順位"},
    {"NOT UPDATED", "更新なし"},
    {"PENDING", "送信中"},
    {"UPDATED", "更新"},
    {"OFFLINE", "オフライン"},
    {"BEST", "BEST"},
    {"Replay", "リプレイ"},
};

constexpr int key_index(text_key key) {
    return static_cast<int>(key);
}

const translation_entry& entry_for(text_key key) {
    return kTranslations[static_cast<std::size_t>(key_index(key))];
}

}  // namespace

void set_current_locale(locale value) {
    g_current_locale = value;
}

locale current_locale() {
    return g_current_locale;
}

const char* locale_code(locale value) {
    switch (value) {
        case locale::japanese:
            return "ja";
        case locale::english:
        default:
            return "en";
    }
}

const char* locale_display_name(locale value) {
    switch (value) {
        case locale::japanese:
            return tr(text_key::japanese);
        case locale::english:
        default:
            return tr(text_key::english);
    }
}

std::optional<locale> parse_locale_code(std::string_view code) {
    if (code == "en" || code == "en-US" || code == "english") {
        return locale::english;
    }
    if (code == "ja" || code == "ja-JP" || code == "japanese") {
        return locale::japanese;
    }
    return std::nullopt;
}

locale parse_locale_code_or_default(std::string_view code, locale fallback) {
    if (const std::optional<locale> parsed = parse_locale_code(code)) {
        return *parsed;
    }
    return fallback;
}

const char* tr(text_key key) {
    return tr(key, g_current_locale);
}

const char* tr(text_key key, locale value) {
    const translation_entry& entry = entry_for(key);
    if (value == locale::japanese && entry.japanese != nullptr && entry.japanese[0] != '\0') {
        return entry.japanese;
    }
    return entry.english != nullptr ? entry.english : "";
}

const char* tr_literal(const char* english_literal) {
    if (english_literal == nullptr || g_current_locale == locale::english) {
        return english_literal;
    }

    for (int i = 0; i < text_key_count(); ++i) {
        const auto key = static_cast<text_key>(i);
        if (std::strcmp(english_literal, english_text(key)) == 0) {
            return tr(key);
        }
    }

    for (const literal_translation& entry : kLiteralTranslations) {
        if (std::strcmp(english_literal, entry.english) == 0) {
            return entry.japanese;
        }
    }

    return english_literal;
}

const char* english_text(text_key key) {
    return tr(key, locale::english);
}

bool has_translation(text_key key, locale value) {
    const translation_entry& entry = entry_for(key);
    if (value == locale::japanese) {
        return entry.japanese != nullptr && entry.japanese[0] != '\0';
    }
    return entry.english != nullptr && entry.english[0] != '\0';
}

int text_key_count() {
    return static_cast<int>(kTranslations.size());
}

}  // namespace localization
