//
// Created by rento on 2026/03/08.
//

#ifndef RAYTHM_AUDIO_H
#define RAYTHM_AUDIO_H

#pragma once
#include <string>

class audio {
public:
    audio();
    ~audio();

    void load(const std::string& file_path);
    void play() const;

private:
    unsigned long handle;
};


#endif //RAYTHM_AUDIO_H