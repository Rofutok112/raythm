//
// Created by rento on 2026/03/08.
//

#include "audio.h"
#include "bass.h"
#include "iostream"

audio::audio() {
    if (!BASS_Init(-1, 44100, 0, nullptr, nullptr)) {
        std::cout << "[audio/Error] An error occurred during initialization";
        return;
    }

    handle = 0;
}

audio::~audio() {
    BASS_Free();
}

void audio::load(const std::string &file_path) {
    handle = BASS_StreamCreateFile(FALSE ,file_path.c_str(), 0, 0, 0);
}

void audio::play() const {
    BASS_ChannelPlay(handle, false);
}
