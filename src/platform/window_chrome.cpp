#include "window_chrome.h"

#include <algorithm>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define LoadImage Win32LoadImage
#define DrawText Win32DrawText
#define DrawTextEx Win32DrawTextEx
#include <windows.h>
#include <windowsx.h>
#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#endif

#include "raylib.h"
#include "core/window_dialog_support.h"
#include "game_settings.h"
#include "scene_manager.h"
#include "theme.h"
#include "ui/ui_coord.h"
#include "ui/ui_text.h"

namespace window_chrome {
namespace {

constexpr int kTitlebarHeight = 36;
constexpr int kButtonWidth = 46;
constexpr int kCaptionLeftPadding = 14;
constexpr int kButtonCount = 3;
constexpr int kMinWindowWidth = 640;
constexpr int kMinWindowHeight = 360 + kTitlebarHeight;
bool g_enabled = false;
float g_button_hover_t[kButtonCount] = {};
float g_button_press_t[kButtonCount] = {};
float g_maximize_icon_t = 0.0f;

#ifdef _WIN32
struct window_rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

window_rect g_animation_from = {};
window_rect g_animation_to = {};
float g_animation_elapsed = 0.0f;
float g_animation_duration = 0.16f;
bool g_animation_active = false;
bool g_animation_target_maximized = false;
bool g_committing_native_maximize = false;

enum class resize_edge {
    none,
    left,
    right,
    top,
    bottom,
    top_left,
    top_right,
    bottom_left,
    bottom_right,
};

resize_edge g_resize_edge = resize_edge::none;
window_rect g_resize_start_rect = {};
POINT g_resize_start_cursor = {};
bool g_move_active = false;
window_rect g_move_start_rect = {};
POINT g_move_start_cursor = {};
#endif

Rectangle titlebar_rect() {
    return {0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(kTitlebarHeight)};
}

Rectangle button_rect(int button_from_right) {
    return {
        static_cast<float>(GetScreenWidth() - (button_from_right + 1) * kButtonWidth),
        0.0f,
        static_cast<float>(kButtonWidth),
        static_cast<float>(kTitlebarHeight)
    };
}

bool clicked(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool hovered(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect);
}

bool pressed(Rectangle rect) {
    return hovered(rect) && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

Color blend(Color from, Color to, float amount) {
    return lerp_color(from, to, amount);
}

float approach(float value, float target, float speed, float dt) {
    const float t = 1.0f - std::exp(-speed * std::max(0.0f, dt));
    return value + (target - value) * t;
}

Color alpha_color(Color color, float alpha) {
    return with_alpha(color, static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(color.a)));
}

void tick_button_animation(int button_from_right, float dt) {
    const Rectangle rect = button_rect(button_from_right);
    g_button_hover_t[button_from_right] =
        approach(g_button_hover_t[button_from_right], hovered(rect) ? 1.0f : 0.0f, 18.0f, dt);
    g_button_press_t[button_from_right] =
        approach(g_button_press_t[button_from_right], pressed(rect) ? 1.0f : 0.0f, 28.0f, dt);
}

void tick_animations(float dt) {
    for (int i = 0; i < kButtonCount; ++i) {
        tick_button_animation(i, dt);
    }
    g_maximize_icon_t = approach(g_maximize_icon_t, is_maximized() ? 1.0f : 0.0f, 20.0f, dt);
}

void draw_button_background(Rectangle rect, int button_from_right, bool close_button) {
    const float hover_t = g_button_hover_t[button_from_right];
    const float press_t = g_button_press_t[button_from_right];
    const Color hover_fill = close_button ? blend(g_theme->panel, g_theme->error, 0.72f)
                                          : blend(g_theme->panel, g_theme->row_hover, 0.72f);
    const Color press_fill = close_button ? blend(g_theme->panel, g_theme->error, 0.9f)
                                          : blend(g_theme->panel, g_theme->row_active, 0.88f);
    Color fill = blend(blend(g_theme->panel, hover_fill, hover_t), press_fill, press_t);
    ui::draw_rect_f(rect, fill);
}

void draw_minimize_icon(Rectangle rect, Color color) {
    const float y = rect.y + rect.height * 0.62f;
    ui::draw_line_ex({rect.x + 17.0f, y}, {rect.x + rect.width - 17.0f, y}, 2.0f, color);
}

void draw_maximize_icon(Rectangle rect, Color color) {
    DrawRectangleLinesEx({rect.x + 16.0f, rect.y + 11.0f, 14.0f, 14.0f}, 2.0f, color);
}

void draw_restore_icon(Rectangle rect, Color color) {
    DrawRectangleLinesEx({rect.x + 16.0f, rect.y + 12.0f, 12.0f, 10.0f}, 2.0f, color);
    DrawRectangleLinesEx({rect.x + 20.0f, rect.y + 16.0f, 12.0f, 10.0f}, 2.0f, color);
}

void draw_maximize_restore_icon(Rectangle rect, Color color) {
    draw_maximize_icon(rect, alpha_color(color, 1.0f - g_maximize_icon_t));
    draw_restore_icon(rect, alpha_color(color, g_maximize_icon_t));
}

void draw_close_icon(Rectangle rect, Color color) {
    ui::draw_line_ex({rect.x + 17.0f, rect.y + 12.0f}, {rect.x + rect.width - 17.0f, rect.y + 24.0f}, 2.0f, color);
    ui::draw_line_ex({rect.x + rect.width - 17.0f, rect.y + 12.0f}, {rect.x + 17.0f, rect.y + 24.0f}, 2.0f, color);
}

#ifdef _WIN32

HWND g_hwnd = nullptr;
WNDPROC g_previous_wnd_proc = nullptr;
bool g_installed = false;

window_rect rect_from_win32(RECT rect) {
    return {
        static_cast<int>(rect.left),
        static_cast<int>(rect.top),
        static_cast<int>(rect.right - rect.left),
        static_cast<int>(rect.bottom - rect.top)
    };
}

window_rect current_window_rect() {
    RECT rect{};
    if (g_hwnd == nullptr || GetWindowRect(g_hwnd, &rect) == 0) {
        return {};
    }
    return rect_from_win32(rect);
}

window_rect current_monitor_work_rect() {
    RECT fallback = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    if (g_hwnd == nullptr) {
        return rect_from_win32(fallback);
    }

    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR monitor = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr || GetMonitorInfoW(monitor, &monitor_info) == 0) {
        return rect_from_win32(fallback);
    }
    return rect_from_win32(monitor_info.rcWork);
}

window_rect centered_windowed_rect() {
    const window_rect work = current_monitor_work_rect();
    const int width = kDefaultWindowedWidth;
    const int height = kDefaultWindowedHeight + kTitlebarHeight;
    return {
        work.x + std::max(0, work.width - width) / 2,
        work.y + std::max(0, work.height - height) / 2,
        std::min(width, work.width),
        std::min(height, work.height)
    };
}

int lerp_int(int from, int to, float t) {
    return static_cast<int>(std::lround(static_cast<float>(from) + static_cast<float>(to - from) * t));
}

float ease_window_t(float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return c * c * (3.0f - 2.0f * c);
}

void apply_window_rect(window_rect rect) {
    if (g_hwnd == nullptr) {
        return;
    }
    SetWindowPos(g_hwnd, nullptr, rect.x, rect.y, rect.width, rect.height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void start_window_animation(window_rect target, bool target_maximized) {
    if (g_hwnd == nullptr) {
        return;
    }
    g_animation_from = current_window_rect();
    if (IsZoomed(g_hwnd)) {
        ShowWindow(g_hwnd, SW_RESTORE);
    }
    g_animation_to = target;
    g_animation_elapsed = 0.0f;
    g_animation_duration = 0.16f;
    g_animation_active = true;
    g_animation_target_maximized = target_maximized;
}

void tick_window_animation(float dt) {
    if (!g_animation_active) {
        return;
    }

    g_animation_elapsed += std::max(0.0f, dt);
    const float t = ease_window_t(g_animation_elapsed / std::max(0.001f, g_animation_duration));
    const window_rect next = {
        lerp_int(g_animation_from.x, g_animation_to.x, t),
        lerp_int(g_animation_from.y, g_animation_to.y, t),
        lerp_int(g_animation_from.width, g_animation_to.width, t),
        lerp_int(g_animation_from.height, g_animation_to.height, t)
    };
    apply_window_rect(next);

    if (g_animation_elapsed >= g_animation_duration) {
        g_animation_active = false;
        apply_window_rect(g_animation_to);
        if (g_animation_target_maximized && g_hwnd != nullptr && !IsWindowFullscreen()) {
            g_committing_native_maximize = true;
            ShowWindow(g_hwnd, SW_MAXIMIZE);
            g_committing_native_maximize = false;
        }
    }
}

int resize_border_px() {
    return std::max(6, GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER));
}

resize_edge resize_edge_at_point(int x, int y, window_rect rect) {
    const int border = resize_border_px();
    const bool left = x >= rect.x && x < rect.x + border;
    const bool right = x < rect.x + rect.width && x >= rect.x + rect.width - border;
    const bool top = y >= rect.y && y < rect.y + border;
    const bool bottom = y < rect.y + rect.height && y >= rect.y + rect.height - border;

    if (top && left) {
        return resize_edge::top_left;
    }
    if (top && right) {
        return resize_edge::top_right;
    }
    if (bottom && left) {
        return resize_edge::bottom_left;
    }
    if (bottom && right) {
        return resize_edge::bottom_right;
    }
    if (left) {
        return resize_edge::left;
    }
    if (right) {
        return resize_edge::right;
    }
    if (top) {
        return resize_edge::top;
    }
    if (bottom) {
        return resize_edge::bottom;
    }
    return resize_edge::none;
}

bool is_resizing_left(resize_edge edge) {
    return edge == resize_edge::left || edge == resize_edge::top_left || edge == resize_edge::bottom_left;
}

bool is_resizing_right(resize_edge edge) {
    return edge == resize_edge::right || edge == resize_edge::top_right || edge == resize_edge::bottom_right;
}

bool is_resizing_top(resize_edge edge) {
    return edge == resize_edge::top || edge == resize_edge::top_left || edge == resize_edge::top_right;
}

bool is_resizing_bottom(resize_edge edge) {
    return edge == resize_edge::bottom || edge == resize_edge::bottom_left || edge == resize_edge::bottom_right;
}

int cursor_for_resize_edge(resize_edge edge) {
    switch (edge) {
        case resize_edge::left:
        case resize_edge::right:
            return MOUSE_CURSOR_RESIZE_EW;
        case resize_edge::top:
        case resize_edge::bottom:
            return MOUSE_CURSOR_RESIZE_NS;
        case resize_edge::top_left:
        case resize_edge::bottom_right:
            return MOUSE_CURSOR_RESIZE_NWSE;
        case resize_edge::top_right:
        case resize_edge::bottom_left:
            return MOUSE_CURSOR_RESIZE_NESW;
        case resize_edge::none:
        default:
            return MOUSE_CURSOR_DEFAULT;
    }
}

bool point_in_caption_area(int x, int y, window_rect rect) {
    const int local_x = x - rect.x;
    const int local_y = y - rect.y;
    const int button_area_left = std::max(0, rect.width - kButtonWidth * 3);
    return local_y >= 0 && local_y < kTitlebarHeight &&
           local_x >= kCaptionLeftPadding && local_x < button_area_left;
}

void update_cursor() {
    if (g_resize_edge != resize_edge::none) {
        SetMouseCursor(cursor_for_resize_edge(g_resize_edge));
        return;
    }
    if (g_move_active || is_maximized() || IsWindowFullscreen()) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return;
    }
    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return;
    }
    SetMouseCursor(cursor_for_resize_edge(resize_edge_at_point(cursor.x, cursor.y, current_window_rect())));
}

