#pragma once

#include <string>
#include <vector>

namespace file_dialog {

// Open a file dialog for selecting an audio file. Returns empty string if cancelled.
std::string open_audio_file();

// Open a file dialog for selecting an image file. Returns empty string if cancelled.
std::string open_image_file();

// Open a file dialog for selecting an .rchart file. Returns empty string if cancelled.
std::string open_chart_package_file();

// Open a file dialog for selecting one or more .rchart files. Returns empty vector if cancelled.
std::vector<std::string> open_chart_package_files();

// Open a file dialog for selecting an .rpack file. Returns empty string if cancelled.
std::string open_song_package_file();

// Open a file dialog for selecting one or more .rpack files. Returns empty vector if cancelled.
std::vector<std::string> open_song_package_files();

// Open a file dialog for selecting an .rmvcomp file. Returns empty string if cancelled.
std::string open_mv_composition_file();

// Open a file dialog for selecting an .rmvpack file. Returns empty string if cancelled.
std::string open_mv_package_file();

// Open a file dialog for selecting a MIDI file. Returns empty string if cancelled.
std::string open_midi_file();

// Open a save file dialog for saving an .rchart file. Returns empty string if cancelled.
std::string save_chart_package_file(const std::string& default_file_name);

// Open a save file dialog for saving an .rpack file. Returns empty string if cancelled.
std::string save_song_package_file(const std::string& default_file_name);

// Open a save file dialog for saving an .rmvcomp file. Returns empty string if cancelled.
std::string save_mv_composition_file(const std::string& default_file_name);

// Open a save file dialog for saving an .rmvpack file. Returns empty string if cancelled.
std::string save_mv_package_file(const std::string& default_file_name);

// Show a yes/no confirmation dialog. Returns true when the user accepts.
bool confirm_yes_no(const std::string& title, const std::string& message);

}
