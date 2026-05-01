#include "updater/update_progress_window.h"

#include <algorithm>
#include <cmath>
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

#ifdef _WIN32
struct progress_window_state {
    HWND window = nullptr;
    std::wstring class_name = L"raythm_updater_progress_window";
    std::wstring title = L"Updating raythm";
    std::wstring status = L"Preparing update...";
    float progress = 0.0f;
    HFONT title_font = nullptr;
    HFONT status_font = nullptr;
    HFONT detail_font = nullptr;
};
#endif

struct progress_window::impl
#ifdef _WIN32
    : progress_window_state
#endif
{
#ifndef _WIN32
    int unused = 0;
#endif
};

#ifdef _WIN32
namespace {

struct progress_window_palette {
    COLORREF background;
    COLORREF header;
    COLORREF accent;
    COLORREF accent_soft;
    COLORREF accent_text;
    COLORREF accent_shine;
    COLORREF card;
    COLORREF card_border;
    COLORREF text_primary;
    COLORREF text_secondary;
    COLORREF text_status;
    COLORREF text_muted;
    COLORREF track;
    COLORREF track_border;
};

constexpr int kWindowWidth = 520;
constexpr int kWindowHeight = 300;
constexpr int kCardInset = 22;
constexpr int kContentLeft = 38;
constexpr int kContentRightInset = 38;
constexpr int kHeaderHeight = 78;
constexpr int kGlowHeight = 3;
constexpr int kCardRadius = 18;
constexpr int kBarRadius = 16;

std::wstring widen(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

COLORREF rgb(int red, int green, int blue) {
    return RGB(red, green, blue);
}

const progress_window_palette& palette() {
    static const progress_window_palette colors{
        rgb(14, 16, 20),
        rgb(20, 24, 30),
        rgb(158, 100, 255),
        rgb(82, 46, 150),
        rgb(204, 176, 255),
        rgb(220, 200, 255),
        rgb(24, 28, 36),
        rgb(48, 58, 68),
        rgb(232, 238, 245),
        rgb(156, 168, 182),
        rgb(214, 222, 232),
        rgb(116, 128, 140),
        rgb(38, 46, 52),
        rgb(62, 74, 82),
    };
    return colors;
}

void fill_round_rect(HDC dc, const RECT& rect, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void stroke_round_rect(HDC dc, const RECT& rect, int radius, COLORREF color) {
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
}

void draw_text(HDC dc, HFONT font, COLORREF color, const std::wstring& text, RECT rect, UINT format) {
    SetTextColor(dc, color);
    HGDIOBJ old_font = SelectObject(dc, font);
    DrawTextW(dc, text.c_str(), -1, &rect, format);
    SelectObject(dc, old_font);
}

void draw_progress_bar(HDC dc, const RECT& track_rect, float progress, const progress_window_palette& colors) {
    fill_round_rect(dc, track_rect, kBarRadius, colors.track);
    stroke_round_rect(dc, track_rect, kBarRadius, colors.track_border);

    RECT fill_rect = track_rect;
    fill_rect.right = fill_rect.left + static_cast<int>((track_rect.right - track_rect.left) * progress);
    if (fill_rect.right <= fill_rect.left) {
        return;
    }

    fill_round_rect(dc, fill_rect, kBarRadius, colors.accent);
    RECT shine_rect{fill_rect.left + 4, fill_rect.top + 3, std::max(fill_rect.left + 4, fill_rect.right - 4),
                    fill_rect.top + 7};
    if (shine_rect.right > shine_rect.left) {
        fill_round_rect(dc, shine_rect, 8, colors.accent_shine);
    }
}

void paint_progress_window(progress_window_state& state, HDC dc, const RECT& client_rect) {
    const progress_window_palette& colors = palette();

    fill_round_rect(dc, client_rect, 0, colors.background);

    RECT header_rect{0, 0, client_rect.right, kHeaderHeight};
    fill_round_rect(dc, header_rect, 0, colors.header);

    RECT glow_rect{0, 0, client_rect.right, kGlowHeight};
    fill_round_rect(dc, glow_rect, 0, colors.accent);

    RECT card_rect{kCardInset, kCardInset, client_rect.right - kCardInset, client_rect.bottom - kCardInset};
    fill_round_rect(dc, card_rect, kCardRadius, colors.card);
    stroke_round_rect(dc, card_rect, kCardRadius, colors.card_border);

    RECT pulse_outer{kContentLeft, 40, 66, 68};
    fill_round_rect(dc, pulse_outer, 28, colors.accent_soft);
    RECT pulse_inner{45, 47, 59, 61};
    fill_round_rect(dc, pulse_inner, 14, colors.accent_text);

    SetBkMode(dc, TRANSPARENT);
    RECT title_rect{78, 32, client_rect.right - 34, 58};
    draw_text(dc, state.title_font, colors.text_primary, state.title, title_rect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    RECT detail_rect{78, 56, client_rect.right - 34, 80};
    draw_text(dc, state.detail_font, colors.text_secondary, L"Keep this window open while raythm updates.",
              detail_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    RECT status_rect{kContentLeft, 100, client_rect.right - kContentRightInset, 134};
    draw_text(dc, state.status_font, colors.text_status, state.status, status_rect,
              DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    const float clamped_progress = std::clamp(state.progress, 0.0f, 1.0f);
    const int percent = static_cast<int>(std::round(clamped_progress * 100.0f));

    RECT percent_rect{client_rect.right - 92, 150, client_rect.right - kContentRightInset, 174};
    std::wstring percent_text = std::to_wstring(percent) + L"%";
    draw_text(dc, state.status_font, colors.accent_text, percent_text, percent_rect,
              DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    RECT track_rect{kContentLeft, 178, client_rect.right - kContentRightInset, 194};
    draw_progress_bar(dc, track_rect, clamped_progress, colors);

    RECT footer_rect{kContentLeft, 214, client_rect.right - kContentRightInset, 236};
    draw_text(dc, state.detail_font, colors.text_muted, L"raythm updater", footer_rect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
}

LRESULT CALLBACK progress_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
    }

    auto* state = reinterpret_cast<progress_window_state*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state != nullptr) {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client_rect{};
            GetClientRect(window, &client_rect);
            paint_progress_window(*state, dc, client_rect);
            EndPaint(window, &paint);
            return 0;
        }
        break;
    case WM_CLOSE:
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }

    return DefWindowProcW(window, message, w_param, l_param);
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
    RegisterClassW(&window_class);

    impl_->title_font = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    impl_->status_font = CreateFontW(18, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    impl_->detail_font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    impl_->window = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        impl_->class_name.c_str(),
        L"Updating raythm",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
        nullptr, nullptr, GetModuleHandleW(nullptr), impl_);

    ShowWindow(impl_->window, SW_SHOWNORMAL);
    UpdateWindow(impl_->window);
#endif
}

progress_window::~progress_window() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->window != nullptr) {
        DestroyWindow(impl_->window);
    }
    if (impl_ != nullptr && impl_->title_font != nullptr) {
        DeleteObject(impl_->title_font);
    }
    if (impl_ != nullptr && impl_->status_font != nullptr) {
        DeleteObject(impl_->status_font);
    }
    if (impl_ != nullptr && impl_->detail_font != nullptr) {
        DeleteObject(impl_->detail_font);
    }
#endif
    delete impl_;
}

void progress_window::set_title(std::string_view title) {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->window != nullptr) {
        impl_->title = widen(title);
        SetWindowTextW(impl_->window, widen(title).c_str());
        InvalidateRect(impl_->window, nullptr, FALSE);
        UpdateWindow(impl_->window);
    }
#else
    (void)title;
#endif
}

void progress_window::set_status(std::string_view status) {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->window != nullptr) {
        impl_->status = widen(status);
        InvalidateRect(impl_->window, nullptr, FALSE);
        UpdateWindow(impl_->window);
    }
    process_events();
#else
    (void)status;
#endif
}

void progress_window::set_progress(float progress) {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->window != nullptr) {
        impl_->progress = std::clamp(progress, 0.0f, 1.0f);
        InvalidateRect(impl_->window, nullptr, FALSE);
        UpdateWindow(impl_->window);
    }
    process_events();
#else
    (void)progress;
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
