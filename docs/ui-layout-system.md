# UI レイアウトシステム

## 概要

`src/ui/` 以下のヘッダーオンリーライブラリ。2つの層で構成される:

1. **レイアウト層** (`ui_layout.h`, `ui_text.h`, `ui_hit.h`): Rectangle の位置・サイズ計算とヒットテスト
2. **描画ユーティリティ層** (`ui_draw.h`): ボタン・パネル・行UI等の頻出描画パターンをまとめた関数群

### 設計思想

- **値型 + フリー関数**: 継承や virtual なし。constexpr 対応のシンプルな構造体と関数
- **ヘッダーオンリー**: .cpp 不要。`namespace ui` に全 API を格納
- **既存システムと共存**: theme.h（カラー）、virtual_screen（解像度スケーリング）、scene 構造はそのまま
- **段階的移行**: 既存コードを壊さずにシーン単位で順次導入可能

### ファイル構成

| ファイル | 内容 |
|---------|------|
| `src/ui/ui_layout.h` | コア型（anchor, edge_insets）とレイアウト関数（place, inset, vstack, hstack, grid 等） |
| `src/ui/ui_text.h` | テキスト配置ユーティリティ（text_position, draw_text_in_rect） |
| `src/ui/ui_hit.h` | ヒットテスト（is_hovered, is_pressed, is_clicked） |
| `src/ui/ui_draw.h` | 描画ユーティリティ（draw_button, draw_panel, draw_section, draw_progress_bar 等） |

---

## API リファレンス

### コア型

#### `ui::anchor`

9点アンカー。Rectangle 上の基準点を指定する enum class。

```
top_left ──── top_center ──── top_right
    │              │              │
center_left ── center ──── center_right
    │              │              │
bottom_left ─ bottom_center ─ bottom_right
```

```cpp
enum class anchor : int {
    top_left = 0, top_center, top_right,
    center_left,  center,     center_right,
    bottom_left,  bottom_center, bottom_right,
};
```

#### `ui::edge_insets`

四辺のインセット値。padding や margin に使用する。

```cpp
struct edge_insets {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    static constexpr edge_insets uniform(float v);
    static constexpr edge_insets symmetric(float vertical, float horizontal);
};
```

**ファクトリ関数:**

| 関数 | 説明 | 例 |
|------|------|----|
| `uniform(8.0f)` | 全辺 8px | `{8, 8, 8, 8}` |
| `symmetric(12.0f, 24.0f)` | 上下 12px、左右 24px | `{12, 24, 12, 24}` |

#### `ui::text_align`

テキストの水平揃え方向。

```cpp
enum class text_align { left, center, right };
```

#### `ui::rect_pair`

2分割レイアウトの戻り値に使うシンプルな構造体。

```cpp
struct rect_pair {
    Rectangle first;
    Rectangle second;
};
```

#### `ui::scroll_metrics`

スクロールバー計算の戻り値。

```cpp
struct scroll_metrics {
    float max_scroll;
    Rectangle thumb_rect;
};
```

---

### レイアウト関数 (`ui_layout.h`)

すべて `namespace ui` 内のフリー関数。

#### `anchor_point`

```cpp
constexpr Vector2 anchor_point(Rectangle rect, anchor a);
```

`rect` 上のアンカー座標を返す。

```cpp
// 例: 画面中央の座標
constexpr Rectangle screen = {0, 0, 1280, 720};
Vector2 pt = ui::anchor_point(screen, ui::anchor::center);  // {640, 360}
```

#### `place`

```cpp
constexpr Rectangle place(Rectangle parent,
                          float child_width, float child_height,
                          anchor parent_anchor,
                          anchor child_anchor = anchor::top_left,
                          Vector2 offset = {0, 0});
```

`parent` 上の `parent_anchor` 位置に、`child_width` x `child_height` の矩形を `child_anchor` 基準で配置する。

- `parent_anchor`: 親のどこに置くか
- `child_anchor`: 子のどの点を基準にするか（デフォルト: 左上）
- `offset`: 微調整用のオフセット

```cpp
constexpr Rectangle screen = {0, 0, 1280, 720};

// 画面中央に 420x320 のパネルを配置
constexpr Rectangle panel = ui::place(screen, 420, 320,
                                      ui::anchor::center, ui::anchor::center);
// → {430, 200, 420, 320}

// 右下隅に 100x40 のボタンを配置（24px マージン）
constexpr Rectangle btn = ui::place(screen, 100, 40,
                                    ui::anchor::bottom_right, ui::anchor::bottom_right,
                                    {-24, -24});
// → {1156, 656, 100, 40}

// パネル左上から (16, 12) オフセットした位置にラベルを配置
constexpr Rectangle label = ui::place(panel, 200, 30,
                                      ui::anchor::top_left, ui::anchor::top_left,
                                      {16, 12});
```

#### `inset`

```cpp
constexpr Rectangle inset(Rectangle rect, edge_insets edges);
constexpr Rectangle inset(Rectangle rect, float amount);  // 全辺均一
```

矩形を内側に縮小する。padding の適用に使用する。

```cpp
constexpr Rectangle panel = {100, 100, 400, 300};
constexpr Rectangle content = ui::inset(panel, 16.0f);
// → {116, 116, 368, 268}

constexpr Rectangle content2 = ui::inset(panel, ui::edge_insets::symmetric(24, 16));
// → {116, 124, 368, 252}
```

