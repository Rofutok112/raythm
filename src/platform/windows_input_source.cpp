#include <algorithm>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "windows_input_source.h"

namespace {

constexpr int kKeyNull = 0;
constexpr int kKeySpace = 32;
constexpr int kKeyApostrophe = 39;
constexpr int kKeyComma = 44;
constexpr int kKeyMinus = 45;
constexpr int kKeyPeriod = 46;
constexpr int kKeySlash = 47;
constexpr int kKeyLeftBracket = 91;
constexpr int kKeyBackslash = 92;
constexpr int kKeyRightBracket = 93;
constexpr int kKeyGrave = 96;
constexpr int kKeyTab = 258;
constexpr int kKeyRight = 262;
constexpr int kKeyLeft = 263;
constexpr int kKeyDown = 264;
constexpr int kKeyUp = 265;
constexpr int kKeyF1 = 290;
constexpr int kKeyF2 = 291;
constexpr int kKeyF3 = 292;
constexpr int kKeyF4 = 293;
constexpr int kKeyF5 = 294;
constexpr int kKeyF6 = 295;
constexpr int kKeyF7 = 296;
constexpr int kKeyF8 = 297;
constexpr int kKeyF9 = 298;
constexpr int kKeyF10 = 299;
constexpr int kKeyF11 = 300;
constexpr int kKeyF12 = 301;
constexpr int kKeyLeftShift = 340;
constexpr int kKeyLeftControl = 341;
constexpr int kKeyLeftAlt = 342;
constexpr int kKeyRightShift = 344;
constexpr int kKeyRightControl = 345;
constexpr int kKeyRightAlt = 346;

class windows_input_source_state {
public:
    bool initialize(void* native_window_handle) {
        std::scoped_lock lock(mutex_);

#ifdef _WIN32
        if (installed_) {
            return true;
        }

        if (native_window_handle == nullptr) {
            return false;
        }

        LARGE_INTEGER frequency = {};
        LARGE_INTEGER origin = {};
        if (QueryPerformanceFrequency(&frequency) == 0 || QueryPerformanceCounter(&origin) == 0) {
            return false;
        }

        hwnd_ = static_cast<HWND>(native_window_handle);
        SetLastError(0);
        previous_wnd_proc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windows_input_source_state::wnd_proc)));
        if (previous_wnd_proc_ == nullptr && GetLastError() != 0) {
            hwnd_ = nullptr;
            return false;
        }

        installed_ = true;
        qpc_frequency_ = frequency.QuadPart;
        qpc_origin_ = origin.QuadPart;
        queued_events_.clear();
        sequence_ = 0;
        ime_context_enabled_ = true;
        ime_requested_this_frame_ = false;
        saved_ime_context_ = nullptr;
#else
        (void)native_window_handle;
#endif

        return true;
    }

    void shutdown() {
        set_ime_context_enabled(true);

        std::scoped_lock lock(mutex_);

#ifdef _WIN32
        if (installed_ && hwnd_ != nullptr) {
            SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous_wnd_proc_));
        }

        installed_ = false;
        hwnd_ = nullptr;
        previous_wnd_proc_ = nullptr;
#endif

        queued_events_.clear();
        test_mode_ = false;
        ime_context_enabled_ = true;
        ime_requested_this_frame_ = false;
    }

    bool is_available() const {
        std::scoped_lock lock(mutex_);
        return installed_ || test_mode_;
    }

    double current_time_ms() const {
        std::scoped_lock lock(mutex_);

#ifdef _WIN32
        if (installed_ && qpc_frequency_ != 0) {
            LARGE_INTEGER counter = {};
            if (QueryPerformanceCounter(&counter) != 0) {
                return static_cast<double>(counter.QuadPart - qpc_origin_) * 1000.0 / static_cast<double>(qpc_frequency_);
            }
        }
#endif

        return test_current_time_ms_;
    }

    void begin_frame() {
        std::scoped_lock lock(mutex_);
        ime_requested_this_frame_ = false;
    }

    void request_text_input() {
        bool should_enable_ime = false;
        {
            std::scoped_lock lock(mutex_);
            ime_requested_this_frame_ = true;
            should_enable_ime = !ime_context_enabled_;
        }

        if (should_enable_ime && set_ime_context_enabled(true)) {
            std::scoped_lock lock(mutex_);
            ime_context_enabled_ = true;
        }
    }

    void end_frame() {
        bool should_disable_ime = false;
        {
            std::scoped_lock lock(mutex_);
            const bool allow_text_input = ime_requested_this_frame_;
            ime_requested_this_frame_ = false;
            should_disable_ime = !allow_text_input && ime_context_enabled_;
        }

        if (should_disable_ime && set_ime_context_enabled(false)) {
            std::scoped_lock lock(mutex_);
            ime_context_enabled_ = false;
        }
    }

    std::vector<native_key_event> drain_events() {
        std::scoped_lock lock(mutex_);
        std::vector<native_key_event> drained;
        drained.reserve(queued_events_.size());
        while (!queued_events_.empty()) {
            drained.push_back(queued_events_.front());
            queued_events_.pop_front();
        }
        return drained;
    }

    void enable_test_mode() {
        std::scoped_lock lock(mutex_);
        queued_events_.clear();
        test_mode_ = true;
        sequence_ = 0;
        test_current_time_ms_ = 0.0;
    }

    void set_test_current_time_ms(double current_time_ms) {
        std::scoped_lock lock(mutex_);
        test_mode_ = true;
        test_current_time_ms_ = current_time_ms;
    }

    void push_test_event(native_key_event event) {
        std::scoped_lock lock(mutex_);
        test_mode_ = true;
        if (event.sequence == 0) {
            event.sequence = ++sequence_;
        }
        queued_events_.push_back(event);
    }

