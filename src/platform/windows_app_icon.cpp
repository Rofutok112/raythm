#include "windows_app_icon.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void apply_windows_app_icon(void* native_window_handle) {
#ifdef _WIN32
    if (native_window_handle == nullptr) {
        return;
    }

    HWND window = static_cast<HWND>(native_window_handle);
    HINSTANCE instance = GetModuleHandleW(nullptr);

    const int small_icon_size = GetSystemMetrics(SM_CXSMICON);
    const int large_icon_size = GetSystemMetrics(SM_CXICON);

    HICON large_icon = static_cast<HICON>(LoadImageW(
        instance, L"IDI_APP_ICON", IMAGE_ICON, large_icon_size, large_icon_size, 0));
    HICON small_icon = static_cast<HICON>(LoadImageW(
        instance, L"IDI_APP_ICON", IMAGE_ICON, small_icon_size, small_icon_size, 0));

    if (large_icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
    }
    if (small_icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }
#else
    (void)native_window_handle;
#endif
}