#### `center`

```cpp
constexpr Rectangle center(Rectangle parent, float w, float h);
```

`parent` の中央に配置する。`place(parent, w, h, anchor::center, anchor::center)` と同等。

```cpp
constexpr Rectangle screen = {0, 0, 1280, 720};
constexpr Rectangle dialog = ui::center(screen, 500, 300);
// → {390, 210, 500, 300}
```

#### `split_columns`

```cpp
constexpr rect_pair split_columns(Rectangle parent, float first_width,
                                  float spacing = 0.0f);
```

`parent` を左右2カラムに分割する。`first_width` が左カラム幅、残りが右カラムになる。

```cpp
constexpr Rectangle row = {100, 100, 500, 48};
constexpr ui::rect_pair cols = ui::split_columns(row, 180.0f, 16.0f);
// cols.first  = {100, 100, 180, 48}
// cols.second = {296, 100, 304, 48}
```

#### `split_rows`

```cpp
constexpr rect_pair split_rows(Rectangle parent, float first_height,
                               float spacing = 0.0f);
```

`parent` を上下2行に分割する。タイトル行 + サブタイトル行のような構成に向く。

```cpp
constexpr Rectangle header = {40, 60, 400, 64};
constexpr ui::rect_pair rows = ui::split_rows(header, 34.0f, 8.0f);
// rows.first  = {40, 60, 400, 34}
// rows.second = {40, 102, 400, 22}
```

#### `scroll_view`

```cpp
constexpr Rectangle scroll_view(Rectangle container, float header_height = 0.0f,
                                float bottom_padding = 0.0f);
```

固定ヘッダや下余白を除いたスクロール対象領域を返す。

```cpp
constexpr Rectangle list_rect = {790, 44, 466, 660};
constexpr Rectangle view_rect = ui::scroll_view(list_rect, 48.0f, 12.0f);
// → {790, 92, 466, 600}
```

#### `vertical_scroll_metrics`

```cpp
inline scroll_metrics vertical_scroll_metrics(Rectangle track_rect, float content_height,
                                              float scroll_offset,
                                              float min_thumb_height = 36.0f);
```

表示領域の高さとコンテンツ高さから、最大スクロール量とサム矩形を計算する。

#### `vstack`

```cpp
inline int vstack(Rectangle parent, float item_height, float spacing,
                  std::span<Rectangle> out);
```

`parent` 内に固定高さの要素を上から垂直に並べる。幅は `parent` に合わせる。

```cpp
Rectangle buttons[3];
ui::vstack({470, 220, 340, 184}, 42.0f, 16.0f, buttons);
// buttons[0] = {470, 220, 340, 42}
// buttons[1] = {470, 278, 340, 42}
// buttons[2] = {470, 336, 340, 42}
```

#### `hstack`

```cpp
inline int hstack(Rectangle parent, float item_width, float spacing,
                  std::span<Rectangle> out);
```

`parent` 内に固定幅の要素を左から水平に並べる。高さは `parent` に合わせる。

```cpp
Rectangle cols[3];
ui::hstack({0, 0, 400, 100}, 120.0f, 10.0f, cols);
// cols[0] = {0, 0, 120, 100}
// cols[1] = {130, 0, 120, 100}
// cols[2] = {260, 0, 120, 100}
```

#### `vstack_fill` / `hstack_fill`

```cpp
inline int vstack_fill(Rectangle parent, float spacing, std::span<Rectangle> out);
inline int hstack_fill(Rectangle parent, float spacing, std::span<Rectangle> out);
```

親の高さ（または幅）を要素数で均等分割する。

```cpp
Rectangle rows[4];
ui::vstack_fill({0, 0, 200, 400}, 8.0f, rows);
// 各行の高さ = (400 - 8*3) / 4 = 94
```

#### `grid`

```cpp
inline int grid(Rectangle parent, int cols,
                float cell_width, float cell_height,
                float h_spacing, float v_spacing,
                std::span<Rectangle> out);
```

グリッド配置。`cols` 列で左上から順に並べる。

```cpp
Rectangle cells[6];
ui::grid({100, 100, 500, 300}, 3, 150, 80, 10, 10, cells);
// cells[0] = {100, 100, 150, 80}  // row 0, col 0
// cells[1] = {260, 100, 150, 80}  // row 0, col 1
// ...
// cells[3] = {100, 190, 150, 80}  // row 1, col 0
```

---

### テキストユーティリティ (`ui_text.h`)

#### `text_position`

```cpp
inline Vector2 text_position(const char* text, int font_size, Rectangle rect,
                             text_align align = text_align::center);
```

`rect` 内でテキストを描画するための座標を返す。水平は `align` 指定、垂直は中央揃え。
内部で `MeasureText()` を呼ぶためランタイム専用（constexpr 不可）。

```cpp
Rectangle title_area = {100, 100, 400, 60};
Vector2 pos = ui::text_position("PAUSED", 42, title_area);
DrawText("PAUSED", (int)pos.x, (int)pos.y, 42, g_theme->text);
```

#### `draw_text_in_rect`

```cpp
inline void draw_text_in_rect(const char* text, int font_size, Rectangle rect,
                              Color color, text_align align = text_align::center);
```

`text_position` で座標を計算し `DrawText` を呼ぶ便利関数。

