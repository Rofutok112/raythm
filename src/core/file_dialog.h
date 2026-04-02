#pragma once

#include <string>

namespace file_dialog {

// Open a file dialog for selecting an audio file. Returns empty string if cancelled.
std::string open_audio_file();

// Open a file dialog for selecting an image file. Returns empty string if cancelled.
std::string open_image_file();

}
