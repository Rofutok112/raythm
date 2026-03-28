//
// Created by rento on 2026/03/08.
//

#include "audio.h"

#include <algorithm>
#include <iostream>

#include "bass.h"

int audio::instance_count_ = 0;

audio::audio() {
    if (instance_count_ == 0 && !BASS_Init(-1, 44100, 0, nullptr, nullptr)) {
        std::cout << "[audio/Error] An error occurred during initialization";
    }
    ++instance_count_;
}

audio::~audio() {
    stop();
    if (handle_ != 0) {
        BASS_StreamFree(handle_);
        handle_ = 0;
    }

    --instance_count_;
    if (instance_count_ == 0) {
        BASS_Free();
    }
}

void audio::load(const std::string& file_path) {
    if (handle_ != 0) {
        BASS_StreamFree(handle_);
        handle_ = 0;
    }

    handle_ = BASS_StreamCreateFile(FALSE, file_path.c_str(), 0, 0, 0);
}

void audio::play(bool restart) const {
    if (handle_ != 0) {
        BASS_ChannelPlay(handle_, restart ? TRUE : FALSE);
    }
}

void audio::pause() const {
    if (handle_ != 0) {
        BASS_ChannelPause(handle_);
    }
}

void audio::stop() const {
    if (handle_ != 0) {
        BASS_ChannelStop(handle_);
    }
}

void audio::set_volume(float volume) const {
    if (handle_ != 0) {
        BASS_ChannelSetAttribute(handle_, BASS_ATTRIB_VOL, std::clamp(volume, 0.0f, 1.0f));
    }
}

void audio::set_position_seconds(double seconds) const {
    if (handle_ != 0) {
        const QWORD position = BASS_ChannelSeconds2Bytes(handle_, std::max(0.0, seconds));
        BASS_ChannelSetPosition(handle_, position, BASS_POS_BYTE);
    }
}

bool audio::is_loaded() const {
    return handle_ != 0;
}

bool audio::is_playing() const {
    return handle_ != 0 && BASS_ChannelIsActive(handle_) == BASS_ACTIVE_PLAYING;
}

double audio::get_position_seconds() const {
    if (handle_ == 0) {
        return 0.0;
    }

    const QWORD position = BASS_ChannelGetPosition(handle_, BASS_POS_BYTE);
    return BASS_ChannelBytes2Seconds(handle_, position);
}

double audio::get_length_seconds() const {
    if (handle_ == 0) {
        return 0.0;
    }

    const QWORD length = BASS_ChannelGetLength(handle_, BASS_POS_BYTE);
    return BASS_ChannelBytes2Seconds(handle_, length);
}