```cpp
// 中央揃え（デフォルト）
ui::draw_text_in_rect("PAUSED", 42, title_rect, g_theme->text);

// 左揃え
ui::draw_text_in_rect("ESC: Resume", 20, hint_rect, g_theme->text_muted, ui::text_align::left);

// 右揃え
ui::draw_text_in_rect("99.99%", 24, score_rect, g_theme->text, ui::text_align::right);
```

---

### ヒットテスト (`ui_hit.h`)

`virtual_screen::get_virtual_mouse()` を使用して仮想座標系でのマウス判定を行う。

#### `is_hovered`

```cpp
inline bool is_hovered(Rectangle rect);
```

仮想マウスカーソルが `rect` 内にあるか判定する。

#### `is_pressed`

```cpp
inline bool is_pressed(Rectangle rect);
```

`is_hovered(rect) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)` と同等。
ボタンの押下フィードバック（押し込み表現など）に使用する。

#### `is_clicked`

```cpp
inline bool is_clicked(Rectangle rect);
```

`is_hovered(rect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)` と同等。
ボタンのクリックアクション判定に使用する。

```cpp
Rectangle btn = {100, 100, 200, 40};
if (ui::is_clicked(btn)) {
    // ボタンがクリックされた
}

// 押し込みフィードバック
const Rectangle visual = ui::is_pressed(btn) ? ui::inset(btn, 1.5f) : btn;
DrawRectangleRec(visual, color);
```

---

### 描画ユーティリティ (`ui_draw.h`)

頻出する描画パターンをまとめた関数群。`g_theme` を参照する。

#### `draw_button`

```cpp
inline button_state draw_button(Rectangle rect, const char* label, int font_size,
                                float border_width = 2.0f);
```

標準ボタンを描画する。hover で `row` → `row_hover` に色変化、press で 1.5px 押し込み、テキスト中央揃え。
戻り値 `button_state` の `clicked` フィールドでアクション判定できる。

```cpp
struct button_state {
    bool hovered;   // マウスが上にある
    bool pressed;   // マウスボタン押下中
    bool clicked;   // マウスボタンを離した瞬間（アクション判定用）
};
```

```cpp
Rectangle btn = {100, 100, 200, 40};
if (ui::draw_button(btn, "RESUME", 24).clicked) {
    // ボタンがクリックされた
}
```

#### `draw_button_colored`

```cpp
inline button_state draw_button_colored(Rectangle rect, const char* label, int font_size,
                                        Color bg, Color bg_hover, Color text_color,
                                        float border_width = 2.0f);
```

カスタム色のボタン。選択状態やアクティブ状態の表現に使用する。

```cpp
// 選択中のタブ
ui::draw_button_colored(tab_rect, "Gameplay", 22,
                        g_theme->row_active, g_theme->row_active, g_theme->text);

// 非選択のタブ
ui::draw_button_colored(tab_rect, "Audio", 22,
                        g_theme->row, g_theme->row_hover, g_theme->text_secondary);
```

#### `draw_row`

```cpp
struct row_state {
    bool hovered;
    bool pressed;
    bool clicked;
    Rectangle visual;
};

inline row_state draw_row(Rectangle rect, Color bg, Color bg_hover,
                          Color border_color, float border_width = 2.0f);
```

行背景とボーダーだけを描画する汎用 helper。中のテキストやアイコンは呼び出し側が自由に載せる。

```cpp
const ui::row_state row = ui::draw_row(item_rect, g_theme->row, g_theme->row_hover, g_theme->border);
ui::draw_text_in_rect("Lane 1", 24, ui::inset(row.visual, 18.0f), g_theme->text, ui::text_align::left);
```

#### `draw_selectable_row`

```cpp
inline row_state draw_selectable_row(Rectangle rect, bool selected,
                                     float border_width = 2.0f);
```

選択状態付きの行UI。`settings_scene` のキー割当行や `song_select_scene` のリスト行に向く。

```cpp
const ui::row_state row = ui::draw_selectable_row(chart_rect, chart_index == difficulty_index_);
```

#### `draw_panel`

```cpp
inline void draw_panel(Rectangle rect);
```

メインパネルを描画する（`panel` 背景 + `border` ボーダー 2px）。

```cpp
ui::draw_panel(kMainPanel);
// 以下と同等:
// DrawRectangleRec(kMainPanel, g_theme->panel);
// DrawRectangleLinesEx(kMainPanel, 2.0f, g_theme->border);
```

#### `draw_section`

```cpp
inline void draw_section(Rectangle rect);
```

セクションパネルを描画する（`section` 背景 + `border_light` ボーダー 1.5px）。

```cpp
ui::draw_section(kSongInfoRect);
// 以下と同等:
// DrawRectangleRec(kSongInfoRect, g_theme->section);
// DrawRectangleLinesEx(kSongInfoRect, 1.5f, g_theme->border_light);
```

#### `draw_label_value`

```cpp
inline void draw_label_value(Rectangle rect, const char* label, const char* value,
                             int font_size, Color label_color, Color value_color,
                             float label_width = 200.0f);
```

左にラベル、右に値を表示する行。設定画面やリザルト画面の統計行に使用する。

```cpp
Rectangle row = {100, 100, 500, 40};
ui::draw_label_value(row, "Max Combo", "342", 24, g_theme->text_dim, g_theme->text);
```