void start_manual_resize(resize_edge edge) {
    if (edge == resize_edge::none || g_hwnd == nullptr || is_maximized() || IsWindowFullscreen()) {
        return;
    }
    g_animation_active = false;
    g_animation_target_maximized = false;
    g_resize_edge = edge;
    g_resize_start_rect = current_window_rect();
    GetCursorPos(&g_resize_start_cursor);
}

void update_manual_resize() {
    if (g_resize_edge == resize_edge::none || g_hwnd == nullptr) {
        return;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_resize_edge = resize_edge::none;
        return;
    }

    POINT cursor{};
    GetCursorPos(&cursor);
    const int dx = cursor.x - g_resize_start_cursor.x;
    const int dy = cursor.y - g_resize_start_cursor.y;

    window_rect next = g_resize_start_rect;
    if (is_resizing_left(g_resize_edge)) {
        const int right = g_resize_start_rect.x + g_resize_start_rect.width;
        next.x = std::min(g_resize_start_rect.x + dx, right - kMinWindowWidth);
        next.width = right - next.x;
    }
    if (is_resizing_right(g_resize_edge)) {
        next.width = std::max(kMinWindowWidth, g_resize_start_rect.width + dx);
    }
    if (is_resizing_top(g_resize_edge)) {
        const int bottom = g_resize_start_rect.y + g_resize_start_rect.height;
        next.y = std::min(g_resize_start_rect.y + dy, bottom - kMinWindowHeight);
        next.height = bottom - next.y;
    }
    if (is_resizing_bottom(g_resize_edge)) {
        next.height = std::max(kMinWindowHeight, g_resize_start_rect.height + dy);
    }
    apply_window_rect(next);
}