#ifdef _WIN32
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
        windows_input_source_state& state = instance();
        if (state.try_capture_event(message, w_param, l_param)) {
            return CallWindowProcW(state.previous_wnd_proc_, hwnd, message, w_param, l_param);
        }

        return CallWindowProcW(state.previous_wnd_proc_, hwnd, message, w_param, l_param);
    }
#endif

    static windows_input_source_state& instance() {
        static windows_input_source_state state;
        return state;
    }

private:
#ifdef _WIN32
    using imm_associate_context_fn = HIMC(WINAPI*)(HWND, HIMC);

    struct ime_functions {
        HMODULE module = nullptr;
        imm_associate_context_fn associate_context = nullptr;
        bool available = false;
    };

    static const ime_functions& loaded_ime_functions() {
        static const ime_functions functions = [] {
            ime_functions loaded;
            loaded.module = LoadLibraryW(L"imm32.dll");
            if (loaded.module == nullptr) {
                return loaded;
            }

            loaded.associate_context =
                reinterpret_cast<imm_associate_context_fn>(GetProcAddress(loaded.module, "ImmAssociateContext"));
            loaded.available = loaded.associate_context != nullptr;
            return loaded;
        }();
        return functions;
    }

    bool set_ime_context_enabled(bool enabled) {
        HWND hwnd = nullptr;
        HIMC restore_context = nullptr;
        {
            std::scoped_lock lock(mutex_);
            if (!installed_ || hwnd_ == nullptr) {
                return false;
            }
            hwnd = hwnd_;
            restore_context = saved_ime_context_;
        }

        // IME APIs may dispatch window messages. Never hold mutex_ while calling them.
        const ime_functions& functions = loaded_ime_functions();
        if (!functions.available) {
            return false;
        }

        if (enabled) {
            if (restore_context == nullptr) {
                return false;
            }

            HIMC previous_context = functions.associate_context(hwnd, restore_context);
            {
                std::scoped_lock lock(mutex_);
                if (previous_context != nullptr && previous_context != restore_context) {
                    saved_ime_context_ = previous_context;
                }
            }
            return true;
        }

        HIMC previous_context = functions.associate_context(hwnd, nullptr);
        if (previous_context != nullptr) {
            std::scoped_lock lock(mutex_);
            saved_ime_context_ = previous_context;
        }
        return true;
    }

    bool try_capture_event(UINT message, WPARAM w_param, LPARAM l_param) {
        std::scoped_lock lock(mutex_);
        if (!installed_) {
            return false;
        }

        if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN &&
            message != WM_KEYUP && message != WM_SYSKEYUP) {
            return false;
        }

        const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (pressed && (l_param & (1LL << 30)) != 0) {
            return false;
        }

        const int key = translate_key(w_param, l_param);
        if (key == kKeyNull) {
            return false;
        }

        LARGE_INTEGER counter = {};
        if (QueryPerformanceCounter(&counter) == 0 || qpc_frequency_ == 0) {
            return false;
        }

        native_key_event event;
        event.key = key;
        event.type = pressed ? input_event_type::press : input_event_type::release;
        event.timestamp_ms =
            static_cast<double>(counter.QuadPart - qpc_origin_) * 1000.0 / static_cast<double>(qpc_frequency_);
        event.sequence = ++sequence_;
        queued_events_.push_back(event);
        return true;
    }

    static int translate_key(WPARAM w_param, LPARAM l_param) {
        UINT virtual_key = static_cast<UINT>(w_param);
        const UINT scan_code = (static_cast<UINT>(l_param) >> 16) & 0xff;
        const bool extended = (static_cast<UINT>(l_param) & 0x01000000U) != 0;

        if (virtual_key == VK_SHIFT) {
            virtual_key = MapVirtualKeyW(scan_code, MAPVK_VSC_TO_VK_EX);
        } else if (virtual_key == VK_CONTROL) {
            virtual_key = extended ? VK_RCONTROL : VK_LCONTROL;
        } else if (virtual_key == VK_MENU) {
            virtual_key = extended ? VK_RMENU : VK_LMENU;
        }

        if ((virtual_key >= '0' && virtual_key <= '9') || (virtual_key >= 'A' && virtual_key <= 'Z')) {
            return static_cast<int>(virtual_key);
        }

        switch (virtual_key) {
            case VK_SPACE: return kKeySpace;
            case VK_TAB: return kKeyTab;
            case VK_LEFT: return kKeyLeft;
            case VK_RIGHT: return kKeyRight;
            case VK_UP: return kKeyUp;
            case VK_DOWN: return kKeyDown;
            case VK_OEM_COMMA: return kKeyComma;
            case VK_OEM_PERIOD: return kKeyPeriod;
            case VK_OEM_2: return kKeySlash;
            case VK_OEM_1: return 59;
            case VK_OEM_7: return kKeyApostrophe;
            case VK_OEM_4: return kKeyLeftBracket;
            case VK_OEM_6: return kKeyRightBracket;
            case VK_OEM_5: return kKeyBackslash;
            case VK_OEM_MINUS: return kKeyMinus;
            case VK_OEM_PLUS: return 61;
            case VK_OEM_3: return kKeyGrave;
            case VK_F1: return kKeyF1;
            case VK_F2: return kKeyF2;
            case VK_F3: return kKeyF3;
            case VK_F4: return kKeyF4;
            case VK_F5: return kKeyF5;
            case VK_F6: return kKeyF6;
            case VK_F7: return kKeyF7;
            case VK_F8: return kKeyF8;
            case VK_F9: return kKeyF9;
            case VK_F10: return kKeyF10;
            case VK_F11: return kKeyF11;
            case VK_F12: return kKeyF12;
            case VK_LSHIFT: return kKeyLeftShift;
            case VK_RSHIFT: return kKeyRightShift;
            case VK_LCONTROL: return kKeyLeftControl;
            case VK_RCONTROL: return kKeyRightControl;
            case VK_LMENU: return kKeyLeftAlt;
            case VK_RMENU: return kKeyRightAlt;
            default: return kKeyNull;
        }
    }

    HWND hwnd_ = nullptr;
    WNDPROC previous_wnd_proc_ = nullptr;
    long long qpc_frequency_ = 0;
    long long qpc_origin_ = 0;
    HIMC saved_ime_context_ = nullptr;
