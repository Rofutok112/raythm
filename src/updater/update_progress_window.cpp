#include "updater/update_progress_window.h"

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace updater {

struct progress_window::impl {
#ifdef _WIN32
    HWND window = nullptr;
    HWND status_label = nullptr;
    std::wstring class_name = L"raythm_updater_progress_window";
#endif
};

#ifdef _WIN32
namespace {

std::wstring widen(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

LRESULT CALLBACK progress_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CLOSE:
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }
}

}  // namespace
#endif

progress_window::progress_window()
    : impl_(new impl()) {
#ifdef _WIN32
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = progress_window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = impl_->class_name.c_str();
    window_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&window_class);

    impl_->window = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        impl_->class_name.c_str(),
        L"Updating raythm",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 160,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    impl_->status_label = CreateWindowExW(
        0,
        L"STATIC",
        L"Preparing update...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        24, 32, 356, 54,
        impl_->window, nullptr, GetModuleHandleW(nullptr), nullptr);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(impl_->status_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    ShowWindow(impl_->window, SW_SHOWNORMAL);
    UpdateWindow(impl_->window);
#endif
}

progress_window::~progress_window() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->window != nullptr) {
        DestroyWindow(impl_->window);
    }
#endif
    delete impl_;
}

void progress_window::set_title(std::string_view title) {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->window != nullptr) {
        SetWindowTextW(impl_->window, widen(title).c_str());
        UpdateWindow(impl_->window);
    }
#else
    (void)title;
#endif
}

void progress_window::set_status(std::string_view status) {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->status_label != nullptr) {
        SetWindowTextW(impl_->status_label, widen(status).c_str());
        InvalidateRect(impl_->window, nullptr, TRUE);
        UpdateWindow(impl_->window);
    }
    process_events();
#else
    (void)status;
#endif
}

void progress_window::process_events() {
#ifdef _WIN32
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
#endif
}

void progress_window::show_error(std::string_view message) {
#ifdef _WIN32
    process_events();
    MessageBoxW(impl_ != nullptr ? impl_->window : nullptr,
                widen(message).c_str(),
                L"raythm Update Failed",
                MB_OK | MB_ICONERROR);
#else
    (void)message;
#endif
}

}  // namespace updater