void start_manual_move() {
    if (g_hwnd == nullptr || IsWindowFullscreen()) {
        return;
    }
    g_animation_active = false;
    g_animation_target_maximized = false;
    const bool was_maximized = is_maximized();
    if (IsZoomed(g_hwnd)) {
        ShowWindow(g_hwnd, SW_RESTORE);
    }
    if (was_maximized) {
        const window_rect work = current_monitor_work_rect();
        POINT cursor{};
        GetCursorPos(&cursor);
        const int width = kDefaultWindowedWidth;
        const int height = kDefaultWindowedHeight + kTitlebarHeight;
        const int cursor_x = static_cast<int>(cursor.x);
        const int cursor_y = static_cast<int>(cursor.y);
        const int grip_x = std::clamp(cursor_x - work.x, 160, width - 160);
        const int target_x = std::clamp(cursor_x - grip_x, work.x, work.x + std::max(0, work.width - width));
        const int target_y = std::clamp(cursor_y - kTitlebarHeight / 2, work.y, work.y + std::max(0, work.height - height));
        apply_window_rect({target_x, target_y, std::min(width, work.width), std::min(height, work.height)});
        g_settings.window_maximized = false;
    }
    g_move_active = true;
    g_move_start_rect = current_window_rect();
    GetCursorPos(&g_move_start_cursor);
}