#### `draw_value_selector`

```cpp
struct selector_state {
    row_state row;
    button_state left;
    button_state right;
};

inline selector_state draw_value_selector(Rectangle rect, const char* label, const char* value,
                                          int font_size = 24, float button_size = 34.0f,
                                          float label_width = 200.0f,
                                          float content_padding = 18.0f);
```

`< value >` 型の選択行を描画する。解像度、テーマ、4K/6K 切り替えのようなUI向け。

```cpp
const ui::selector_state selector = ui::draw_value_selector(row, "Theme",
    g_settings.dark_mode ? "Dark" : "Light");
if (selector.left.clicked || selector.right.clicked) {
    g_settings.dark_mode = !g_settings.dark_mode;
}
```

#### `draw_progress_bar`

```cpp
inline void draw_progress_bar(Rectangle rect, float ratio,
                              Color bg, Color fill, Color border_color,
                              float border_width = 3.0f, float bar_inset = 4.0f);
```

水平プログレスバー。ヘルスゲージ等に使用する。`ratio` は 0.0〜1.0。

```cpp
ui::draw_progress_bar(kHealthBarRect, gauge_.get_value() / 100.0f,
                      g_theme->hud_health_bg,
                      gauge_.get_value() >= 70.0f ? g_theme->health_high : g_theme->health_low,
                      g_theme->hud_health_border);
```

#### `draw_slider`

```cpp
inline float draw_slider(Rectangle row_rect, const char* label, const char* value_text,
                         float ratio, float track_left, float track_width,
                         int font_size = 22, float track_top_offset = 26.0f);
```

スライダー行を一括描画する。行背景 + ラベル + トラック + 塗り + つまみ + 値テキスト。

- `ratio`: 0.0〜1.0 の現在値
- `track_left`: トラック開始X座標（絶対座標）
- `track_width`: トラックの幅
- 戻り値: マウスがドラッグ中の場合は新しい ratio（0.0〜1.0）。ドラッグ中でなければ `-1.0f`

```cpp
// 設定画面での使用例
Rectangle rows[3];
ui::vstack(row_area, 48.0f, 12.0f, rows);

const float drag = ui::draw_slider(rows[0], "Note Speed",
    TextFormat("%.3f", g_settings.note_speed),
    (g_settings.note_speed - 0.020f) / (0.090f - 0.020f),
    548.0f, 630.0f);
if (drag >= 0.0f) {
    g_settings.note_speed = 0.020f + drag * (0.090f - 0.020f);
}
```

#### `draw_slider_relative`

```cpp
inline float draw_slider_relative(Rectangle row_rect, const char* label, const char* value_text,
                                  float ratio, float track_left_inset, float track_right_inset,
                                  int font_size = 22, float track_top_offset = 26.0f,
                                  float label_width = 200.0f, float content_padding = 18.0f);
```

`row_rect` を基準にトラックの左右インセットを相対指定するスライダー。設定画面のように同一レイアウトを複数行に並べる場合はこちらを推奨。

```cpp
ui::draw_slider_relative(row, "Note Speed", TextFormat("%.3f", g_settings.note_speed),
                         ratio, 218.0f, 42.0f);
```

#### `draw_scrollbar`

```cpp
inline void draw_scrollbar(Rectangle track_rect, float content_height, float scroll_offset,
                           Color track_color, Color thumb_color,
                           float min_thumb_height = 36.0f);
```

縦スクロールバーを描画する。サム位置計算は `vertical_scroll_metrics()` を内部で使用する。

```cpp
ui::draw_scrollbar(kScrollbarTrack, compute_content_height(), scroll_y_,
                   g_theme->scrollbar_track, g_theme->scrollbar_thumb);
```

#### `draw_header_block`

```cpp
inline void draw_header_block(Rectangle rect, const char* title, const char* subtitle,
                              int title_size = 34, int subtitle_size = 20,
                              float spacing = 8.0f);
```

タイトル + サブタイトルの見出しブロックを描画する。タイトル画面や設定画面のページヘッダ向け。

```cpp
ui::draw_header_block({48, 70, 320, 64}, "SETTINGS", "Saved on exit");
```

#### `draw_fullscreen_overlay`

```cpp
inline void draw_fullscreen_overlay(Color color);
```

画面全体（1280x720）を覆う半透明オーバーレイ。ポーズ画面やフェードに使用する。

```cpp
ui::draw_fullscreen_overlay(g_theme->pause_overlay);
```

---

## 移行ガイド

### 基本方針

1. **include を追加**: `#include "ui_draw.h"` を追加すれば全 UI ヘッダーが利用可能（ui_draw.h が他を include している）
2. **ハードコード座標をレイアウト関数で置換**: constexpr Rectangle の定義を `ui::place`, `ui::center`, `ui::inset` 等で書き換える
3. **手動テキスト中央揃えを `draw_text_in_rect` で置換**: `MeasureText` + 座標計算のパターンを除去
4. **手動ヒットテストを `ui::is_hovered` / `ui::is_clicked` で置換**: `CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), ...)` のパターンを除去
5. **描画パターンをユーティリティで置換**: ボタン・パネル・セクションの描画を `draw_button` / `draw_panel` / `draw_section` に集約
6. **中間粒度のUIを helper 化する**: 選択行や値セレクタを `draw_selectable_row` / `draw_value_selector` に寄せる
7. **スクロールやスライダーも親矩形基準で扱う**: `draw_slider_relative` と `scroll_view` / `draw_scrollbar` を優先する