#else
    bool set_ime_context_enabled(bool) {
        return false;
    }
#endif

    mutable std::mutex mutex_;
    std::deque<native_key_event> queued_events_;
    std::uint64_t sequence_ = 0;
    bool test_mode_ = false;
    bool installed_ = false;
    bool ime_context_enabled_ = true;
    bool ime_requested_this_frame_ = false;
    double test_current_time_ms_ = 0.0;
};

}  // namespace

windows_input_source& windows_input_source::instance() {
    static windows_input_source source;
    return source;
}

bool windows_input_source::initialize(void* native_window_handle) {
    return windows_input_source_state::instance().initialize(native_window_handle);
}

void windows_input_source::shutdown() {
    windows_input_source_state::instance().shutdown();
}

bool windows_input_source::is_available() const {
    return windows_input_source_state::instance().is_available();
}

double windows_input_source::current_time_ms() const {
    return windows_input_source_state::instance().current_time_ms();
}

void windows_input_source::begin_frame() {
    windows_input_source_state::instance().begin_frame();
}

void windows_input_source::request_text_input() {
    windows_input_source_state::instance().request_text_input();
}

void windows_input_source::end_frame() {
    windows_input_source_state::instance().end_frame();
}

std::vector<native_key_event> windows_input_source::drain_events() {
    return windows_input_source_state::instance().drain_events();
}

void windows_input_source::enable_test_mode() {
    windows_input_source_state::instance().enable_test_mode();
}

void windows_input_source::set_test_current_time_ms(double current_time_ms) {
    windows_input_source_state::instance().set_test_current_time_ms(current_time_ms);
}

void windows_input_source::push_test_event(native_key_event event) {
    windows_input_source_state::instance().push_test_event(std::move(event));
}