void update_manual_move() {
    if (!g_move_active || g_hwnd == nullptr) {
        return;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        POINT cursor{};
        if (GetCursorPos(&cursor)) {
            const window_rect work = current_monitor_work_rect();
            if (cursor.y <= work.y + 2) {
                g_settings.window_maximized = true;
                start_window_animation(work, true);
            }
        }
        g_move_active = false;
        return;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }
    const window_rect next = {
        g_move_start_rect.x + cursor.x - g_move_start_cursor.x,
        g_move_start_rect.y + cursor.y - g_move_start_cursor.y,
        g_move_start_rect.width,
        g_move_start_rect.height
    };
    apply_window_rect(next);
}

void stop_manual_window_interaction() {
    g_resize_edge = resize_edge::none;
    g_move_active = false;
}

LRESULT hit_test(HWND hwnd, LPARAM l_param) {
    if (IsWindowFullscreen()) {
        return HTCLIENT;
    }

    RECT rect{};
    if (GetWindowRect(hwnd, &rect) == 0) {
        return HTCLIENT;
    }

    const int x = GET_X_LPARAM(l_param);
    const int y = GET_Y_LPARAM(l_param);
    return HTCLIENT;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message == WM_NCCALCSIZE && w_param == TRUE) {
        return 0;
    }
    if ((message == WM_ENTERSIZEMOVE || message == WM_SIZING) && !g_committing_native_maximize) {
        g_animation_active = false;
        g_animation_target_maximized = false;
        stop_manual_window_interaction();
    }
    if (message == WM_SYSCOMMAND && (w_param & 0xfff0) == SC_MAXIMIZE &&
        !g_committing_native_maximize && !IsWindowFullscreen()) {
        g_settings.window_maximized = true;
        start_window_animation(current_monitor_work_rect(), true);
        return 0;
    }
    if (message == WM_NCLBUTTONDBLCLK && w_param == HTCAPTION) {
        if (is_maximized()) {
            g_settings.window_maximized = false;
            start_window_animation(centered_windowed_rect(), false);
        } else {
            g_settings.window_maximized = true;
            start_window_animation(current_monitor_work_rect(), true);
        }
        return 0;
    }
    if (message == WM_NCHITTEST) {
        const LRESULT handled = hit_test(hwnd, l_param);
        if (handled != HTCLIENT) {
            return handled;
        }
    }
    return CallWindowProcW(g_previous_wnd_proc, hwnd, message, w_param, l_param);
}

void apply_borderless_resizable_style(HWND hwnd) {
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_CAPTION);
    style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

#endif

}  // namespace

bool initialize(void* native_window_handle) {
#ifdef _WIN32
    if (g_installed) {
        g_enabled = true;
        return true;
    }
    if (native_window_handle == nullptr) {
        g_enabled = false;
        return false;
    }

    g_hwnd = static_cast<HWND>(native_window_handle);
    apply_borderless_resizable_style(g_hwnd);
    SetLastError(0);
    g_previous_wnd_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wnd_proc)));
    if (g_previous_wnd_proc == nullptr && GetLastError() != 0) {
        g_hwnd = nullptr;
        g_enabled = false;
        return false;
    }
    g_installed = true;
    g_enabled = true;
    return true;