### 移行の優先順序

1. `play_scene.cpp` — pause overlay（最小・自己完結）
2. `play_scene.cpp` — HUD（score, health, combo, fps）
3. `result_scene.cpp` — パネルベースのレイアウト
4. `title_scene.cpp` — シンプルなアンカー配置
5. `settings_scene.cpp` — サイドバー + タブ + 設定行
6. `song_select_scene.cpp` — 最も複雑（スクロールリスト、ジャケット画像）

### Before/After 例

#### play_scene.cpp: ポーズオーバーレイ

**Before（現在のコード）:**

```cpp
// ハードコードされた絶対座標
constexpr Rectangle kPausePanelRect      = {430.0f, 132.0f, 420.0f, 320.0f};
constexpr Rectangle kPauseResumeRect     = {470.0f, 220.0f, 340.0f, 42.0f};
constexpr Rectangle kPauseRestartRect    = {470.0f, 278.0f, 340.0f, 42.0f};
constexpr Rectangle kPauseSongSelectRect = {470.0f, 336.0f, 340.0f, 42.0f};

void play_scene::draw_pause_overlay() const {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, g_theme->pause_overlay);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    DrawRectangleRec(kPausePanelRect, g_theme->pause_panel);
    DrawRectangleLinesEx(kPausePanelRect, 2.0f, g_theme->border);
    DrawText("PAUSED",
             static_cast<int>(kPausePanelRect.x + 134.0f),
             static_cast<int>(kPausePanelRect.y + 24.0f), 42, g_theme->text);

    constexpr Rectangle buttons[] = {kPauseResumeRect, kPauseRestartRect, kPauseSongSelectRect};
    const char* labels[] = {"RESUME", "RESTART", "SONG SELECT"};
    for (int i = 0; i < 3; ++i) {
        const bool hovered = CheckCollisionPointRec(mouse, buttons[i]);
        const bool pressed = hovered && mouse_down;
        const Rectangle rect = pressed
            ? Rectangle{buttons[i].x + 1.5f, buttons[i].y + 1.5f,
                        buttons[i].width - 3.0f, buttons[i].height - 3.0f}
            : buttons[i];
        DrawRectangleRec(rect, lerp_color(g_theme->row, g_theme->row_hover, hovered ? 1.0f : 0.0f));
        DrawRectangleLinesEx(rect, 2.0f, g_theme->border);
        const int text_width = MeasureText(labels[i], 24);
        DrawText(labels[i],
                 static_cast<int>(rect.x + rect.width * 0.5f - text_width * 0.5f),
                 static_cast<int>(rect.y + 9.0f), 24, g_theme->text);
    }

    DrawText("ESC: Resume",
             static_cast<int>(kPausePanelRect.x + 24.0f),
             static_cast<int>(kPausePanelRect.y + 270.0f), 20, g_theme->text_muted);
}
```

**After（移行後のコード）:**

```cpp
#include "ui_draw.h"  // ui_layout.h, ui_text.h, ui_hit.h も含む

namespace {

constexpr Rectangle kScreen = {0, 0, kScreenWidth, kScreenHeight};

// パネル: 420x320、画面中央
constexpr Rectangle kPausePanel = ui::center(kScreen, 420.0f, 320.0f);

// タイトル領域: パネル上部、高さ 64px
constexpr Rectangle kPauseTitleRect = {
    kPausePanel.x, kPausePanel.y, kPausePanel.width, 64.0f
};

// ボタン領域: パネル内、タイトル下に配置
constexpr Rectangle kPauseButtonArea = {
    kPausePanel.x + 40.0f,
    kPausePanel.y + 88.0f,
    340.0f,
    3 * 42.0f + 2 * 16.0f
};

// ヒント領域: パネル下部
constexpr Rectangle kPauseHintRect = {
    kPausePanel.x + 24.0f,
    kPausePanel.y + kPausePanel.height - 50.0f,
    kPausePanel.width - 48.0f,
    30.0f
};

}  // namespace

void play_scene::draw_pause_overlay() const {
    ui::draw_fullscreen_overlay(g_theme->pause_overlay);

    // パネル
    ui::draw_panel(kPausePanel);

    // タイトル（中央揃え）
    ui::draw_text_in_rect("PAUSED", 42, kPauseTitleRect, g_theme->text);

    // ボタン（垂直スタック）
    Rectangle buttons[3];
    ui::vstack(kPauseButtonArea, 42.0f, 16.0f, buttons);

    const char* labels[] = {"RESUME", "RESTART", "SONG SELECT"};
    for (int i = 0; i < 3; ++i) {
        ui::draw_button(buttons[i], labels[i], 24);
    }

    // ヒント（左揃え）
    ui::draw_text_in_rect("ESC: Resume", 20, kPauseHintRect,
                          g_theme->text_muted, ui::text_align::left);
}
```

**変更点のまとめ:**

