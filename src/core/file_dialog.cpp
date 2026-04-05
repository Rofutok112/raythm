#include "file_dialog.h"

#include <cwchar>

#include "path_utils.h"
#include "window_dialog_support.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <string>
#endif

namespace file_dialog {

#ifdef _WIN32

namespace {

class fullscreen_dialog_guard {
public:
    fullscreen_dialog_guard() : was_fullscreen_(window_dialog_support::is_fullscreen()) {
        if (was_fullscreen_) {
            window_dialog_support::toggle_fullscreen();
        }
    }

    ~fullscreen_dialog_guard() {
        if (was_fullscreen_) {
            window_dialog_support::toggle_fullscreen();
        }
    }

private:
    bool was_fullscreen_ = false;
};

std::wstring utf8_to_wstring(const std::string& utf8) {
    return path_utils::from_utf8(utf8).wstring();
}

std::string open_file_dialog(const wchar_t* filter, const wchar_t* title) {
    fullscreen_dialog_guard fullscreen_guard;
    wchar_t file_name[MAX_PATH] = {0};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(window_dialog_support::native_window_handle());
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (ofn.hwndOwner != nullptr) {
        SetForegroundWindow(ofn.hwndOwner);
    }

    if (GetOpenFileNameW(&ofn) != 0) {
        return path_utils::to_utf8(std::filesystem::path(file_name));
    }

    return {};
}

std::string save_file_dialog(const wchar_t* filter, const wchar_t* title,
                             const wchar_t* default_extension,
                             const std::string& default_file_name) {
    fullscreen_dialog_guard fullscreen_guard;
    wchar_t file_name[MAX_PATH] = {0};
    const std::wstring initial_name = utf8_to_wstring(default_file_name);
    std::wcsncpy(file_name, initial_name.c_str(), MAX_PATH - 1);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(window_dialog_support::native_window_handle());
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = default_extension;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

    if (ofn.hwndOwner != nullptr) {
        SetForegroundWindow(ofn.hwndOwner);
    }

    if (GetSaveFileNameW(&ofn) != 0) {
        return path_utils::to_utf8(std::filesystem::path(file_name));
    }

    return {};
}

}

std::string open_audio_file() {
    return open_file_dialog(
        L"Audio Files (*.mp3;*.ogg;*.wav)\0*.mp3;*.ogg;*.wav\0All Files (*.*)\0*.*\0",
        L"Select Audio File");
}

std::string open_image_file() {
    return open_file_dialog(
        L"Image Files (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0",
        L"Select Image File");
}

std::string open_chart_package_file() {
    return open_file_dialog(
        L"raythm Chart Package (*.rchart)\0*.rchart\0All Files (*.*)\0*.*\0",
        L"Import Chart Package");
}

std::string open_song_package_file() {
    return open_file_dialog(
        L"raythm Song Package (*.rpack)\0*.rpack\0All Files (*.*)\0*.*\0",
        L"Import Song Package");
}

std::string save_chart_package_file(const std::string& default_file_name) {
    return save_file_dialog(
        L"raythm Chart Package (*.rchart)\0*.rchart\0All Files (*.*)\0*.*\0",
        L"Export Chart Package",
        L"rchart",
        default_file_name);
}

std::string save_song_package_file(const std::string& default_file_name) {
    return save_file_dialog(
        L"raythm Song Package (*.rpack)\0*.rpack\0All Files (*.*)\0*.*\0",
        L"Export Song Package",
        L"rpack",
        default_file_name);
}

bool confirm_yes_no(const std::string& title, const std::string& message) {
    fullscreen_dialog_guard fullscreen_guard;
    const int result = MessageBoxW(static_cast<HWND>(window_dialog_support::native_window_handle()),
                                   utf8_to_wstring(message).c_str(),
                                   utf8_to_wstring(title).c_str(),
                                   MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    return result == IDYES;
}

#else

std::string open_audio_file() { return {}; }
std::string open_image_file() { return {}; }
std::string open_chart_package_file() { return {}; }
std::string open_song_package_file() { return {}; }
std::string save_chart_package_file(const std::string&) { return {}; }
std::string save_song_package_file(const std::string&) { return {}; }
bool confirm_yes_no(const std::string&, const std::string&) { return false; }

#endif

}