#else
    (void)native_window_handle;
    g_enabled = false;
    return false;
#endif
}

void shutdown() {
#ifdef _WIN32
    if (g_installed && g_hwnd != nullptr) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_previous_wnd_proc));
    }
    g_hwnd = nullptr;
    g_previous_wnd_proc = nullptr;
    g_installed = false;
#endif
    g_enabled = false;
}

void update(scene_manager& manager) {
    if (!g_enabled) {
        return;
    }
    const float dt = GetFrameTime();
    tick_animations(dt);
    tick_window_animation(dt);
    update_cursor();
    if (g_move_active) {
        update_manual_move();
        return;
    }
    if (g_resize_edge != resize_edge::none) {
        update_manual_resize();
        return;
    }
    if (IsWindowFullscreen()) {
        return;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !is_maximized()) {
        POINT cursor{};
        if (GetCursorPos(&cursor)) {
            start_manual_resize(resize_edge_at_point(cursor.x, cursor.y, current_window_rect()));
            if (g_resize_edge != resize_edge::none) {
                update_manual_resize();
                return;
            }
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        POINT cursor{};
        const window_rect rect = current_window_rect();
        if (GetCursorPos(&cursor) && point_in_caption_area(cursor.x, cursor.y, rect)) {
            start_manual_move();
            update_manual_move();
            return;
        }
    }
    if (clicked(button_rect(2))) {
        minimize();
    }
    if (clicked(button_rect(1))) {
        toggle_maximize_restore();
    }
    if (clicked(button_rect(0))) {
        manager.request_exit();
    }
}

void draw() {
    if (!g_enabled || IsWindowFullscreen()) {
        return;
    }
    const Rectangle bar = titlebar_rect();
    ui::draw_rect_f(bar, g_theme->panel);
    ui::draw_rect_f({0.0f, bar.height - 2.0f, bar.width, 2.0f}, g_theme->border);
    ui::draw_rect_f({0.0f, 0.0f, 4.0f, bar.height}, g_theme->accent);

    const Color text_color = g_theme->text;
    const Color muted = g_theme->text_secondary;
    ui::draw_text_f("raythm", 28.0f, 8.0f, 16, text_color);
    ui::draw_rect_f({12.0f, 10.0f, 3.0f, 16.0f}, g_theme->accent);
    ui::draw_rect_f({17.0f, 14.0f, 3.0f, 8.0f}, g_theme->fast);
    ui::draw_rect_f({7.0f, 14.0f, 3.0f, 8.0f}, g_theme->accent);

    const Rectangle minimize_rect = button_rect(2);
    const Rectangle maximize_rect = button_rect(1);
    const Rectangle close_rect = button_rect(0);
    draw_button_background(minimize_rect, 2, false);
    draw_button_background(maximize_rect, 1, false);
    draw_button_background(close_rect, 0, true);

    draw_minimize_icon(minimize_rect, hovered(minimize_rect) ? text_color : muted);
    draw_maximize_restore_icon(maximize_rect, hovered(maximize_rect) ? text_color : muted);
    draw_close_icon(close_rect, hovered(close_rect) ? WHITE : muted);
}

int titlebar_height_px() {
    return kTitlebarHeight;
}

bool is_maximized() {
#ifdef _WIN32
    return g_hwnd != nullptr && (IsZoomed(g_hwnd) != 0 || (g_animation_active && g_animation_target_maximized));
#else
    return false;
#endif
}

bool is_state_transitioning() {
#ifdef _WIN32
    return g_animation_active;
#else
    return false;
#endif
}

void minimize() {
#ifdef _WIN32
    if (g_hwnd != nullptr) {
        g_animation_active = false;
        ShowWindow(g_hwnd, SW_MINIMIZE);
    }
#endif
}

void maximize() {
#ifdef _WIN32
    if (g_hwnd != nullptr) {
        g_animation_active = false;
        g_committing_native_maximize = true;
        ShowWindow(g_hwnd, SW_MAXIMIZE);
        g_committing_native_maximize = false;
    }
#endif
}

void toggle_maximize_restore() {
#ifdef _WIN32
    if (g_hwnd != nullptr) {
        if (is_maximized()) {
            g_settings.window_maximized = false;
            start_window_animation(centered_windowed_rect(), false);
        } else {
            g_settings.window_maximized = true;
            start_window_animation(current_monitor_work_rect(), true);
        }
    }
#endif
}

}  // namespace window_chrome