| Before | After |
|--------|-------|
| `{430.0f, 132.0f, 420.0f, 320.0f}` マジックナンバー | `ui::center(kScreen, 420, 320)` 意図が明確 |
| 3つの個別 constexpr Rectangle | `ui::vstack(area, 42, 16, buttons)` で自動計算 |
| `MeasureText` + 手動計算 | `ui::draw_text_in_rect()` |
| `CheckCollisionPointRec(mouse, ...)` | `ui::is_hovered()` / `ui::is_pressed()` |
| 手動座標計算 `{x+1.5, y+1.5, w-3, h-3}` | `ui::inset(rect, 1.5f)` |
| ボタン描画（hover/press/border/text の6行） | `ui::draw_button(rect, label, size)` 1行 |
| パネル描画 `DrawRectangleRec` + `DrawRectangleLinesEx` | `ui::draw_panel(rect)` |
| セクション描画（section背景 + border_light） | `ui::draw_section(rect)` |
| ヘルスゲージ描画（背景+ボーダー+塗り） | `ui::draw_progress_bar(rect, ratio, ...)` |

#### play_scene.cpp: HUD

**Before:**

```cpp
void play_scene::draw_hud() const {
    const result_data result = score_system_.get_result_data();
    const std::string time_text = TextFormat("%.2f", current_ms_ / 1000.0);
    const std::string fps_text = TextFormat("FPS: %d", GetFPS());
    constexpr int score_left = 48;
    constexpr int score_top = 34;
    constexpr int score_height = 30;
    constexpr int health_left = kScreenWidth - 308;
    constexpr int health_top = 58;
    constexpr int health_width = 260;
    constexpr int health_height = 24;
    constexpr int inset = 4.0f;
    const int fill_width = (health_width - inset * 2) * (gauge_.get_value() / 100.0f);

    DrawText(TextFormat("SCORE %07d", result.score), score_left, score_top, 30, g_theme->hud_score);
    DrawText(TextFormat("Accuracy %.2f %%", result.accuracy), score_left, score_top + score_height, 22, g_theme->hud_score);
    DrawText(fps_text.c_str(), kScreenWidth - MeasureText(fps_text.c_str(), 20) - 10, kScreenHeight - 20, 20, g_theme->hud_fps);
    DrawText(time_text.c_str(), kScreenWidth / 2 - MeasureText(time_text.c_str(), 30) / 2, 34, 30, g_theme->hud_time);
    // ... health gauge, combo 省略
}
```

**After:**

```cpp
#include "ui_draw.h"

namespace {

constexpr Rectangle kScreen = {0, 0, kScreenWidth, kScreenHeight};

// スコア領域（左上）
constexpr Rectangle kScoreRect = ui::place(kScreen, 400, 60,
    ui::anchor::top_left, ui::anchor::top_left, {48, 34});

// 時間表示（上部中央）
constexpr Rectangle kTimeRect = ui::place(kScreen, 200, 30,
    ui::anchor::top_center, ui::anchor::top_center, {0, 34});

// FPS（右下）
constexpr Rectangle kFpsRect = ui::place(kScreen, 120, 20,
    ui::anchor::bottom_right, ui::anchor::bottom_right, {-10, 0});

// ヘルスゲージ（右上）
constexpr Rectangle kHealthLabelRect = ui::place(kScreen, 100, 24,
    ui::anchor::top_right, ui::anchor::top_right, {-48, 34});
constexpr Rectangle kHealthBarRect = ui::place(kScreen, 260, 24,
    ui::anchor::top_right, ui::anchor::top_right, {-48, 58});

// コンボ（中央）
constexpr Rectangle kComboNumberRect = ui::place(kScreen, 300, 86,
    ui::anchor::center, ui::anchor::center, {0, -80});
constexpr Rectangle kComboLabelRect = ui::place(kScreen, 200, 24,
    ui::anchor::center, ui::anchor::center, {0, 0});

}  // namespace

void play_scene::draw_hud() const {
    const result_data result = score_system_.get_result_data();

    // スコアと精度
    Rectangle score_rows[2];
    ui::vstack(kScoreRect, 30, 0, score_rows);
    ui::draw_text_in_rect(TextFormat("SCORE %07d", result.score), 30,
                          score_rows[0], g_theme->hud_score, ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("Accuracy %.2f %%", result.accuracy), 22,
                          score_rows[1], g_theme->hud_score, ui::text_align::left);

    // FPS（右揃え）
    ui::draw_text_in_rect(TextFormat("FPS: %d", GetFPS()), 20,
                          kFpsRect, g_theme->hud_fps, ui::text_align::right);

    // 経過時間（中央揃え）
    ui::draw_text_in_rect(TextFormat("%.2f", current_ms_ / 1000.0), 30,
                          kTimeRect, g_theme->hud_time);

    // ヘルスゲージ（draw_progress_bar で一括描画）
    ui::draw_text_in_rect("HEALTH", 24, kHealthLabelRect,
                          g_theme->hud_health_label, ui::text_align::right);
    ui::draw_progress_bar(kHealthBarRect, gauge_.get_value() / 100.0f,
                          g_theme->hud_health_bg,
                          gauge_.get_value() >= 70.0f ? g_theme->health_high : g_theme->health_low,
                          g_theme->hud_health_border);

    // コンボ
    if (combo_display_ > 0) {
        ui::draw_text_in_rect(TextFormat("%03d", combo_display_), 86,
                              kComboNumberRect, g_theme->hud_combo);
        ui::draw_text_in_rect("COMBO", 24, kComboLabelRect, g_theme->hud_combo);
    }
}
```

#### result_scene.cpp: 結果画面

**Before:**

