#include "file_dialog.h"

#include "path_utils.h"

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

std::string open_file_dialog(const wchar_t* filter, const wchar_t* title) {
    wchar_t file_name[MAX_PATH] = {0};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn) != 0) {
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

#else

std::string open_audio_file() { return {}; }
std::string open_image_file() { return {}; }

#endif

}
