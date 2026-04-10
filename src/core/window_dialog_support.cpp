#include "window_dialog_support.h"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

extern "C" {
void* GetWindowHandle(void);
bool IsWindowFullscreen(void);
void ToggleFullscreen(void);
void SetWindowSize(int width, int height);
int GetMonitorWidth(int monitor);
int GetMonitorHeight(int monitor);
int GetCurrentMonitor(void);
int GetScreenWidth(void);
int GetScreenHeight(void);
}

namespace window_dialog_support {

namespace {

int g_last_windowed_width = 1280;
int g_last_windowed_height = 720;

}

bool is_fullscreen() {
    return IsWindowFullscreen();
}

void toggle_fullscreen() {
    ToggleFullscreen();
}

void minimize_window() {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(GetWindowHandle());
    if (hwnd != nullptr) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
#endif
}

int current_monitor_width() {
    return GetMonitorWidth(GetCurrentMonitor());
}

int current_monitor_height() {
    return GetMonitorHeight(GetCurrentMonitor());
}

void* native_window_handle() {
    return GetWindowHandle();
}

#ifdef _WIN32

namespace {

RECT monitor_work_rect(HWND hwnd) {
    RECT fallback = {0, 0, GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor())};
    if (hwnd == nullptr) {
        return fallback;
    }

    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) {
        return fallback;
    }
    return monitor_info.rcWork;
}

}  // namespace

#endif

void apply_windowed_layout(int client_width, int client_height) {
    const int safe_client_width = std::max(1, client_width);
    const int safe_client_height = std::max(1, client_height);
    g_last_windowed_width = safe_client_width;
    g_last_windowed_height = safe_client_height;

#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(GetWindowHandle());
    const RECT work = monitor_work_rect(hwnd);
    const int work_width = std::max(1L, work.right - work.left);
    const int work_height = std::max(1L, work.bottom - work.top);

    RECT desired_rect = {0, 0, safe_client_width, safe_client_height};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    AdjustWindowRectEx(&desired_rect, style, FALSE, ex_style);

    const int desired_outer_width = desired_rect.right - desired_rect.left;
    const int desired_outer_height = desired_rect.bottom - desired_rect.top;
    const int outer_width = std::min(desired_outer_width, work_width);
    const int outer_height = std::min(desired_outer_height, work_height);
    const int pos_x = work.left + (work_width - outer_width) / 2;
    const int pos_y = work.top + (work_height - outer_height) / 2;

    SetWindowPos(hwnd, nullptr, pos_x, pos_y, outer_width, outer_height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    const int width = std::min(safe_client_width, GetMonitorWidth(GetCurrentMonitor()));
    const int height = std::min(safe_client_height, GetMonitorHeight(GetCurrentMonitor()));
    SetWindowSize(width, height);
#endif
}

void set_fullscreen(bool fullscreen, int windowed_client_width, int windowed_client_height) {
    const int safe_width = std::max(1, windowed_client_width);
    const int safe_height = std::max(1, windowed_client_height);

    if (fullscreen == IsWindowFullscreen()) {
        if (fullscreen) {
            SetWindowSize(safe_width, safe_height);
        } else {
            apply_windowed_layout(g_last_windowed_width, g_last_windowed_height);
        }
        return;
    }

    if (fullscreen) {
        g_last_windowed_width = GetScreenWidth();
        g_last_windowed_height = GetScreenHeight();
    } else if (windowed_client_width > 0 && windowed_client_height > 0) {
        g_last_windowed_width = windowed_client_width;
        g_last_windowed_height = windowed_client_height;
    }

    ToggleFullscreen();

    if (fullscreen) {
        SetWindowSize(safe_width, safe_height);
        return;
    }

#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(GetWindowHandle());
    if (hwnd != nullptr) {
        ShowWindow(hwnd, SW_RESTORE);
    }
#endif
    apply_windowed_layout(g_last_windowed_width, g_last_windowed_height);
}

}  // namespace window_dialog_support