```cpp
constexpr Rectangle kMainPanel   = {24.0f, 24.0f, 1232.0f, 672.0f};
constexpr Rectangle kSongInfoRect = {48.0f, 48.0f, 580.0f, 108.0f};
constexpr Rectangle kRankRect    = {660.0f, 48.0f, 200.0f, 168.0f};
constexpr Rectangle kScoreRect   = {48.0f, 180.0f, 580.0f, 108.0f};
constexpr Rectangle kJudgeRect   = {48.0f, 312.0f, 580.0f, 260.0f};
constexpr Rectangle kStatsRect   = {660.0f, 240.0f, 572.0f, 332.0f};
```

**After:**

```cpp
#include "ui_draw.h"

constexpr Rectangle kScreen = {0, 0, kScreenWidth, kScreenHeight};

// メインパネル: 画面全体を 24px インセット
constexpr Rectangle kMainPanel = ui::inset(kScreen, 24.0f);

// パネル内のコンテンツ領域
constexpr Rectangle kContent = ui::inset(kMainPanel, 24.0f);

// 左列 (楽曲情報 + スコア + 判定)
constexpr Rectangle kLeftCol = {kContent.x, kContent.y, 580.0f, kContent.height};

// 右列 (ランク + 統計)
constexpr Rectangle kRightCol = {
    kContent.x + 580.0f + 32.0f, kContent.y,
    kContent.width - 580.0f - 32.0f, kContent.height
};

// 左列内の各セクション
constexpr Rectangle kSongInfoRect = {kLeftCol.x, kLeftCol.y, kLeftCol.width, 108.0f};
constexpr Rectangle kScoreRect = {kLeftCol.x, kLeftCol.y + 132.0f, kLeftCol.width, 108.0f};
constexpr Rectangle kJudgeRect = {kLeftCol.x, kLeftCol.y + 264.0f, kLeftCol.width, 260.0f};

// 右列内の各セクション
constexpr Rectangle kRankRect = {kRightCol.x, kRightCol.y, 200.0f, 168.0f};
constexpr Rectangle kStatsRect = {kRightCol.x, kRightCol.y + 192.0f, kRightCol.width, 332.0f};

// draw() 内での使用例:
// ui::draw_panel(kMainPanel);                     // パネル描画 (2行→1行)
// ui::draw_section(kSongInfoRect);                // セクション描画 (2行→1行)
// ui::draw_section(kRankRect);
// ui::draw_text_in_rect(rlabel, 96, kRankRect, rcolor);  // ランク文字の中央揃え
//
// // 統計行の描画 (vstack + draw_label_value)
// Rectangle stat_rows[5];
// ui::vstack(ui::inset(kStatsRect, 24.0f), 40.0f, 0.0f, stat_rows);
// ui::draw_label_value(stat_rows[0], "Max Combo", TextFormat("%d", result_.max_combo),
//                      24, t.text_dim, t.text);
// ui::draw_label_value(stat_rows[1], "Avg Offset", TextFormat("%.1f ms", result_.avg_offset),
//                      24, t.text_dim, t.text);
```

#### settings_scene.cpp: 設定画面

**Before:**

```cpp
constexpr Rectangle kSidebarRect = {24.0f, 44.0f, 256.0f, 660.0f};
constexpr Rectangle kContentRect = {300.0f, 44.0f, 956.0f, 660.0f};
constexpr Rectangle kTabRects[kPageCount] = {
    {48.0f, 196.0f, 208.0f, 42.0f},
    {48.0f, 246.0f, 208.0f, 42.0f},
    {48.0f, 296.0f, 208.0f, 42.0f},
    {48.0f, 346.0f, 208.0f, 42.0f},
};
constexpr Rectangle kBackRect = {48.0f, 622.0f, 208.0f, 42.0f};
constexpr Rectangle kGeneralRows[] = {
    {330.0f, 160.0f, 890.0f, 48.0f},
    {330.0f, 220.0f, 890.0f, 48.0f},
    // ... 8行のハードコード
};
```

**After:**

```cpp
constexpr Rectangle kScreen = {0, 0, kScreenWidth, kScreenHeight};

// サイドバー + コンテンツの2カラムレイアウト
constexpr Rectangle kSidebarRect = ui::place(kScreen, 256, 660,
    ui::anchor::top_left, ui::anchor::top_left, {24, 44});
constexpr Rectangle kContentRect = ui::place(kScreen, 956, 660,
    ui::anchor::top_left, ui::anchor::top_left, {300, 44});

// サイドバー内のタブ領域
constexpr Rectangle kTabArea = ui::place(kSidebarRect, 208, 4 * 42 + 3 * 8,
    ui::anchor::top_center, ui::anchor::top_center, {0, 152});

// BACK ボタン
constexpr Rectangle kBackRect = ui::place(kSidebarRect, 208, 42,
    ui::anchor::bottom_center, ui::anchor::bottom_center, {0, -38});

// draw 内で動的に計算
void settings_scene::draw_tabs() {
    Rectangle tabs[kPageCount];
    ui::vstack(kTabArea, 42.0f, 8.0f, tabs);
    for (int i = 0; i < kPageCount; ++i) {
        // 選択中/非選択でボタン色を分ける
        if (i == current_page_) {
            ui::draw_button_colored(tabs[i], kPageNames[i], 22,
                                    g_theme->row_active, g_theme->row_active, g_theme->text);
        } else {
            if (ui::draw_button(tabs[i], kPageNames[i], 22).clicked) {
                current_page_ = i;
            }
        }
    }
    // BACK ボタン
    if (ui::draw_button(kBackRect, "BACK", 22).clicked) {
        // シーン遷移
    }
}

void settings_scene::draw_general_page() {
    constexpr Rectangle row_area = ui::inset(kContentRect, ui::edge_insets{116, 30, 0, 30});
    Rectangle rows[8];
    ui::vstack(row_area, 48.0f, 12.0f, rows);
    // rows[0], rows[1], ... で設定行を描画
    // スライダー行の例:
    // ui::draw_label_value(rows[0], "Note Speed", TextFormat("%.3f", g_settings.note_speed),
    //                      24, g_theme->text_dim, g_theme->text);
}
```

---

## 移行時の注意事項

### include の追加

移行するシーンファイルの先頭にヘッダーを追加する。`ui_draw.h` が全ヘッダーを include しているため、通常はこれだけでよい:

```cpp
#include "ui_draw.h"     // ui_layout.h, ui_text.h, ui_hit.h を含む
```

描画ユーティリティが不要な場合（レイアウト計算のみ）は個別に include も可能:

```cpp
#include "ui_layout.h"   // レイアウト関数のみ
```

### constexpr の制約

- `ui::place`, `ui::center`, `ui::inset` は **constexpr** なので、`constexpr Rectangle` の初期化に使用可能
- `ui::vstack`, `ui::hstack`, `ui::grid` は **std::span** を使うため `constexpr` 不可。`draw()` 関数内でローカル配列に書き込む形で使用する
- `ui::text_position`, `ui::draw_text_in_rect` は `MeasureText()` を呼ぶため `constexpr` 不可

### 既存パターンの置換ルール

| 既存パターン | 置換先 |
|-------------|--------|
| `constexpr Rectangle r = {x, y, w, h};`（絶対座標） | `ui::place(parent, w, h, anchor, ...)` または `ui::inset(parent, ...)` |
| `kScreenWidth / 2 - MeasureText(text, size) / 2` | `ui::draw_text_in_rect(text, size, rect, color)` |
| `rect.x + rect.width * 0.5f - text_width * 0.5f` | `ui::draw_text_in_rect(text, size, rect, color)` |
| `CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), rect)` | `ui::is_hovered(rect)` |
| `is_hovered && IsMouseButtonDown(...)` | `ui::is_pressed(rect)` |
| `is_hovered && IsMouseButtonReleased(...)` | `ui::is_clicked(rect)` |
| `{rect.x + n, rect.y + n, rect.w - 2*n, rect.h - 2*n}` | `ui::inset(rect, n)` |
| 複数の固定高さ Rectangle の羅列 | `ui::vstack(area, height, spacing, out)` |
| 1行をラベル列 + 値列に分ける手計算 | `ui::split_columns(rect, width, spacing)` |
| タイトル + サブタイトルの上下分割 | `ui::split_rows(rect, height, spacing)` |
| `x + col * (w + gap)` 的なグリッド計算 | `ui::grid(parent, cols, w, h, hgap, vgap, out)` |
| スライダー行（行背景+ラベル+トラック+つまみ+値の15行超） | `ui::draw_slider(row, label, value, ratio, ...)` |
| スライダーのトラック位置を絶対座標で持つ | `ui::draw_slider_relative(row, label, value, ratio, left_inset, right_inset)` |
| `< value >` 型の設定行 | `ui::draw_value_selector(rect, label, value)` |
| hover / selected / pressed を持つ行背景 | `ui::draw_row(...)` / `ui::draw_selectable_row(...)` |
| スクロール表示領域とスクロールバーの手計算 | `ui::scroll_view(...)` / `ui::vertical_scroll_metrics(...)` / `ui::draw_scrollbar(...)` |
| `DrawRectangle(0, 0, kScreenWidth, kScreenHeight, overlay_color)` | `ui::draw_fullscreen_overlay(color)` |

### update() 内のヒットテスト移行

`draw()` だけでなく `update()` 内でもマウス判定を行っている場合がある。
`ui::is_clicked()` は `update()` でも使用可能（`IsMouseButtonReleased` はフレーム単位の判定）。

```cpp
// Before (update 内)
const Vector2 mouse = virtual_screen::get_virtual_mouse();
if (CheckCollisionPointRec(mouse, kSettingsButtonRect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    // ...
}

// After
if (ui::is_clicked(kSettingsButtonRect)) {
    // ...
}
```

### Raylib 描画関数の直接使用

`ui_draw.h` でカバーされないケースでは、引き続き Raylib の描画関数を直接使用する:

- `DrawRectangleGradientV` — グラデーション背景
- `DrawRectangleRounded` / `DrawRectangleRoundedLinesEx` — 角丸矩形
- `DrawTexturePro` — テクスチャ描画（ジャケット画像等）
- `BeginScissorMode` / `EndScissorMode` — クリッピング

これらに渡す `Rectangle` や座標値をレイアウト関数で算出する形になる。

### 3D 描画は対象外

`play_scene.cpp` の 3D レンダリング（DrawCube, DrawCubeWires, Camera3D 関連）はレイアウトシステムの対象外。
HUD（2D オーバーレイ）のみが移行対象。
